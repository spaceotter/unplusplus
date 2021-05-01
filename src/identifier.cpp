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
std::unordered_map<std::string, std::string> operator_map = {
  {"+", "add"},
  {"*", "mul"},
  {"/", "div"},
  {"-", "sub"},
  {"=", "set"},
  {"()", "call"},
  {"[]", "idx"},
  {" ", ""},
  {".", "dot"}
};
// clang-format on

std::string IdentifierConfig::sanitize(const std::string &name) const {
  std::string result(name);
  std::string::size_type s;
  for (auto &pair : operator_map) {
    std::string replace = c_separator + pair.second + c_separator;
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

// replaces printTemplateArgumentList(os, TemplateArgs.asArray(), P);
static void printCTemplateArgs(std::ostream &os, const ArrayRef<clang::TemplateArgument> &Args,
                               const IdentifierConfig &cfg) {
  bool FirstArg = true;
  for (const auto &Arg : Args) {
    // Print the argument into a string.
    SmallString<128> Buf;
    llvm::raw_svector_ostream ArgOS(Buf);
    const TemplateArgument &Argument = Arg;
    if (Argument.getKind() == TemplateArgument::Pack) {
      if (Argument.pack_size() && !FirstArg) os << cfg.c_separator;
      printCTemplateArgs(os, Argument.getPackAsArray(), cfg);
    } else {
      if (!FirstArg) os << cfg.c_separator;
      // Tries to print the argument with location info if exists.
      Arg.print(cfg.PP, ArgOS);
    }

    StringRef ArgString = ArgOS.str();
    os << ArgString.str();

    FirstArg = false;
  }
}

// closely follows the NamedDecl::printQualifiedName method
static std::string getCName(const clang::NamedDecl *d, const IdentifierConfig &cfg) {
  std::stringstream os;
  const DeclContext *Ctx = d->getDeclContext();
  if (Ctx->isFunctionOrMethod()) {
    throw mangling_error("Identifier in function or method");
  }
  using ContextsTy = SmallVector<const DeclContext *, 8>;
  ContextsTy Contexts;

  os << cfg.root_prefix;

  bool ctor = dyn_cast<CXXConstructorDecl>(d);
  bool dtor = dyn_cast<CXXDestructorDecl>(d);
  if (ctor) {
    os << cfg.ctor;
  }
  if (dtor) {
    os << cfg.dtor;
  }

  // Collect named contexts.
  while (Ctx) {
    if (isa<NamedDecl>(Ctx)) Contexts.push_back(Ctx);
    Ctx = Ctx->getParent();
  }

  bool first = true;
  for (const DeclContext *DC : llvm::reverse(Contexts)) {
    if (!first) os << cfg.c_separator;
    if (const auto *D = dyn_cast<Decl>(DC)) {
      AccessSpecifier a = D->getAccess();
      if (a == AccessSpecifier::AS_private) throw mangling_error("Private parent decl");
      if (a == AccessSpecifier::AS_protected) throw mangling_error("Protected parent decl");
    }
    if (const auto *Spec = dyn_cast<ClassTemplateSpecializationDecl>(DC)) {
      os << Spec->getName().str();
      const TemplateArgumentList &TemplateArgs = Spec->getTemplateArgs();
      os << cfg.c_separator;
      printCTemplateArgs(os, TemplateArgs.asArray(), cfg);
    } else if (const auto *ND = dyn_cast<NamespaceDecl>(DC)) {
      if (ND->isAnonymousNamespace()) {
        throw mangling_error("Anonymous namespace");
      } else
        os << ND->getDeclName().getAsString();
    } else if (const auto *RD = dyn_cast<clang::RecordDecl>(DC)) {
      if (!RD->getIdentifier())
        throw mangling_error("Anonymous struct or class");
      else
        os << RD->getDeclName().getAsString();
    } else if (const auto *FD = dyn_cast<clang::FunctionDecl>(DC)) {
      throw mangling_error("Decl inside a fuction");
    } else if (const auto *ED = dyn_cast<clang::EnumDecl>(DC)) {
      // C++ [dcl.enum]p10: Each enum-name and each unscoped
      // enumerator is declared in the scope that immediately contains
      // the enum-specifier. Each scoped enumerator is declared in the
      // scope of the enumeration.
      // For the case of unscoped enumerator, do not include in the qualified
      // name any information about its enum enclosing scope, as its visibility
      // is global.
      if (ED->isScoped())
        os << ED->getDeclName().getAsString();
    } else {
      os << cast<NamedDecl>(DC)->getDeclName().getAsString();
    }
    first = false;
  }

  if (!ctor && !dtor) {
    if (!first) os << cfg.c_separator;
    std::string name = getName(d);
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
      os << cfg.c_separator;
      printCTemplateArgs(os, l->asArray(), cfg);
    }
  }

  return cfg.sanitize(os.str());
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
      c = getCName(d, cfg);
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
  const Type *t = qt.getTypePtrOrNull();
  if (t == nullptr) {
    throw mangling_error("Null Decl");
  }
  t = t->getUnqualifiedDesugaredType();

  if (const auto *bt = dyn_cast<BuiltinType>(t)) {
    if (qt->isNullPtrType()) {
      c = "void * " + name.c;
      cpp = "std::nullptr_t " + name.cpp;
    } else {
      std::string btn = (bt->getName(cfg.PP) + " ").str();
      c = btn + name.c;
      cpp = btn + name.cpp;
    }
  } else if (const auto *tt = dyn_cast<TagType>(t)) {
    *this = Identifier(tt->getDecl(), cfg);
    c += " " + name.c;
    cpp += " " + name.cpp;
  } else if (const auto *pt = dyn_cast<PointerType>(t)) {
    *this = Identifier(pt->getPointeeType(), {"*" + name.c, "*" + name.cpp}, cfg);
  } else if (const auto *pt = dyn_cast<ReferenceType>(t)) {
    *this = Identifier(pt->getPointeeType(), name, cfg);
  } else if (const auto *pt = dyn_cast<ConstantArrayType>(t)) {
    std::string s = "[" + pt->getSize().toString(10, false) + "]";
    *this = Identifier(pt->getElementType(), {name.c + s, name.cpp + s}, cfg);
  } else if (const auto *pt = dyn_cast<FunctionProtoType>(t)) {
    std::stringstream ssc;
    std::stringstream sscpp;
    bool first = true;
    for (size_t i = 0; i < pt->getNumParams(); i++) {
      Identifier pname("p" + std::to_string(i), cfg);
      Identifier p(pt->getParamType(i), pname, cfg);
      if (!first) {
        ssc << ", ";
        sscpp << ", ";
      }
      ssc << p.c;
      sscpp << p.cpp;
      first = false;
    }
    *this = Identifier(
        pt->getReturnType(),
        {"(" + name.c + ")(" + ssc.str() + ")", "(" + name.cpp + ")(" + sscpp.str() + ")"}, cfg);
  } else {
    throw mangling_error(std::string("Unknown type kind ") + t->getTypeClassName());
  }

  if (qt.isLocalConstQualified()) {
    c = "const " + c;
    cpp = "const " + cpp;
  }
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
