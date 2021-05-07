/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include "decls.hpp"
#include <clang/AST/DeclCXX.h>
#include <clang/AST/CXXInheritance.h>

namespace unplusplus {
struct CXXRecordDeclWriter : public DeclWriter<clang::CXXRecordDecl> {
 private:
  bool _wroteMembers = false;
  std::unordered_set<std::string> _fields;
  std::unordered_set<const clang::CXXRecordDecl *> _vbases;
  clang::CXXIndirectPrimaryBaseSet _indirect;

  void writeMembers();
  void writeFields(Outputs &out, const clang::CXXRecordDecl *d);
  void writeVirtualBases(Outputs &out, const clang::CXXRecordDecl *d);
  void writeNonVirtualBases(Outputs &out, const clang::CXXRecordDecl *d);
 public:
  CXXRecordDeclWriter(const type *d, DeclHandler &dh);
  virtual ~CXXRecordDeclWriter() override;
};
}
