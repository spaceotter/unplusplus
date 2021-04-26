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
  // these determine how flattened C names are assembled
  std::string root_prefix = "upp_";
  std::string c_separator = "_";
  std::string _this = "_upp_this";
  std::string _return = "_upp_return";
  std::string _struct = "_s_";
  std::string dtor = "del_";
  std::string ctor = "new_";
  clang::PrintingPolicy PP = {clang::LangOptions()};
  std::string sanitize(const std::string &name) const;
};

struct Identifier {
  // remember old identifiers to save time, they don't change
  static std::unordered_map<const clang::NamedDecl *, Identifier> ids;
  static std::unordered_map<const clang::Type *, Identifier> types;
  // Remember generated names to rename duplicates
  static std::unordered_set<std::string> dups;

  Identifier(const clang::NamedDecl *d, const IdentifierConfig &cfg);
  Identifier(const clang::Type *d, const IdentifierConfig &cfg);
  Identifier() {}
  bool empty() const { return c.empty() && cpp.empty(); }

  std::string c;    // a name-mangled identifier for C
  std::string cpp;  // the fully qualified C++ name
};
}  // namespace unplusplus
