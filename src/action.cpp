/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "action.hpp"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Index/IndexingAction.h>
#include <clang/Index/IndexingOptions.h>

#include "decls.hpp"
#include "iparseast.hpp"

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

  void HandleTranslationUnit(ASTContext &Ctx) override {}

  bool shouldSkipFunctionBody(Decl *D) override { return true; }
};

class UppAction : public ASTFrontendAction {
  DeclHandler &_dh;

 public:
  UppAction(DeclHandler &dh) : _dh(dh) {}
  virtual void ExecuteAction() override {
    ASTFrontendAction::ExecuteAction();
    CompilerInstance &CI = getCompilerInstance();
    std::string T = _dh.templates();
    IncrementalParseAST(CI.getSema(), T, "<templates>", CI.getFrontendOpts().ShowStats,
                        CI.getFrontendOpts().SkipFunctionBodies);
    _dh.finish();
  }

 protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef InFile) override {
    return std::make_unique<UppASTConsumer>(_dh);
  }
};

std::unique_ptr<clang::FrontendAction> IndexActionFactory::create() {
  return std::make_unique<UppAction>(_dh);
}
