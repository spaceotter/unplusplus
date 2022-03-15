/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "filter.hpp"

#include <clang/AST/DeclTemplate.h>
#include <clang/Basic/SourceManager.h>

#include <fstream>
#include <iostream>

#include "identifier.hpp"

using namespace unplusplus;
using namespace clang;

static const std::vector<std::string> C_STD_HEADERS = {
    "assert.h", "complex.h",  "ctype.h",  "errno.h",     "fenv.h",        "float.h",   "inttypes.h",
    "iso646.h", "limits.h",   "locale.h", "malloc.h",    "math.h",        "memory.h",  "setjmp.h",
    "signal.h", "stdalign.h", "stdarg.h", "stdatomic.h", "stdbool.h",     "stddef.h",  "stdint.h",
    "stdio.h",  "stdio_s.h",  "stdlib.h", "stdlib_s.h",  "stdnoreturn.h", "string.h",  "string_s.h",
    "tgmath.h", "threads.h",  "time.h",   "uchar.h",     "wchar.h",       "wchar_s.h", "wctype.h"};

static bool isInaccessibleP(const Decl *D) {
  if (D)
    return (D->getAccess() == AccessSpecifier::AS_protected ||
            D->getAccess() == AccessSpecifier::AS_private);
  else
    return false;
}

static bool isLibraryInternalP(const clang::Decl *D) {
  if (!D) return false;

  if (const auto *ND = dyn_cast_or_null<NamedDecl>(D)) {
    std::string name = getName(ND);
    if (name == "__gnu_cxx" || name == "__cxxabiv1") return true;

    const DeclContext *Ctx = ND->getDeclContext();
    if (const auto *NCtx = dyn_cast_or_null<NamedDecl>(Ctx))
      if (name[0] == '_' && getName(NCtx) == "std") return true;
  }

  return false;
}

bool unplusplus::isInaccessible(const Decl *D) { return traverse(D, isInaccessibleP); }

bool unplusplus::isLibraryInternal(const Decl *D) { return traverse(D, isLibraryInternalP); }

bool unplusplus::traverse(const ArrayRef<clang::TemplateArgument> &d,
                          std::function<bool(const clang::Decl *)> Predicate) {
  for (const auto &Arg : d) {
    switch (Arg.getKind()) {
      case TemplateArgument::Type:
        if (traverse(Arg.getAsType(), Predicate)) return true;
        break;
      case TemplateArgument::Declaration:
        if (traverse(Arg.getAsDecl(), Predicate)) return true;
        break;
      case TemplateArgument::Pack:
        return traverse(Arg.pack_elements(), Predicate);
        break;
      default:
        break;
    }
  }
  return false;
}

bool unplusplus::traverse(const clang::Type *T,
                          std::function<bool(const clang::Decl *)> Predicate) {
  if (const auto *ST = dyn_cast<TagType>(T)) {
    if (traverse(ST->getDecl(), Predicate)) return true;
  } else if (const auto *ST = dyn_cast<PointerType>(T)) {
    return traverse(ST->getPointeeType(), Predicate);
  } else if (const auto *ST = dyn_cast<ReferenceType>(T)) {
    return traverse(ST->getPointeeType(), Predicate);
  } else if (const auto *ST = dyn_cast<ConstantArrayType>(T)) {
    return traverse(ST->getElementType(), Predicate);
  } else if (const auto *ST = dyn_cast<FunctionProtoType>(T)) {
    for (size_t i = 0; i < ST->getNumParams(); i++) {
      if (traverse(ST->getParamType(i), Predicate)) return true;
    }
    return traverse(ST->getReturnType(), Predicate);
  } else if (const auto *ST = dyn_cast<InjectedClassNameType>(T)) {
    return traverse(ST->getDecl(), Predicate);
  }
  return false;
}

bool unplusplus::traverse(QualType QT, std::function<bool(const clang::Decl *)> Predicate) {
  if (QT->isPointerType()) {
    return traverse(QT->getPointeeType(), Predicate);
  } else if (QT->isReferenceType()) {
    return traverse(QT->getPointeeType(), Predicate);
  } else {
    return traverse(QT->getUnqualifiedDesugaredType(), Predicate);
  }
}

bool unplusplus::traverse(const Decl *D, std::function<bool(const clang::Decl *)> Predicate) {
  if (Predicate(D)) return true;

  const TemplateArgumentList *L = nullptr;
  if (const auto *CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D)) {
    if (traverse(CTSD->getSpecializedTemplate(), Predicate)) return true;
    L = &CTSD->getTemplateArgs();
  } else if (const auto *FD = dyn_cast<FunctionDecl>(D)) {
    if (traverse(FD->getReturnType(), Predicate)) return true;
    for (const auto *PD : FD->parameters()) {
      if (traverse(PD->getType(), Predicate)) return true;
    }
    L = FD->getTemplateSpecializationArgs();
  }
  if (L != nullptr) {
    if (traverse(L->asArray(), Predicate)) return true;
  }

  if (const auto *DC = dyn_cast_or_null<Decl>(D->getDeclContext())) {
    return traverse(DC, Predicate);
  } else {
    return false;
  }
}

static const SrcMgr::SLocEntry *getSEntry(const clang::Decl *D) {
  SourceLocation loc = D->getLocation();
  while (true) {
    SourceManager &SM = D->getASTContext().getSourceManager();
    FileID FID = SM.getFileID(SM.getFileLoc(loc));
    bool Invalid = false;
    const SrcMgr::SLocEntry *SEntry = &SM.getSLocEntry(FID, &Invalid);
    if (Invalid) return nullptr;
    if (SEntry->isFile()) {
      return SEntry;
    } else if (SEntry->isExpansion()) {
      SourceLocation loc2 = SEntry->getExpansion().getExpansionLocStart();
      if (loc2 != loc) {
        loc = loc2;
      } else {
        return nullptr;
      }
    } else {
      return nullptr;
    }
  }
}

std::string unplusplus::getDeclHeader(const clang::Decl *D) {
  auto *SEntry = getSEntry(D);
  if (SEntry)
    return SEntry->getFile().getName().str();
  else
    throw std::runtime_error("Can't find source entry");
}

DeclFilter::DeclFilter(const clang::LangOptions &LO, DeclFilterConfig &FC) : _conf(FC), _pp(LO) {
  _pp.PrintCanonicalTypes = 1;
  if (!_conf.exclusion_file.empty()) {
    std::ifstream ifs(_conf.exclusion_file);
    std::string line;
    while (std::getline(ifs, line)) {
      if (line.size() && line[0] != '#') _excluded.emplace(line);
    }
  }
  for (auto &d : _conf.exclude_decls) {
    _excluded.emplace(d);
  }
  _excluded.emplace("__va_list_tag");

  for (const auto &h : C_STD_HEADERS) {
    _headerPatterns.push_back(h);
  }

  for (auto &p : _conf.cheader_files) {
    std::ifstream ifs(p);
    std::string line;
    while (std::getline(ifs, line)) {
      if (line.size() && line[0] != '#') _headerPatterns.push_back(line);
    }
  }
}

bool DeclFilter::predicate(const clang::Decl *D) {
  std::string name = getCXXQualifiedName(_pp, D);
  AvailabilityResult ar = D->getAvailability();
  return (isInaccessibleP(D) || isLibraryInternalP(D) || _excluded.count(name) ||
          (_conf.no_deprecated && ar == AR_Deprecated) || ar == AR_Unavailable ||
          ar == AR_NotYetIntroduced) &&
         !Identifier::ids.count(dyn_cast_or_null<NamedDecl>(D));
}

bool DeclFilter::filterOut(const clang::Decl *D) {
  if (!_cache.count(D)) {
    _cache[D] = traverse(D, std::bind(&DeclFilter::predicate, this, std::placeholders::_1));
  }
  return _cache[D];
}

void DeclFilter::sanitizeType(QualType &QT, const ASTContext &AC) {
  if (QT->isRecordType()) {
    if (!QT->getAsRecordDecl()->isCompleteDefinition() || filterOut(QT->getAsRecordDecl())) {
      uint64_t size = AC.getTypeSizeInChars(QT).getQuantity();
      QT = AC.getConstantArrayType(AC.CharTy, llvm::APInt(AC.getTypeSize(AC.getSizeType()), size),
                                   nullptr, ArrayType::Normal, 0);
    }
  } else if (QT->isReferenceType()) {
    QT = AC.getPointerType(QT.getNonReferenceType());
    sanitizeType(QT, AC);
  } else if (QT->isPointerType()) {
    QualType pointee = QT->getPointeeType();
    if (pointee->isRecordType()) {
      if (filterOut(pointee->getAsRecordDecl())) QT = AC.VoidPtrTy;
    } else {
      sanitizeType(pointee, AC);
      QT = AC.getPointerType(pointee);
    }
  }
}

bool DeclFilter::matchHeader(std::string &S) {
  for (auto &p : _headerPatterns) {
    if (S.find(p) == S.size() - p.size()) {
      return true;
    }
  }
  return false;
}

bool DeclFilter::isCHeader(const clang::Decl *D) {
  if (D == nullptr) return false;
  if (_cheaderd.count(D)) {
    return _cheaderd.at(D);
  }
  std::string qname = getCXXQualifiedName(_pp, D);

  auto *SEntry = getSEntry(D);
  if (!SEntry)
    return false;

  std::string header = SEntry->getFile().getName().str();
  if (_cheader.count(header)) {
    _cheaderd[D] = _cheader.at(header);
    return _cheader.at(header);
  }
  SrcMgr::CharacteristicKind ck = SEntry->getFile().getFileCharacteristic();
  if (ck == SrcMgr::CharacteristicKind::C_ExternCSystem) {
    _cheaderd[D] = true;
    _cheader[header] = true;
    return true;
  }

  bool match = matchHeader(header);
  _cheaderd[D] = match;
  _cheader[header] = match;
  return match;
}
