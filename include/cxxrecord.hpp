/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/AST/CXXInheritance.h>
#include <clang/AST/DeclCXX.h>

#include "jobs.hpp"

namespace unplusplus {
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
  bool _no_ctor = false;
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
