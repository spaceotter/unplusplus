/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/AST/CXXInheritance.h>
#include <clang/AST/DeclCXX.h>

#include "decls.hpp"
#include "jobs.hpp"

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

struct ClassDeclareJob : public Job<clang::CXXRecordDecl> {
  static bool accept(const type *D);
  ClassDeclareJob(type *D, clang::Sema &S, JobManager &manager);
  void impl() override;
};

class ClassDefineJob : public Job<clang::CXXRecordDecl> {
  struct FieldInfo {
    const clang::FieldDecl *field;
    const clang::CXXRecordDecl *parent;
    std::string name;
    clang::QualType type;
    bool isUnion;
    std::vector<FieldInfo> subFields;
  };
  bool _makeInstantiation = false;
  std::unordered_set<const clang::CXXRecordDecl *> _vbases;
  clang::CXXIndirectPrimaryBaseSet _indirect;
  std::vector<FieldInfo> _fields;
  std::unordered_map<std::string, unsigned> _nameCount;

  std::string nameField(const std::string &original);
  void findFields();
  void addFields(const clang::CXXRecordDecl *d, std::vector<FieldInfo> &list);
  void findVirtualBaseFields(const clang::CXXRecordDecl *d);
  void findNonVirtualBaseFields(const clang::CXXRecordDecl *d);
  void writeFields(std::vector<FieldInfo> &list, std::string indent = "  ");

 public:
  static bool accept(type *D, const IdentifierConfig &cfg, clang::Sema &S);
  ClassDefineJob(type *D, clang::Sema &S, JobManager &manager);
  void impl() override;
};

}  // namespace unplusplus
