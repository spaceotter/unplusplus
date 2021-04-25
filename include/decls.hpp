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
  std::unordered_set<const clang::Decl *> _decls;

 public:
  DeclHandler(Outputs &out) : _out(out) {}
  void add(const clang::Decl *d);
  Outputs &out() { return _out; }
};
}  // namespace unplusplus
