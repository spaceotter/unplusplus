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
  std::queue<std::unique_ptr<DeclWriterBase>> _unfinished;
  std::queue<const clang::TemplateDecl *> _templates;
  std::queue<std::unique_ptr<DeclWriterBase>> _unfinished_templates;
  IdentifierConfig _cfg;

 public:
  DeclHandler(Outputs &out, const clang::ASTContext &_astc) : _out(out), _cfg(_astc) {}
  Outputs &out() { return _out; }
  const IdentifierConfig &cfg() const { return _cfg; }

  // Emit the forward declaration, then finish the declaration.
  void add(const clang::Decl *d);

  // Emit fully instantiated template specializations, and any additional specializations that were
  // discovered while emitting the known ones.
  void finishTemplates(clang::Sema &S);

  // Ensure that a type is declared already. Emit the forward declaration if there is one.
  void forward(const clang::QualType &t);

  // Emit only the forward declaration and save it for later.
  void forward(const clang::Decl *d);

  // Emit the forward declarations for the template arguments.
  void forward(const llvm::ArrayRef<clang::TemplateArgument> &Args);

  // Rename the standard library internal with Identifier
  bool renameInternal(const clang::NamedDecl *d, const Identifier &i);
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
  DeclWriter(const T *d, DeclHandler &dh) : DeclWriterBase(dh), _d(d), _i(d, _dh.cfg()) {}
  const T *decl() const { return _d; }

  void preamble(std::ostream &out) {
    std::string location = _d->getLocation().printToString(_d->getASTContext().getSourceManager());
    out << "// location: " << location << "\n";
    out << "// C++ name: " << _i.cpp << "\n";
  }
};

// Does the QualType use declarations that are private
bool isAccessible(clang::QualType QT);
// Is any component of the Declaration's name private
bool isAccessible(const clang::Decl *d);
}  // namespace unplusplus
