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

static void printCTemplateArgs(std::ostream &os, const ArrayRef<clang::TemplateArgument> &Args,
                               const IdentifierConfig &cfg);

static void printCTemplateArg(std::ostream &os, QualType QT, const IdentifierConfig &cfg) {
  if (QT.isLocalConstQualified()) {
    os << "const_";
    QT.removeLocalConst();
  }
  if (QT->isAnyPointerType()) {
    printCTemplateArg(os, QT->getPointeeType(), cfg);
    os << "_ptr";
  } else if (QT->isReferenceType()) {
    printCTemplateArg(os, QT->getPointeeType(), cfg);
    os << "_ref";
  } else {
    std::string name = cfg.getCName(QT, "", false);
    std::string::size_type s;
    while (1) {
      s = name.find(' ');
      if (s == std::string::npos)
        break;
      name.replace(s, 1, "_");
    }
    os << name;
  }
}

// mirror TemplateArgument::print
static void printCTemplateArg(std::ostream &os, const TemplateArgument &Arg, const IdentifierConfig &cfg) {
  switch (Arg.getKind()) {
    case TemplateArgument::Type: {
      // FIXME SubPolicy.SuppressStronglifetime = true;
      printCTemplateArg(os, Arg.getAsType(), cfg);
      break;
    }

    case TemplateArgument::Declaration: {
      NamedDecl *ND = Arg.getAsDecl();
      os << cfg.getCName(ND, false);
      break;
    }

    case TemplateArgument::Pack:
      printCTemplateArgs(os, Arg.pack_elements(), cfg);
      break;

    default:
      std::string Buf;
      llvm::raw_string_ostream ArgOS(Buf);
      Arg.print(cfg.PP, ArgOS);
      ArgOS.flush();
      os << ArgOS.str();
      break;
  }
}

// replaces printTemplateArgumentList(os, TemplateArgs.asArray(), P);
static void printCTemplateArgs(std::ostream &os, const ArrayRef<clang::TemplateArgument> &Args,
                               const IdentifierConfig &cfg) {
  bool FirstArg = true;
  for (const auto &Arg : Args) {
    if (Arg.getKind() == TemplateArgument::Pack) {
      if (Arg.pack_size() && !FirstArg) os << cfg.c_separator;
    } else {
      if (!FirstArg) os << cfg.c_separator;
    }

    printCTemplateArg(os, Arg, cfg);

    FirstArg = false;
  }
}

// closely follows the NamedDecl::printQualifiedName method
std::string IdentifierConfig::getCName(const clang::NamedDecl *d, bool root) const {
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
      printCTemplateArgs(os, TemplateArgs.asArray(), *this);
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
    if (name.find("operator") == 0)
      name = sanitize(name);
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
      printCTemplateArgs(os, l->asArray(), *this);
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

std::string IdentifierConfig::getCName(const QualType &qt, const std::string &name, bool root) const {
  const Type *t = qt.getTypePtrOrNull();
  if (t == nullptr) {
    throw mangling_error("Type is null");
  }
  t = t->getUnqualifiedDesugaredType();

  std::string c = getCName(t, name, root);

  if (qt.isLocalConstQualified()) {
    c = "const " + c;
  }

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

    clang::SmallString<128> Buf;
    llvm::raw_svector_ostream ArgOS(Buf);
    d->getNameForDiagnostic(ArgOS, cfg.PP, true);
    cpp = ArgOS.str().str();

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

bool unplusplus::isLibraryInternal(const clang::NamedDecl *d) {
  const DeclContext *Ctx = d->getDeclContext();
  SmallVector<const DeclContext *, 8> Contexts;

  // Collect named contexts.
  while (Ctx) {
    if (isa<NamedDecl>(Ctx)) Contexts.push_back(Ctx);
    Ctx = Ctx->getParent();
  }

  // FIXME: A hack to filter out internal-only parts of the standard library
  if (!Contexts.empty()) {
    if (const auto *ND = dyn_cast<NamedDecl>(*(Contexts.end() - 1))) {
      std::string root = ND->getDeclName().getAsString();
      if (root == "__gnu_cxx") {
        return true;
      } else if (root == "__cxxabiv1") {
        return true;
      }
      if (root == "std" && Contexts.size() > 1) {
        if (const auto *ND = dyn_cast<NamedDecl>(*(Contexts.end() - 2))) {
          std::string root = ND->getDeclName().getAsString();
          if (root[0] == '_' && root[1] == '_') {
            return true;
          }
        }
      }
    }
  }

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
