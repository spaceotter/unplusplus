/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "identifier.hpp"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/TemplateBase.h>
#include <clang/Basic/SourceManager.h>

#include <iostream>
#include <sstream>
#include <unordered_map>

#include "filter.hpp"

using namespace unplusplus;
using namespace clang;

// clang-format off
std::vector<std::pair<std::string, std::string>> operator_map = {
  {"<=>", "twc"},
  {"!=", "ne"},
  {"==", "eq"},
  {"<=", "lte"},
  {">=", "gte"},
  {"<<", "lsh"},
  {">>", "rsh"},
  {"&&", "and"},
  {"||", "or"},
  {"++", "inc"},
  {"--", "dec"},
  {"->", "m"},
  {",", "com"},
  {"+", "add"},
  {"-", "sub"},
  {"*", "mul"},
  {"/", "div"},
  {"%", "mod"},
  {"^", "xorb"},
  {"&", "andb"},
  {"|", "orb"},
  {"~", "notb"},
  {"!", "not"},
  {"=", "set"},
  {"<", "lt"},
  {">", "gt"},
  {"()", "call"},
  {"[]", "idx"},
  {"\"\"", "lit"},
  {" ", ""},
  {".", "dot"}
};
// clang-format on

std::string IdentifierConfig::sanitize(const std::string &name) const {
  std::string result(name);
  std::string::size_type s;
  for (auto &pair : operator_map) {
    std::string replace = c_separator + pair.second;
    while (1) {
      s = result.find(pair.first);
      if (s == std::string::npos) {
        break;
      }
      result.replace(s, pair.first.size(), replace);
    }
  }
  return result;
}

void IdentifierConfig::printCTemplateArg(std::ostream &os, QualType QT) const {
  if (QT.isLocalConstQualified()) {
    os << "const_";
    QT.removeLocalConst();
  }
  if (QT->isAnyPointerType()) {
    printCTemplateArg(os, QT->getPointeeType());
    os << "_ptr";
  } else if (QT->isReferenceType()) {
    printCTemplateArg(os, QT->getPointeeType());
    os << "_ref";
  } else {
    std::string name = getCName(QT, "", false);
    std::string::size_type s;
    while (1) {
      s = name.find(' ');
      if (s == std::string::npos) break;
      name.replace(s, 1, "_");
    }
    os << name;
  }
}

// mirror TemplateArgument::print
void IdentifierConfig::printCTemplateArg(std::ostream &os, const TemplateArgument &Arg) const {
  switch (Arg.getKind()) {
    case TemplateArgument::Type: {
      // FIXME SubPolicy.SuppressStronglifetime = true;
      printCTemplateArg(os, Arg.getAsType());
      break;
    }

    case TemplateArgument::Declaration: {
      NamedDecl *ND = Arg.getAsDecl();
      os << getCName(ND, false);
      break;
    }

    case TemplateArgument::Pack:
      printCTemplateArgs(os, Arg.pack_elements());
      break;

    default:
      std::string Buf;
      llvm::raw_string_ostream ArgOS(Buf);
      Arg.print(PP, ArgOS);
      ArgOS.flush();
      os << ArgOS.str();
      break;
  }
}

// replaces printTemplateArgumentList(os, TemplateArgs.asArray(), P);
void IdentifierConfig::printCTemplateArgs(std::ostream &os,
                                          const ArrayRef<clang::TemplateArgument> &Args) const {
  bool FirstArg = true;
  for (const auto &Arg : Args) {
    if (Arg.getKind() == TemplateArgument::Pack) {
      if (Arg.pack_size() && !FirstArg) os << c_separator;
    } else {
      if (!FirstArg) os << c_separator;
    }

    printCTemplateArg(os, Arg);

    FirstArg = false;
  }
}

// closely follows the NamedDecl::printQualifiedName method
std::string IdentifierConfig::getCName(const clang::NamedDecl *d, bool root) const {
  if (Identifier::ids.count(d)) {
    return root ? Identifier::ids.at(d).c : Identifier::ids.at(d).c.substr(_root.size());
  }

  std::stringstream os;
  const DeclContext *Ctx = d->getDeclContext();
  if (Ctx->isFunctionOrMethod()) {
    throw mangling_error("Identifier in function or method", d, *this);
  }
  using ContextsTy = SmallVector<const DeclContext *, 8>;
  ContextsTy Contexts;

  if (root) os << _root;

  bool ctor = dyn_cast<CXXConstructorDecl>(d);
  bool dtor = dyn_cast<CXXDestructorDecl>(d);
  if (ctor) {
    os << _ctor;
  }
  if (dtor) {
    os << _dtor;
  }

  // Collect named contexts.
  while (Ctx) {
    if (isa<NamedDecl>(Ctx)) Contexts.push_back(Ctx);
    Ctx = Ctx->getParent();
  }

  bool first = true;
  for (const DeclContext *DC : llvm::reverse(Contexts)) {
    if (!first && !(isa<EnumDecl>(DC) && !dyn_cast<EnumDecl>(DC)->isScoped())) os << c_separator;
    if (const auto *D = dyn_cast<Decl>(DC)) {
      AccessSpecifier a = D->getAccess();
      if (a == AccessSpecifier::AS_private) throw mangling_error("Private parent decl", d, *this);
      if (a == AccessSpecifier::AS_protected)
        throw mangling_error("Protected parent decl", d, *this);
    }
    if (const auto *Spec = dyn_cast<ClassTemplateSpecializationDecl>(DC)) {
      os << Spec->getName().str();
      const TemplateArgumentList &TemplateArgs = Spec->getTemplateArgs();
      os << c_separator;
      printCTemplateArgs(os, TemplateArgs.asArray());
    } else if (const auto *ND = dyn_cast<NamespaceDecl>(DC)) {
      if (ND->isAnonymousNamespace()) {
        throw mangling_error("Anonymous namespace", d, *this);
      } else
        os << ND->getDeclName().getAsString();
    } else if (const auto *RD = dyn_cast<RecordDecl>(DC)) {
      if (!RD->getIdentifier()) {
        if (const auto *TD = getAnonTypedef(RD)) {
          os << TD->getDeclName().getAsString();
        } else {
          throw mangling_error("Anonymous struct or class", d, *this);
        }
      } else
        os << RD->getDeclName().getAsString();
    } else if (const auto *FD = dyn_cast<FunctionDecl>(DC)) {
      throw mangling_error("Decl inside a fuction", d, *this);
    } else if (const auto *ED = dyn_cast<EnumDecl>(DC)) {
      // C++ [dcl.enum]p10: Each enum-name and each unscoped
      // enumerator is declared in the scope that immediately contains
      // the enum-specifier. Each scoped enumerator is declared in the
      // scope of the enumeration.
      // For the case of unscoped enumerator, do not include in the qualified
      // name any information about its enum enclosing scope, as its visibility
      // is global.
      if (ED->isScoped()) os << ED->getDeclName().getAsString();
    } else {
      os << cast<NamedDecl>(DC)->getDeclName().getAsString();
    }
    first = false;
  }

  if (!ctor && !dtor) {
    if (!first) os << c_separator;
    std::string name = getName(d);
    if (name.find("operator") == 0) name = sanitize(name);
    if (name.empty()) {
      throw mangling_error("Anonymous Decl", d, *this);
    } else {
      os << name;
    }

    const TemplateArgumentList *l = nullptr;
    if (const auto *t = dyn_cast<ClassTemplateSpecializationDecl>(d))
      l = &t->getTemplateArgs();
    else if (const auto *t = dyn_cast<FunctionDecl>(d))
      l = t->getTemplateSpecializationArgs();
    if (l != nullptr) {
      os << c_separator;
      printCTemplateArgs(os, l->asArray());
    }
  }

  return os.str();
}

std::string IdentifierConfig::getCName(const QualType &qt, std::string name, bool root) const {
  const Type *t = qt.getTypePtrOrNull();
  if (t == nullptr) {
    throw mangling_error("Type is null", qt, *this);
  }
  t = t->getUnqualifiedDesugaredType();

  if (qt.isLocalConstQualified()) {
    name = "const " + name;
  }

  std::string c;
  std::string sname = name.empty() ? "" : " " + name;

  if (const auto *bt = dyn_cast<BuiltinType>(t)) {
    if (t->isNullPtrType()) {
      c = "void *" + name;
    } else {
      std::string btn = (bt->getName(PP)).str();
      c = btn + sname;
    }
  } else if (const auto *tt = dyn_cast<TagType>(t)) {
    TagDecl *TD = tt->getDecl();
    c = Identifier(TD, *this).c;
    if (!root) c = c.substr(_root.size());
    if (TD->isStruct() && getName(TD).size() && getCSystem(TD).size()) c = "struct " + c;
    c += sname;
  } else if (const auto *pt = dyn_cast<PointerType>(t)) {
    c = getCName(pt->getPointeeType(), "*" + name);
  } else if (const auto *pt = dyn_cast<ReferenceType>(t)) {
    c = getCName(pt->getPointeeType(), name);
  } else if (const auto *pt = dyn_cast<ConstantArrayType>(t)) {
    std::string s = "[" + pt->getSize().toString(10, false) + "]";
    c = getCName(pt->getElementType(), "(" + name + ")" + s);
  } else if (const auto *pt = dyn_cast<FunctionProtoType>(t)) {
    std::stringstream ss;
    bool first = true;
    for (size_t i = 0; i < pt->getNumParams(); i++) {
      std::string pname = "p" + std::to_string(i);
      if (!first) ss << ", ";
      ss << getCName(pt->getParamType(i), pname);
      first = false;
    }
    c = getCName(pt->getReturnType(), "(" + name + ")(" + ss.str() + ")");
  } else if (const auto *pt = dyn_cast<InjectedClassNameType>(t)) {
    c = getCName(pt->getDecl(), root) + sname;
  } else {
    throw mangling_error(std::string("Unknown type kind ") + t->getTypeClassName(), qt, *this);
  }

  return c;
}

std::unordered_map<const clang::NamedDecl *, Identifier> Identifier::ids;
std::unordered_map<std::string, const clang::NamedDecl *> Identifier::dups;

Identifier::Identifier(const clang::NamedDecl *d, const IdentifierConfig &cfg) {
  if (d == nullptr) {
    throw mangling_error("Null Decl", d, cfg);
  }

  const NamedDecl *p = d;
  while (p) {
    if (ids.count(p)) {
      c = ids.at(p).c;
      cpp = ids.at(p).cpp;
      return;
    }
    p = dyn_cast_or_null<NamedDecl>(p->getPreviousDecl());
  }

  // if this is an anonymous decl that is being given a name by a typedef, steal the typedef's
  // name
  // as this decl's name, but cache the result for this decl.
  const NamedDecl *orig = getAnonTypedef(d);
  if (orig)
    std::swap(d, orig);
  else
    orig = d;

  const FunctionDecl *FD = dyn_cast<FunctionDecl>(d);
  // The name-mangling is not applied to extern C functions, which are declared with the same name
  // so users link to the original, or to C system header structs and typedefs which are included
  // and used directly.
  if ((FD && (FD->isExternC() || FD->isInExternCContext()) && !FD->isCXXClassMember()) ||
      getCSystem(d).size()) {
    c = d->getDeclName().getAsString();
    if (dups.count(c)) {
      throw mangling_error("Generated symbol conflicts with a C symbol", dups.at(c), cfg);
    }
  } else {
    c = cfg.getCName(d);
    if (dups.count(c)) {
      unsigned cnt = 2;
      std::string nc;
      while (dups.count(nc = c + "_" + std::to_string(cnt))) cnt++;
      c = nc;
    }
  }

  cpp = cfg.getCXXQualifiedName(d);

  dups.emplace(c, d);
  ids.emplace(std::make_pair(orig, *this));
}

Identifier::Identifier(const QualType &qt, const Identifier &name, const IdentifierConfig &cfg) {
  c = cfg.getCName(qt, name.c);
  std::string s;
  llvm::raw_string_ostream ss(s);
  QualType desugar = qt.getDesugaredType(cfg.astc);
  if (desugar->isNullPtrType())
    ss << "std::nullptr_t " << name.cpp;
  else
    desugar.print(ss, cfg.PP, name.cpp);
  ss.flush();
  cpp = ss.str();
}

std::string unplusplus::getName(const Decl *d) {
  if (!d) return "<null>";
  const NamedDecl *nd = dyn_cast<NamedDecl>(d);
  if (!nd) return "<" + std::string(d->getDeclKindName()) + ">";
  if (nd->getDeclName())
    return nd->getDeclName().getAsString();
  else {
    // Give the printName override a chance to pick a different name before we
    // fall back to "(anonymous)".
    SmallString<64> NameBuffer;
    llvm::raw_svector_ostream NameOS(NameBuffer);
    nd->printName(NameOS);
    return NameBuffer.str().str();
  }
}

std::string IdentifierConfig::getDebugName(const clang::Decl *d) const {
  std::string name = std::string(d->getDeclKindName()) + " ";
  if (const auto *fd = dyn_cast_or_null<FunctionDecl>(d)) {
    std::string s;
    llvm::raw_string_ostream cxx_params(s);
    cxx_params << getCXXQualifiedName(fd) << "(";
    bool first = true;
    for (auto *p : fd->parameters()) {
      if (!first) cxx_params << ", ";
      p->getType().print(cxx_params, PP, getName(p));
      first = false;
    }
    cxx_params << ")";
    cxx_params.flush();
    llvm::raw_string_ostream namestream(name);
    fd->getReturnType().print(namestream, PP, s);
    namestream.flush();
  } else {
    name += getCXXQualifiedName(d);
  }
  return name;
}

std::string IdentifierConfig::getDebugName(const clang::QualType &T) const {
  std::string name = T->getTypeClassName();
  llvm::raw_string_ostream ss(name);
  ss << " ";
  T.print(ss, PP);
  ss.flush();
  return name;
}

std::string IdentifierConfig::getCXXQualifiedName(const clang::Decl *D) const {
  return unplusplus::getCXXQualifiedName(PP, D);
}

std::string unplusplus::getCXXQualifiedName(const clang::PrintingPolicy &PP, const clang::Decl *d) {
  if (!d) return "<null>";
  if (const auto *nd = dyn_cast<NamedDecl>(d)) {
    std::string s;
    llvm::raw_string_ostream ArgOS(s);
    nd->getNameForDiagnostic(ArgOS, PP, true);
    return ArgOS.str();
  } else {
    return "<" + std::string(d->getDeclKindName()) + ">";
  }
}

const TypedefDecl *unplusplus::getAnonTypedef(const NamedDecl *d) {
  if (getName(d).empty()) {
    // try to find a typedef that is naming this anonymous declaration
    if (const auto *tnext = dyn_cast_or_null<TypedefDecl>(d->getNextDeclInContext())) {
      if (const auto *tagt =
              dyn_cast<TagType>(tnext->getUnderlyingType()->getUnqualifiedDesugaredType())) {
        const NamedDecl *tagd = tagt->getDecl()->getUnderlyingDecl();
        if (tagd == d) {
          return tnext;
        }
      }
    }
  }
  return nullptr;
}

std::string unplusplus::getCSystem(const clang::Decl *D) {
  SourceManager &SM = D->getASTContext().getSourceManager();
  FileID FID = SM.getFileID(SM.getFileLoc(D->getLocation()));
  bool Invalid = false;
  const SrcMgr::SLocEntry &SEntry = SM.getSLocEntry(FID, &Invalid);
  SrcMgr::CharacteristicKind ck = SEntry.getFile().getFileCharacteristic();
  if (ck == SrcMgr::CharacteristicKind::C_ExternCSystem)
    return SEntry.getFile().getName().str();
  else
    return "";
}
