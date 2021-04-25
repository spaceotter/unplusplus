/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/AST/Decl.h>

#include "outputs.hpp"

namespace unplusplus {
void handle_decl(const clang::NamedDecl *d, Outputs &out);
}
