/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <clang/AST/Decl.h>

namespace unplusplus {
struct mangling_error : public std::runtime_error {
  mangling_error(const std::string &what_arg) : std::runtime_error(what_arg) {}
};

struct IdentifierConfig {
  // these determine how flattened C names are assembled
  std::string root_prefix = "upp_";
  std::string c_separator = "_";
  std::string _this = "_upp_this";
  std::string _return = "_upp_return";
  std::string _struct = "_s_";
  std::string dtor = "_dtor";
  std::string ctor = "_ctor";
  std::string sanitize(const std::string &name) const;
};

struct Identifier {
  // remember old identifiers to save time, they don't change
  static std::unordered_map<const clang::NamedDecl *, Identifier> ids;
  // Remember generated names to rename duplicates
  static std::unordered_set<std::string> dups;

  Identifier(const clang::NamedDecl *d, const IdentifierConfig &cfg);

  std::string c;    // a name-mangled identifier for C
  std::string cpp;  // the fully qualified C++ name
};
}
