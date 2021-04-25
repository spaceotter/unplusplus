/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "decls.hpp"

#include <clang/AST/ASTContext.h>
#include <llvm/ADT/TypeSwitch.h>

#include "identifier.hpp"

using namespace unplusplus;
using namespace clang;

template <class T>
struct DeclWriter {
  typedef T type;
  const T *_d;
  DeclHandler &_dh;
  Outputs &_out;
  const Identifier _i;

  DeclWriter(const T *d, DeclHandler &dh) : _d(d), _dh(dh), _out(dh.out()), _i(d, _out.cfg()) {}

  void preamble() {
    _out.hf() << "// location: "
              << _d->getLocation().printToString(_d->getASTContext().getSourceManager()) << "\n";
    _out.hf() << "// C++ name: " << _i.cpp << "\n";
  }
  ~DeclWriter() {}

  // Ensure that the type is declared already
  void forward(const Type *t) {
    if (t == nullptr) {
      return;
    } else if (const auto *tt = dyn_cast<TagType>(t)) {
      _dh.add(tt->getDecl());
    } else if (const auto *pt = dyn_cast<PointerType>(t)) {
      forward(pt->getPointeeType().getTypePtrOrNull());
    } else if (const auto *pt = dyn_cast<ReferenceType>(t)) {
      forward(pt->getPointeeType().getTypePtrOrNull());
    }
  }
};

struct TypedefDeclWriter : public DeclWriter<TypedefDecl> {
  TypedefDeclWriter(const type *d, DeclHandler &dh) : DeclWriter(d, dh) {
    SubOutputs out(_out);
    preamble();

    // need to add a forward declaration if the target type is a struct - it may not have been
    // declared already.
    const Type *t = d->getUnderlyingType().getTypePtrOrNull();
    forward(t);
    try {
      Identifier ti(t, out.cfg());
      out.hf() << "#ifdef __cplusplus\n";
      if (_i.cpp == _i.c) out.hf() << "// ";
      out.hf() << "typedef " << ti.cpp << " " << _i.c << ";\n";
      out.hf() << "#else\n";
      out.hf() << "typedef " << ti.c << " " << _i.c << "\n";
      out.hf() << "#endif // __cplusplus\n\n";
    } catch (const mangling_error err) {
      std::cerr << "Error: " << err.what() << std::endl;
      out.hf() << "// ERROR: " << err.what() << "\n";
    }
  }
  ~TypedefDeclWriter() {}
};

static void handle(const TypedefDecl *d, const Identifier &i, Outputs &out) {}

void DeclHandler::add(const NamedDecl *d) {
  if (!_decls.count(d)) {
    _decls.emplace(d);
    _out.hf() << "// Decl kind: " << d->getDeclKindName() << "\n";
    try {
      llvm::TypeSwitch<const NamedDecl *>(d).Case(
          [&](const TypedefDecl *nd) { TypedefDeclWriter(nd, *this); });
    } catch (const mangling_error err) {
      std::cerr << "Warning: Ignoring " << err.what() << " " << d->getQualifiedNameAsString()
                << " at " << d->getLocation().printToString(d->getASTContext().getSourceManager())
                << "\n";
    }
  }
}
