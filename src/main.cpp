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
#include "identifier.hpp"
#include "options.hpp"
#include "outputs.hpp"

using namespace clang;
using namespace llvm;
using namespace unplusplus;
using std::filesystem::path;

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
    stem += ".clib";
  } else {
    stem = path(OutStem.getValue());
  }
  DeclFilterConfig FC;
  if (!ExcludesFile.empty()) {
    FC.exclusion_file = path(ExcludesFile.getValue());
  }
  for (auto &s : CHeadersFiles) {
    FC.cheader_files.push_back(path(s));
  }
  FC.exclude_decls = ExcludeDecl;
  FC.no_deprecated = NoDeprecated;
  std::cout << "Writing library to: " << stem.string() << ".*" << std::endl;
  FileOutputs fout(stem, sources);
  UppActionFactory Factory(fout, FC);
  int ret = Tool.run(&Factory);
  return ret;
}
