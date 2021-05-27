/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/AST/Decl.h>
#include <clang/Sema/Sema.h>

#include <queue>

#include "outputs.hpp"

namespace unplusplus {
class DeclWriterBase;

class DeclHandler {
  Outputs &_out;
  std::unordered_set<const clang::Decl *> _decls;
  std::unordered_set<const clang::NamedDecl *> _renamedInternals;
  std::unordered_map<const clang::ClassTemplateSpecializationDecl *,
                     std::unique_ptr<DeclWriterBase>>
      _ctsd;
  std::queue<std::unique_ptr<DeclWriterBase>> _unfinished;
  std::queue<const clang::TemplateDecl *> _templates;
  IdentifierConfig _cfg;

 public:
  DeclHandler(Outputs &out, const clang::ASTContext &_astc) : _out(out), _cfg(_astc) {}
  Outputs &out() { return _out; }
  const IdentifierConfig &cfg() const { return _cfg; }

  // Emit the forward declaration, then finish the declaration.
  void add(const clang::Decl *d, clang::Sema &S);

  // Emit fully instantiated template specializations, and any additional specializations that were
  // discovered while emitting the known ones.
  void finishTemplates(clang::Sema &S);

  // Ensure that a type is declared already. Emit the forward declaration if there is one.
  void forward(const clang::QualType &t, clang::Sema &S);

  // Emit only the forward declaration and save it for later.
  void forward(const clang::Decl *d, clang::Sema &S);

  // Emit the forward declarations for the template arguments.
  void forward(const llvm::ArrayRef<clang::TemplateArgument> &Args, clang::Sema &S);

  // Rename the standard library internal with Identifier
  bool renameInternal(const clang::NamedDecl *d, const Identifier &i);

  // Is this a renamed library internal declaration?
  bool isRenamedInternal(const clang::NamedDecl *d);
};

// The base class for Decl handlers that write to the output files.
class DeclWriterBase {
 protected:
  DeclHandler &_dh;
  Outputs &_out;

 public:
  // The constructor is supposed to emit only the forward declaration.
  DeclWriterBase(DeclHandler &dh) : _dh(dh), _out(dh.out()) {}
  // The destructor emits any additional code other than the forward declaration.
  virtual ~DeclWriterBase() {}

  const IdentifierConfig &cfg() const { return _dh.cfg(); }
  const Outputs &out() const { return _out; }
};

// A helper template for DeclWriters that use a NamedDecl and an Identifier.
template <class T>
class DeclWriter : public DeclWriterBase {
 protected:
  typedef T type;
  const T *_d;
  Identifier _i;

 public:
  DeclWriter(const T *d, DeclHandler &dh, bool id = true) : DeclWriterBase(dh), _d(d) {
    if (id) _i = Identifier(d, _dh.cfg());
  }
  const T *decl() const { return _d; }

  void preamble(std::ostream &out) {
    std::string location = _d->getLocation().printToString(_d->getASTContext().getSourceManager());
    out << "// location: " << location << "\n";
    out << "// C++ name: " << _i.cpp << "\n";
  }
};
}  // namespace unplusplus
