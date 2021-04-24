/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "identifier.hpp"
#include <unordered_map>

using namespace unplusplus;

// clang-format off
std::unordered_map<std::string, std::string> operator_map = {
  {"+", "add"},
  {"*", "mul"},
  {"/", "div"},
  {"-", "sub"},
  {"=", "set"},
  {"()", "call"},
  {"[]", "idx"},
  {" ", ""},
  {".", "dot"}
};
// clang-format on

std::string IdentifierConfig::sanitize(const std::string &name) const {
  std::string result(name);
  std::string::size_type s;
  for (auto &pair : operator_map) {
    std::string replace = c_separator + pair.second + c_separator;
    while (1) {
      s = result.find(pair.first);
      if (s == std::string::npos) {
        break;
      }
      result.replace(s, pair.first.size(), replace);
    }
  }
  return result;
}
