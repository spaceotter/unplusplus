/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include "jobs.hpp"

namespace unplusplus {
class EnumJob : public Job<clang::EnumDecl> {
 public:
  EnumJob(type *D, clang::Sema &S, JobManager &jm);
  void impl() override;
};
}  // namespace unplusplus
