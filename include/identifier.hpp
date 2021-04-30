/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/AST/Decl.h>
#include <clang/AST/PrettyPrinter.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace unplusplus {
struct mangling_error : public std::runtime_error {
  mangling_error(const std::string &what_arg) : std::runtime_error(what_arg) {}
};

struct IdentifierConfig {
  IdentifierConfig(const clang::LangOptions &opts) : PP(opts) {}
  // these determine how flattened C names are assembled
  std::string root_prefix = "upp_";
  std::string c_separator = "_";
  std::string _this = "_upp_this";
  std::string _return = "_upp_return";
  std::string _struct = "_s_";
  std::string dtor = "del_";
  std::string ctor = "new_";
  clang::PrintingPolicy PP;
  std::string sanitize(const std::string &name) const;
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

bool isLibraryInternal(const clang::NamedDecl *d);
}  // namespace unplusplus
