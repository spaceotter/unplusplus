/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "decls.hpp"

#include <llvm/ADT/TypeSwitch.h>
#include <clang/AST/ASTContext.h>

#include "identifier.hpp"

using namespace unplusplus;
using namespace clang;

struct DeclWriter {
  const NamedDecl *_d;
  const Identifier _i;
  SubOutputs _out;
  DeclWriter(const NamedDecl *d, Outputs &out) : _d(d), _i(d, out.cfg()), _out(out) {
    out.hf() << "// location: "
             << d->getLocation().printToString(d->getASTContext().getSourceManager()) << "\n";
    out.hf() << "// C++ name: " << _i.cpp << "\n";
  }
  ~DeclWriter() { _out.hf() << "// end decl\n"; }
};

struct TypedefDeclWriter : public DeclWriter {
  TypedefDeclWriter(const NamedDecl *d, Outputs &out) : DeclWriter(d, out) {
    out.hf() << "#ifdef __cplusplus\n";
    if (_i.cpp == _i.c) _out.hf() << "// ";
    _out.hf() << "typedef " << _i.cpp << " " << _i.c << ";\n";
    _out.hf() << "#else\n";
  }
  ~TypedefDeclWriter() { _out.hf() << "#endif // __cplusplus\n\n"; }
};

static void handle(const TypedefDecl *d, const Identifier &i, Outputs &out) {}

void unplusplus::handle_decl(const NamedDecl *d, Outputs &out) {
  static std::unordered_set<const NamedDecl *> seen;
  if (!seen.count(d)) {
    out.hf() << "// Decl kind: " << d->getDeclKindName() << "\n";
    try {
      llvm::TypeSwitch<const NamedDecl *>(d).Case(
          [&](const TypedefDecl *nd) { TypedefDeclWriter(nd, out); });
    } catch (const mangling_error err) {
      std::cerr << "Warning: Ignoring " << err.what() << " " << d->getQualifiedNameAsString()
                << " at " << d->getLocation().printToString(d->getASTContext().getSourceManager())
                << "\n";
    }
  }
  seen.emplace(d);
}
