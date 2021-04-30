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
  std::stringstream _templates;

 public:
  DeclHandler(Outputs &out) : _out(out) {}
  void add(const clang::Decl *d);
  Outputs &out() { return _out; }
  void finish();
  std::string templates() const { return _templates.str(); }
  void addTemplate(const std::string &name) {
    _templates << "extern template class " << name << ";\n";
  }
  const IdentifierConfig &cfg() const { return _out.cfg(); }
  // Ensure that a type is declared already
  void forward(const clang::QualType &t);
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
};

}  // namespace unplusplus
