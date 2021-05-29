/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "action.hpp"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Index/IndexingAction.h>
#include <clang/Index/IndexingOptions.h>

#include "jobs.hpp"
#include "decls.hpp"

using namespace unplusplus;
using namespace clang;

class UppASTConsumer : public ASTConsumer {
  JobManager &_jm;
  DeclHandler &_dh;
  CompilerInstance &_CI;

 public:
  UppASTConsumer(JobManager &jm, DeclHandler &dh, CompilerInstance &CI) : _jm(jm), _dh(dh), _CI(CI) {}

 protected:
  bool HandleTopLevelDecl(DeclGroupRef DG) override {
    for (auto d : DG) {
      _jm.create(d, _CI.getSema());
      _jm.flush();
      //_dh.add(d, _CI.getSema());
    }
    return true;
  }

  void HandleTranslationUnit(ASTContext &Ctx) override {}

  bool shouldSkipFunctionBody(Decl *D) override { return true; }
};

class UppAction : public ASTFrontendAction {
  Outputs &_out;
  std::unique_ptr<DeclHandler> _dh;
  std::unique_ptr<JobManager> _jm;

 public:
  UppAction(Outputs &out) : _out(out) {}
  virtual void ExecuteAction() override {
    ASTFrontendAction::ExecuteAction();
    CompilerInstance &CI = getCompilerInstance();
    //_dh->finishTemplates(CI.getSema());
    _jm->finishTemplates(CI.getSema());
  }

 protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef InFile) override {
    _jm = std::make_unique<JobManager>(_out, CI.getASTContext());
    _dh = std::make_unique<DeclHandler>(_out, CI.getASTContext());
    return std::make_unique<UppASTConsumer>(*_jm, *_dh, getCompilerInstance());
  }
};

std::unique_ptr<clang::FrontendAction> IndexActionFactory::create() {
  return std::make_unique<UppAction>(_out);
}
