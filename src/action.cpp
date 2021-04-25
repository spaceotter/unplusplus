/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "action.hpp"

#include <clang/Index/IndexingAction.h>
#include <clang/Index/IndexingOptions.h>

#include "decls.hpp"

using namespace unplusplus;
using namespace clang;

class UppASTConsumer : public ASTConsumer {
  DeclHandler &_dh;

 public:
  UppASTConsumer(DeclHandler &dh) : _dh(dh) {}

 protected:
  bool HandleTopLevelDecl(DeclGroupRef DG) override {
    for (auto d : DG) {
      _dh.add(d);
    }
    return true;
  }

  bool shouldSkipFunctionBody(Decl *D) override { return true; }
};

class UppAction : public ASTFrontendAction {
  DeclHandler &_dh;

 public:
  UppAction(DeclHandler &dh) : _dh(dh) {}

 protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef InFile) override {
    return std::make_unique<UppASTConsumer>(_dh);
  }
};

std::unique_ptr<clang::FrontendAction> IndexActionFactory::create() {
  return std::make_unique<UppAction>(_dh);
}
