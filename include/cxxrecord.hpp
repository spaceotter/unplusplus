/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/AST/CXXInheritance.h>
#include <clang/AST/DeclCXX.h>

#include "decls.hpp"

namespace unplusplus {
struct CXXRecordDeclWriter : public DeclWriter<clang::CXXRecordDecl> {
 private:
  bool _makeInstantiation = false;  // whether we need to emit an explicit template specialization
  std::unordered_set<std::string> _fields;
  std::unordered_set<const clang::CXXRecordDecl *> _vbases;
  clang::CXXIndirectPrimaryBaseSet _indirect;
  std::string _keyword;
  bool _wroteMembers = false;
  clang::Sema &_S;

  void writeMembers(Outputs &out);
  void writeFields(Outputs &out, const clang::CXXRecordDecl *d, std::string indent = "  ");
  void writeVirtualBases(Outputs &out, const clang::CXXRecordDecl *d);
  void writeNonVirtualBases(Outputs &out, const clang::CXXRecordDecl *d);

 public:
  CXXRecordDeclWriter(const type *d, clang::Sema &S, DeclHandler &dh);
  virtual ~CXXRecordDeclWriter() override;
  void maybeDefine();
};
}  // namespace unplusplus
