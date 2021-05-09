/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "identifier.hpp"

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/TemplateBase.h>

#include <iostream>
#include <sstream>
#include <unordered_map>

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

  if (isLibraryInternal(d)) {
    throw mangling_error("Library internal " + getCXXQualifiedName(d));
  }

  std::stringstream os;
  const DeclContext *Ctx = d->getDeclContext();
  if (Ctx->isFunctionOrMethod()) {
    throw mangling_error("Identifier in function or method");
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
      if (a == AccessSpecifier::AS_private) throw mangling_error("Private parent decl");
      if (a == AccessSpecifier::AS_protected) throw mangling_error("Protected parent decl");
    }
    if (const auto *Spec = dyn_cast<ClassTemplateSpecializationDecl>(DC)) {
      os << Spec->getName().str();
      const TemplateArgumentList &TemplateArgs = Spec->getTemplateArgs();
      os << c_separator;
      printCTemplateArgs(os, TemplateArgs.asArray());
    } else if (const auto *ND = dyn_cast<NamespaceDecl>(DC)) {
      if (ND->isAnonymousNamespace()) {
        throw mangling_error("Anonymous namespace");
      } else
        os << ND->getDeclName().getAsString();
    } else if (const auto *RD = dyn_cast<RecordDecl>(DC)) {
      if (!RD->getIdentifier())
        throw mangling_error("Anonymous struct or class");
      else
        os << RD->getDeclName().getAsString();
    } else if (const auto *FD = dyn_cast<FunctionDecl>(DC)) {
      throw mangling_error("Decl inside a fuction");
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
      if (isa<EnumDecl>(d))
        os << "<anonymous>";
      else
        throw mangling_error("Anonymous Decl");
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

std::string IdentifierConfig::getCName(const Type *t, const std::string &name, bool root) const {
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
    if (root)
      c = Identifier(tt->getDecl(), *this).c + sname;
    else
      c = getCName(tt->getDecl(), root) + sname;
  } else if (const auto *pt = dyn_cast<PointerType>(t)) {
    c = getCName(pt->getPointeeType(), "*" + name);
  } else if (const auto *pt = dyn_cast<ReferenceType>(t)) {
    c = getCName(pt->getPointeeType(), name);
  } else if (const auto *pt = dyn_cast<ConstantArrayType>(t)) {
    std::string s = "[" + pt->getSize().toString(10, false) + "]";
    c = getCName(pt->getElementType(), name + s);
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
  } else {
    throw mangling_error(std::string("Unknown type kind ") + t->getTypeClassName());
  }
  return c;
}

std::string IdentifierConfig::getCName(const QualType &qt, std::string name, bool root) const {
  const Type *t = qt.getTypePtrOrNull();
  if (t == nullptr) {
    throw mangling_error("Type is null");
  }
  t = t->getUnqualifiedDesugaredType();

  if (qt.isLocalConstQualified()) {
    name = "const " + name;
  }

  std::string c = getCName(t, name, root);

  return c;
}

std::unordered_map<const clang::NamedDecl *, Identifier> Identifier::ids;
std::unordered_set<std::string> Identifier::dups;

Identifier::Identifier(const clang::NamedDecl *d, const IdentifierConfig &cfg) {
  if (d == nullptr) {
    throw mangling_error("Null Decl");
  }

  if (ids.count(d)) {
    c = ids.at(d).c;
    cpp = ids.at(d).cpp;
  } else {
    // if this is an anonymous decl that is being given a name by a typedef, steal the typedef's
    // name
    // as this decl's name, but cache the result for this decl.
    const NamedDecl *orig = getAnonTypedef(d);
    if (orig)
      std::swap(d, orig);
    else
      orig = d;

    if (d->getDeclContext()->isExternCContext()) {
      c = d->getDeclName().getAsString();
      if (dups.count(c)) {
        throw mangling_error("An automatically generated symbol conflicts with a C symbol `" + c +
                             "`");
      }
    } else {
      c = cfg.getCName(d);
      if (dups.count(c)) {
        unsigned cnt = 2;
        std::string nc;
        while (dups.count(nc = c + "_" + std::to_string(cnt))) cnt++;
        c = nc;
      }
      dups.emplace(c);
    }

    cpp = cfg.getCXXQualifiedName(d);

    ids.emplace(std::make_pair(orig, *this));
  }
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

static bool isLibraryInternal(QualType QT) {
  while (QT->isReferenceType() || QT->isPointerType()) QT = QT->getPointeeType();
  if (const auto *tt = dyn_cast<TagType>(QT->getUnqualifiedDesugaredType())) {
    if (isLibraryInternal(tt->getDecl())) return true;
  }
  return false;
}

bool unplusplus::isLibraryInternal(const clang::NamedDecl *ND) {
  std::string name = getName(ND);

  if (name == "__gnu_cxx" || name == "__cxxabiv1") return true;

  const DeclContext *Ctx = ND->getDeclContext();

  if (Ctx)
    if (const auto *NCtx = dyn_cast<NamedDecl>(Ctx))
      if (name[0] == '_' && getName(NCtx) == "std") return true;

  if (const auto *td = dyn_cast<ClassTemplateSpecializationDecl>(ND)) {
    for (const auto &Arg : td->getTemplateArgs().asArray()) {
      switch (Arg.getKind()) {
        case TemplateArgument::Type: {
          QualType qt = Arg.getAsType();
          if (::isLibraryInternal(Arg.getAsType())) return true;
        } break;
        case TemplateArgument::Declaration:
          if (isLibraryInternal(Arg.getAsDecl())) return true;
          break;
        default:
          break;
      }
    }
  }

  if (const auto *FD = dyn_cast<FunctionDecl>(ND)) {
    if (::isLibraryInternal(FD->getReturnType())) return true;
    for (const auto *PD : FD->parameters()) {
      if (::isLibraryInternal(PD->getType())) return true;
    }
  }

  if (Ctx)
    if (const auto *NCtx = dyn_cast<NamedDecl>(Ctx)) return isLibraryInternal(NCtx);

  return false;
}

std::string unplusplus::getName(const NamedDecl *d) {
  if (d->getDeclName())
    return d->getDeclName().getAsString();
  else {
    // Give the printName override a chance to pick a different name before we
    // fall back to "(anonymous)".
    SmallString<64> NameBuffer;
    llvm::raw_svector_ostream NameOS(NameBuffer);
    d->printName(NameOS);
    return NameBuffer.str().str();
  }
}

std::string IdentifierConfig::getCXXQualifiedName(const clang::NamedDecl *d) const {
  std::string s;
  llvm::raw_string_ostream ArgOS(s);
  d->getNameForDiagnostic(ArgOS, PP, true);
  return ArgOS.str();
}

const TypedefDecl *unplusplus::getAnonTypedef(const NamedDecl *d) {
  if (getName(d).empty()) {
    // try to find a typedef that is naming this anonymous declaration
    if (const auto *tnext = dyn_cast<TypedefDecl>(d->getNextDeclInContext())) {
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
