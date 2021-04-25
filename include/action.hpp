/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/Frontend/FrontendAction.h>
#include <clang/Index/IndexDataConsumer.h>
#include <clang/Tooling/Tooling.h>

#include <memory>

#include "outputs.hpp"
#include "decls.hpp"

namespace unplusplus {
class IndexDataConsumer : public clang::index::IndexDataConsumer {
  DeclHandler &_dh;

 public:
  IndexDataConsumer(DeclHandler &dh) : _dh(dh) {}

  // Print a message to stdout if any kind of declaration is found
  bool handleDeclOccurrence(const clang::Decl *d, clang::index::SymbolRoleSet,
                            llvm::ArrayRef<clang::index::SymbolRelation>, clang::SourceLocation sl,
                            clang::index::IndexDataConsumer::ASTNodeInfo ani) override;
};

class IndexActionFactory : public clang::tooling::FrontendActionFactory {
  DeclHandler &_dh;

 public:
  IndexActionFactory(DeclHandler &dh) : _dh(dh) {}

  std::unique_ptr<clang::FrontendAction> create() override;
};
}  // namespace unplusplus
