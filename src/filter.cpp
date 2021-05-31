/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "filter.hpp"

#include <clang/AST/DeclTemplate.h>

#include <fstream>

#include "identifier.hpp"

using namespace unplusplus;
using namespace clang;

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

DeclFilter::DeclFilter(const clang::PrintingPolicy &PP, DeclFilterConfig &FC) : _conf(FC), _pp(PP) {
  if (!_conf.exclusion_file.empty()) {
    std::ifstream ifs(_conf.exclusion_file);
    std::string line;
    while (std::getline(ifs, line)) {
      if (line.size() && line[0] != '#') _excluded.emplace(line);
    }
  }
  for (auto &d : FC.exclude_decls) {
    _excluded.emplace(d);
  }
}

bool DeclFilter::predicate(const clang::Decl *D) {
  std::string name = getCXXQualifiedName(_pp, D);
  return (isInaccessibleP(D) || isLibraryInternalP(D) || _excluded.count(name)) &&
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
