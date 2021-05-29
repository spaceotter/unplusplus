/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include "decls.hpp"
#include "jobs.hpp"

namespace unplusplus {
struct FunctionDeclWriter : public DeclWriter<clang::FunctionDecl> {
  FunctionDeclWriter(const type *d, clang::Sema &S, DeclHandler &dh);
};

class FunctionJob : public Job<clang::FunctionDecl> {
  std::vector<clang::QualType> _paramTypes;
  std::vector<bool> _paramDeref;

 public:
  static bool accept(const type *D);
  FunctionJob(type *D, clang::Sema &S, JobManager &manager);
  void impl() override;
};

}  // namespace unplusplus
