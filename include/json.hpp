/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/AST/Decl.h>
#include <json/json.h>

#include <string>

#include "identifier.hpp"
#include "outputs.hpp"

namespace unplusplus {
struct JsonConfig {
  const IdentifierConfig &_icfg;
  const clang::ASTContext &_ac;
  const std::string _class = "class";
  const std::string _function = "function";
  const std::string _cname = "cname";
  const std::string _qname = "qname";
  const std::string _const = "const";
  const std::string _typeKind = "kind";
  const std::string _typeName = "name";
  const std::string _builtinFloat = "float";
  const std::string _builtinSize = "bits";
  const std::string _builtinSigned = "signed";
  const std::string _pointee = "pointee";
  const std::string _element = "element";
  const std::string _return = "return";
  const std::string _args = "args";
  const std::string _location = "location";
  const std::string _union = "union";
  const std::string _fields = "fields";
  const std::string _fieldName = "name";
  const std::string _fieldBits = "bits";
  const std::string _fieldType = "type";
  const std::string _variadic = "variadic";

  JsonConfig(const IdentifierConfig &IC, const clang::ASTContext &AC, Outputs &Out);
  Json::Value jsonType(const clang::QualType &QT);
  Json::Value jsonQName(const clang::Decl *D);
};
}  // namespace unplusplus
