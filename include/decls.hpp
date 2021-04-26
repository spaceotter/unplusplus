/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/AST/Decl.h>

#include "outputs.hpp"

namespace unplusplus {
class DeclWriterBase;
typedef std::unordered_map<const clang::Decl *, std::unique_ptr<DeclWriterBase>> DeclWriterMap;
class DeclHandler {
  Outputs &_out;
  DeclWriterMap _decls;

 public:
  DeclHandler(Outputs &out) : _out(out) {}
  void add(const clang::Decl *d);
  Outputs &out() { return _out; }
  void finish();
};

class DeclWriterBase {
 protected:
  DeclHandler &_dh;
  Outputs &_out;

 public:
  DeclWriterBase(DeclHandler &dh) : _dh(dh), _out(dh.out()) {}
  virtual ~DeclWriterBase() {}
  const IdentifierConfig &cfg() const { return _out.cfg(); }
  const Outputs &out() const { return _out; }
  // Ensure that a type is declared already
  void forward(const clang::Type *t);
};

}  // namespace unplusplus
