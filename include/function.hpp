/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include "decls.hpp"

namespace unplusplus {
struct FunctionDeclWriter : public DeclWriter<clang::FunctionDecl> {
  FunctionDeclWriter(const type *d, clang::Sema &S, DeclHandler &dh);
};
}  // namespace unplusplus
