/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/AST/Decl.h>

#include "outputs.hpp"

namespace unplusplus {
class DeclHandler {
  Outputs &_out;
  std::unordered_set<const clang::NamedDecl *> _decls;
 public:
  DeclHandler(Outputs &out) : _out(out) {}
  void add(const clang::NamedDecl *d);
  Outputs &out() { return _out; }
};
}
