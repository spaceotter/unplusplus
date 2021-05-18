/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include "decls.hpp"

namespace unplusplus {
struct EnumDeclWriter : public DeclWriter<clang::EnumDecl> {
  EnumDeclWriter(const type *d, DeclHandler &dh);
};
}  // namespace unplusplus
