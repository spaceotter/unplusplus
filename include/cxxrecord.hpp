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

typedef const std::vector<const clang::CXXRecordDecl *> ClassList;
struct SuperclassVisitor {
  typedef std::function<void(const clang::CXXRecordDecl *, ClassList)> Visitor;

 private:
  std::unordered_set<const clang::CXXRecordDecl *> _vbases;
  clang::CXXIndirectPrimaryBaseSet _indirect;
  Visitor _fn;
  Visitor _fn2;
  void visitNonVirtualBase(const clang::CXXRecordDecl *D, ClassList L);
  void visitVirtualBase(const clang::CXXRecordDecl *D, ClassList L);

 public:
  /**
   * Visits the superclass hierarchy in the correct data layout order depending on
   * virtual/non-virtual inheritance.
   * @param[in] F The function to process a base class
   * @param[in] D The derived class
   * @param[in] H Optional, called before non-virtual superclasses
   */
  SuperclassVisitor(Visitor F, const clang::CXXRecordDecl *D, Visitor H = nullptr);
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
  FieldInfo _fields;

  std::string nameField(const std::string &original);
  void findFields();
  void addFields(const clang::CXXRecordDecl *d, ClassList parents, FieldInfo &list);
  void writeFields(FieldInfo &list, std::string indent = "  ",
                   std::unordered_set<std::string> *names = nullptr);

 public:
  static bool accept(type *D, const IdentifierConfig &cfg, clang::Sema &S);
  ClassDefineJob(type *D, clang::Sema &S, JobManager &manager);
  void impl() override;
};

}  // namespace unplusplus
