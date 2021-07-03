/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "options.hpp"

using namespace llvm;

cl::OptionCategory UppCategory("unplusplus options");

cl::opt<std::string> OutStem("o", cl::desc("Output files base name"), cl::Optional,
                             cl::cat(UppCategory), cl::sub(*cl::AllSubCommands));

cl::opt<std::string> ExcludesFile("excludes-file", cl::desc("File of declarations to exclude"),
                                  cl::Optional, cl::cat(UppCategory), cl::sub(*cl::AllSubCommands));

cl::list<std::string> CHeadersFiles("cheaders-file",
                                    cl::desc("File listing which headers are in C"), cl::ZeroOrMore,
                                    cl::cat(UppCategory), cl::sub(*cl::AllSubCommands));

cl::list<std::string> ExcludeDecl("e", cl::desc("Exclude the fully qualified declaration"),
                                  cl::ZeroOrMore, cl::cat(UppCategory),
                                  cl::sub(*cl::AllSubCommands));

cl::opt<bool> NoDeprecated("no-deprecated",
                           cl::desc("Exclude deprecated or unavailable declarations"), cl::Optional,
                           cl::cat(UppCategory), cl::sub(*cl::AllSubCommands));

cl::opt<bool> Verbose(
    "v", cl::desc("Enable verbose output for debugging (multiple lines per declaration)"),
    cl::Optional, cl::cat(UppCategory), cl::sub(*cl::AllSubCommands));
