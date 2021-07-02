/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <llvm/Support/CommandLine.h>

extern llvm::cl::OptionCategory UppCategory;

extern llvm::cl::opt<std::string> OutStem;
extern llvm::cl::opt<std::string> ExcludesFile;
extern llvm::cl::list<std::string> ExcludeDecl;
extern llvm::cl::list<std::string> CHeadersFiles;
extern llvm::cl::opt<bool> NoDeprecated;
extern llvm::cl::opt<bool> Verbose;
