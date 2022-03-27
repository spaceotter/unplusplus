/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "json.hpp"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>

#include "identifier.hpp"

using namespace unplusplus;
using namespace clang;

JsonConfig::JsonConfig(const IdentifierConfig &IC, const ASTContext &AC, Outputs &Out)
    : _icfg(IC), _ac(AC) {
  Json::Value &root = Out.json();
  root[_class] = Json::Value(Json::ValueType::objectValue);
  root[_function] = Json::Value(Json::ValueType::objectValue);
}

Json::Value JsonConfig::jsonType(const QualType &QT) {
  Json::Value v(Json::ValueType::objectValue);
  v[_const] = QT.isLocalConstQualified();
  const Type *T = QT->getUnqualifiedDesugaredType();
  v[_typeKind] = T->getTypeClassName();
  if (const auto *ST = dyn_cast<TagType>(T)) {
    v[_typeName] = Identifier(ST->getDecl(), _icfg).cpp;
    if (T->getTypeClass() == Type::TypeClass::Enum) {
      v[_builtinFloat] = false;
      v[_builtinSigned] = ST->isSignedIntegerType();
      v[_builtinSize] = _ac.getTypeSize(ST);
      v[_builtinChar] = ST->isCharType();
    }
  } else if (const auto *ST = dyn_cast<BuiltinType>(T)) {
    v[_builtinFloat] = ST->isFloatingPoint();
    v[_builtinSigned] = ST->isSignedIntegerType() || ST->isFloatingPoint();
    v[_builtinSize] = _ac.getTypeSize(ST);
    v[_builtinChar] = ST->isCharType();
  } else if (const auto *ST = dyn_cast<PointerType>(T)) {
    v[_pointee] = jsonType(ST->getPointeeType());
  } else if (const auto *ST = dyn_cast<ConstantArrayType>(T)) {
    v[_element] = jsonType(ST->getElementType());
  } else if (const auto *ST = dyn_cast<FunctionProtoType>(T)) {
    v[_return] = jsonType(ST->getReturnType());
    Json::Value args(Json::ValueType::arrayValue);
    for (size_t i = 0; i < ST->getNumParams(); i++) {
      args.append(jsonType(ST->getParamType(i)));
    }
    v[_args] = args;
    v[_variadic] = ST->isVariadic();
  }
  return v;
}

Json::Value JsonConfig::jsonQName(const Decl *D) {
  Json::Value v(Json::ValueType::arrayValue);
  const Decl *DC = D;
  while (DC) {
    if (const auto *CTSD = dyn_cast<ClassTemplateSpecializationDecl>(DC)) {
      Json::Value t(Json::ValueType::arrayValue);
      for (const auto &Arg : CTSD->getTemplateArgs().asArray()) {
        switch (Arg.getKind()) {
          case TemplateArgument::Type:
            t.append(jsonType(Arg.getAsType()));
            break;

          default:
            std::string Buf;
            llvm::raw_string_ostream ArgOS(Buf);
            Arg.print(_icfg.PP, ArgOS, true);
            ArgOS.flush();
            t.append(ArgOS.str());
            break;
        }
      }
      v.insert(0, t);
    }
    if (isa<NamedDecl>(DC)) {
      v.insert(0, Json::Value(getName(DC)));
    }
    DC = dyn_cast_or_null<Decl>(DC->getDeclContext());
  }
  return v;
}
