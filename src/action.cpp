/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "action.hpp"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Index/IndexingAction.h>
#include <clang/Index/IndexingOptions.h>

#include "jobs.hpp"

using namespace unplusplus;
using namespace clang;

class UppASTConsumer : public ASTConsumer {
  JobManager &_jm;
  CompilerInstance &_CI;

 public:
  UppASTConsumer(JobManager &jm, CompilerInstance &CI) : _jm(jm), _CI(CI) {}

 protected:
  bool HandleTopLevelDecl(DeclGroupRef DG) override {
    for (auto d : DG) {
      _jm.create(d, _CI.getSema());
      _jm.flush(_CI.getSema());
    }
    return true;
  }

  void HandleTranslationUnit(ASTContext &Ctx) override {}

  bool shouldSkipFunctionBody(Decl *D) override { return true; }
};

class UppAction : public ASTFrontendAction {
  Outputs &_out;
  DeclFilterConfig &_fc;
  std::unique_ptr<JobManager> _jm;

 public:
  UppAction(Outputs &out, DeclFilterConfig &FC) : _out(out), _fc(FC) {}
  virtual void ExecuteAction() override {
    ASTFrontendAction::ExecuteAction();
    CompilerInstance &CI = getCompilerInstance();
    _jm->visitMacros(CI.getPreprocessor());
    _jm->finishTemplates(CI.getSema());
  }

 protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef InFile) override {
    _jm = std::make_unique<JobManager>(_out, CI.getASTContext(), _fc);
    return std::make_unique<UppASTConsumer>(*_jm, getCompilerInstance());
  }
};

std::unique_ptr<clang::FrontendAction> UppActionFactory::create() {
  return std::make_unique<UppAction>(_out, _fc);
}
