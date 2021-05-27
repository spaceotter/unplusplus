/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "action.hpp"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Index/IndexingAction.h>
#include <clang/Index/IndexingOptions.h>

#include "decls.hpp"

using namespace unplusplus;
using namespace clang;

class UppASTConsumer : public ASTConsumer {
  DeclHandler &_dh;
  CompilerInstance &_CI;

 public:
  UppASTConsumer(DeclHandler &dh, CompilerInstance &CI) : _dh(dh), _CI(CI) {}

 protected:
  bool HandleTopLevelDecl(DeclGroupRef DG) override {
    for (auto d : DG) {
      _dh.add(d, _CI.getSema());
    }
    return true;
  }

  void HandleTranslationUnit(ASTContext &Ctx) override {}

  bool shouldSkipFunctionBody(Decl *D) override { return true; }
};

class UppAction : public ASTFrontendAction {
  Outputs &_out;
  std::unique_ptr<DeclHandler> _dh;

 public:
  UppAction(Outputs &out) : _out(out) {}
  virtual void ExecuteAction() override {
    ASTFrontendAction::ExecuteAction();
    CompilerInstance &CI = getCompilerInstance();
    _dh->finishTemplates(CI.getSema());
  }

 protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef InFile) override {
    _dh = std::make_unique<DeclHandler>(_out, CI.getASTContext());
    return std::make_unique<UppASTConsumer>(*_dh, getCompilerInstance());
  }
};

std::unique_ptr<clang::FrontendAction> IndexActionFactory::create() {
  return std::make_unique<UppAction>(_out);
}
