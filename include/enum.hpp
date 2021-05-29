/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include "decls.hpp"
#include "jobs.hpp"

namespace unplusplus {
struct EnumDeclWriter : public DeclWriter<clang::EnumDecl> {
  EnumDeclWriter(const type *d, DeclHandler &dh);
};

class EnumJob : public Job<clang::EnumDecl> {
 public:
  EnumJob(type *D, clang::Sema &S, JobManager &jm);
  void impl() override;
};
}  // namespace unplusplus
