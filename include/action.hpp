/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/Frontend/FrontendAction.h>
#include <clang/Index/IndexDataConsumer.h>
#include <clang/Tooling/Tooling.h>

#include <memory>

#include "decls.hpp"
#include "outputs.hpp"

namespace unplusplus {
class IndexActionFactory : public clang::tooling::FrontendActionFactory {
  Outputs &_out;

 public:
  IndexActionFactory(Outputs &out) : _out(out) {}

  std::unique_ptr<clang::FrontendAction> create() override;
};
}  // namespace unplusplus
