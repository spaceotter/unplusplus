/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include "jobs.hpp"

namespace unplusplus {
class FunctionJob : public Job<clang::FunctionDecl> {
  std::vector<clang::QualType> _paramTypes;
  clang::QualType _returnType;
  clang::QualType _returnParamType;
  bool _returnParam;
  std::vector<bool> _paramDeref;

 public:
  static bool accept(const type *D);
  FunctionJob(type *D, clang::Sema &S, JobManager &manager);
  void impl() override;
};

}  // namespace unplusplus
