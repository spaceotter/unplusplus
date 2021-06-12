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
  typedef const std::vector<const clang::CXXRecordDecl *> ClassList;
  struct FieldInfo {
    const clang::FieldDecl *field;
    ClassList parents;
    std::string name;
    clang::QualType type;
    bool isUnion;
    std::shared_ptr<std::unordered_map<std::string, unsigned>> nameCount;
    std::vector<FieldInfo> subFields;
    FieldInfo()
        : name("root"), nameCount(std::make_shared<std::unordered_map<std::string, unsigned>>()) {}
    FieldInfo(const clang::FieldDecl *F, ClassList P, std::string N, clang::QualType T,
              bool isUnion = false)
        : field(F),
          parents(P),
          name(N),
          type(T),
          isUnion(isUnion),
          nameCount(std::make_shared<std::unordered_map<std::string, unsigned>>()) {}
    void sub(const clang::FieldDecl *F, ClassList P, std::string N, clang::QualType T,
             bool isUnion = false) {
      subFields.push_back({F, P, N, T, isUnion});
      if (N.size()) (*nameCount)[N] += 1;
    }
    void adjustNames();
  };
  bool _no_ctor = false;
  std::unordered_set<const clang::CXXRecordDecl *> _vbases;
  clang::CXXIndirectPrimaryBaseSet _indirect;
  FieldInfo _fields;

  std::string nameField(const std::string &original);
  void findFields();
  void addFields(const clang::CXXRecordDecl *d, ClassList parents, FieldInfo &list);
  void findVirtualBaseFields(const clang::CXXRecordDecl *d, ClassList parents);
  void findNonVirtualBaseFields(const clang::CXXRecordDecl *d, ClassList parents);
  void writeFields(FieldInfo &list, std::string indent = "  ",
                   std::unordered_set<std::string> *names = nullptr);

 public:
  static bool accept(type *D, const IdentifierConfig &cfg, clang::Sema &S);
  ClassDefineJob(type *D, clang::Sema &S, JobManager &manager);
  void impl() override;
};

}  // namespace unplusplus
