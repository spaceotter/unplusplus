/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/AST/Decl.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/ASTContext.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace unplusplus {
struct mangling_error : public std::runtime_error {
  mangling_error(const std::string &what_arg) : std::runtime_error(what_arg) {}
};

struct IdentifierConfig {
  IdentifierConfig(const clang::ASTContext &astc) : astc(astc), PP(astc.getLangOpts()) {}
  // these determine how flattened C names are assembled
  std::string _root = "upp_";
  std::string c_separator = "_";
  std::string _this = "_upp_this";
  std::string _return = "_upp_return";
  std::string _struct = "_s_";
  std::string _enum = "_e_";
  std::string _dtor = "del_";
  std::string _ctor = "new_";
  // this is needed for "desugaring" and constructing derived types
  const clang::ASTContext &astc;
  // this is needed for clang to print things correctly, like bool
  const clang::PrintingPolicy PP;

  // remove illegal characters
  std::string sanitize(const std::string &name) const;

  // get a mangled name to use in C to refer to C++ clang declarations
  std::string getCName(const clang::NamedDecl *d, bool root = true) const;
  // get a type specifier that uses the mangled C names, and wraps the given name
  std::string getCName(const clang::Type *t, const std::string &name, bool root = true) const;
  std::string getCName(const clang::QualType &qt, const std::string &name, bool root = true) const;
};

struct Identifier {
  // remember old identifiers to save time, they don't change
  static std::unordered_map<const clang::NamedDecl *, Identifier> ids;
  // Remember generated names to rename duplicates
  static std::unordered_set<std::string> dups;

  Identifier(const clang::NamedDecl *d, const IdentifierConfig &cfg);
  Identifier(const clang::QualType &d, const Identifier &name, const IdentifierConfig &cfg);
  Identifier() {}
  Identifier(std::string name, const IdentifierConfig &cfg) : c(cfg.sanitize(name)), cpp(name) {}
  Identifier(std::string c, std::string cpp) : c(c), cpp(cpp) {}
  Identifier(std::string s) : c(s), cpp(s) {}
  bool empty() const { return c.empty() && cpp.empty(); }

  std::string c;    // a name-mangled identifier for C
  std::string cpp;  // the fully qualified C++ name
};

// a filter function to avoid some private library declarations
bool isLibraryInternal(const clang::NamedDecl *d);
// Get the decl name, or overridden print operator
std::string getName(const clang::NamedDecl *d);
// If the NamedDecl is an anonymous struct or enum, get the typedef that is giving it a name.
const clang::TypedefDecl *getAnonTypedef(const clang::NamedDecl *d);
}  // namespace unplusplus
