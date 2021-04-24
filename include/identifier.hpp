/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <string>

namespace unplusplus {

struct IdentifierConfig {
  // these determine how flattened C names are assembled
  std::string root_prefix = "upp_";
  std::string c_separator = "_";
  std::string _this = "_upp_this";
  std::string _return = "_upp_return";
  std::string _struct = "_s_";
  std::string dtor = "_dtor";
  std::string ctor = "_ctor";
  std::string sanitize(const std::string &name) const;
};
}
