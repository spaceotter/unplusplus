/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "action.hpp"
#include "decls.hpp"
#include "identifier.hpp"
#include "outputs.hpp"

using namespace clang;
using namespace llvm;
using namespace unplusplus;
using std::filesystem::path;

static cl::OptionCategory UppCategory("unplusplus options");
static cl::opt<std::string> OutStem("o", cl::desc("Output files base name"), cl::Optional,
                                    cl::cat(UppCategory), cl::sub(*cl::AllSubCommands));

int main(int argc, const char **argv) {
  std::vector<const char *> args(argv, argv + argc);
  std::cout << "clang resource dir: " << CLANG_RESOURCE_DIRECTORY << std::endl;
  args.push_back("--extra-arg-before=-resource-dir=" CLANG_RESOURCE_DIRECTORY);
  args.push_back("--");
  args.push_back("clang++");
  args.push_back("-c");
  int size = args.size();
  tooling::CommonOptionsParser OptionsParser(size, args.data(), UppCategory);
  std::vector<std::string> sources = OptionsParser.getSourcePathList();
  tooling::ClangTool Tool(OptionsParser.getCompilations(), sources);
  path stem;
  if (OutStem.empty()) {
    stem = path(sources[0]).stem();
  } else {
    stem = path(OutStem.getValue());
  }
  std::cout << "Writing library to: " << stem.string() << ".*" << std::endl;
  IdentifierConfig icfg;
  FileOutputs fout(stem, sources, icfg);
  SubOutputs temp(fout);
  DeclHandler dh(temp);
  IndexActionFactory Factory(dh);
  int ret = Tool.run(&Factory);
  fout.finalize();
  return ret;
}
