/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <memory>

#include <clang/Index/IndexDataConsumer.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include "outputs.hpp"

namespace unplusplus {
class IndexDataConsumer : public clang::index::IndexDataConsumer {
  Outputs &_outs;

 public:
  IndexDataConsumer(Outputs &outs) : _outs(outs) {}

  // Print a message to stdout if any kind of declaration is found
  bool handleDeclOccurrence(const clang::Decl *d, clang::index::SymbolRoleSet,
                            llvm::ArrayRef<clang::index::SymbolRelation>, clang::SourceLocation sl,
                            clang::index::IndexDataConsumer::ASTNodeInfo ani) override;
};

class IndexActionFactory : public clang::tooling::FrontendActionFactory {
  Outputs &_outs;

 public:
  IndexActionFactory(Outputs &outs) : _outs(outs) {}

  std::unique_ptr<clang::FrontendAction> create() override;
};
}
