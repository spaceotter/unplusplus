/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/Frontend/FrontendAction.h>
#include <clang/Index/IndexDataConsumer.h>
#include <clang/Tooling/Tooling.h>

#include <memory>

#include "filter.hpp"
#include "outputs.hpp"

namespace unplusplus {
class UppActionFactory : public clang::tooling::FrontendActionFactory {
  Outputs &_out;
  DeclFilterConfig &_fc;

 public:
  UppActionFactory(Outputs &out, DeclFilterConfig &FC) : _out(out), _fc(FC) {}

  std::unique_ptr<clang::FrontendAction> create() override;
};
}  // namespace unplusplus
