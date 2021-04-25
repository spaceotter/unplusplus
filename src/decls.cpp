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
  ~DeclWriter() {}

  void preamble() {
    std::string location = _d->getLocation().printToString(_d->getASTContext().getSourceManager());
    _out.hf() << "// location: " << location << "\n";
    _out.hf() << "// C++ name: " << _i.cpp << "\n";
    _out.sf() << "// location: " << location << "\n";
    _out.sf() << "// C++ name: " << _i.cpp << "\n";
  }

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
      Identifier ti(t, _out.cfg());
      out.hf() << "#ifdef __cplusplus\n";
      if (_i.cpp == _i.c) out.hf() << "// ";
      out.hf() << "typedef " << ti.cpp << " " << _i.c << ";\n";
      out.hf() << "#else\n";
      out.hf() << "typedef " << ti.c << " " << _i.c << "\n";
      out.hf() << "#endif // __cplusplus\n\n";
    } catch (const mangling_error err) {
      std::cerr << "Error: " << err.what() << std::endl;
      out.hf() << "// ERROR: " << err.what() << "\n\n";
    }
  }
  ~TypedefDeclWriter() {}
};

struct FunctionDeclWriter : public DeclWriter<FunctionDecl> {
  FunctionDeclWriter(const type *d, DeclHandler &dh) : DeclWriter(d, dh) {
    SubOutputs out(_out);
    preamble();

    try {
      const Type *rt = d->getReturnType().getTypePtrOrNull()->getUnqualifiedDesugaredType();
      Identifier ri(rt, _out.cfg());
      // avoid returning structs by value?
      bool ret_param = rt->isRecordType() || rt->isReferenceType();
      forward(rt);
      std::string sig_start = (ret_param ? "void" : ri.c) + " " + _i.c + "(";
      out.hf() << sig_start;
      out.sf() << sig_start;
      std::vector<std::string> args;
      args.reserve(d->param_size());
      bool first = true;
      if (ret_param) {
        out.hf() << ri.c << "* " << _out.cfg()._return;
        out.sf() << ri.c << "* " << _out.cfg()._return;
        first = false;
      }
      for (const auto &p : d->parameters()) {
        if (!first) {
          out.hf() << ", ";
          out.sf() << ", ";
        }
        const Type *pt = p->getType().getTypePtrOrNull()->getUnqualifiedDesugaredType();
        bool ptrize = pt->isRecordType() || pt->isReferenceType();
        Identifier pi(pt, _out.cfg());
        if (ptrize) pi.c += "*";
        std::string san = _out.cfg().sanitize(p->getDeclName().getAsString());
        out.hf() << pi.c << " " << san;
        out.sf() << pi.c << " " << san;
        if (ptrize)
          args.push_back("*" + san);
        else
          args.push_back(san);
        first = false;
      }
      out.hf() << ");\n\n";
      out.sf() << ") {\n  " << (ret_param ? "*" + _out.cfg()._return + " = " : "return ") << _i.cpp
               << "(";
      first = true;
      for (const auto &a : args) {
        if (!first) out.sf() << ", ";
        out.sf() << a;
        first = false;
      }
      out.sf() << ");\n}\n\n";
    } catch (const mangling_error err) {
      std::cerr << "Error: " << err.what() << std::endl;
      out.hf() << "// ERROR: " << err.what() << "\n\n";
    }
  }
};

void DeclHandler::add(const NamedDecl *d) {
  if (!_decls.count(d)) {
    _decls.emplace(d);
    try {
      if (const auto *sd = dyn_cast<TypedefDecl>(d))
        TypedefDeclWriter(sd, *this);
      else if (const auto *sd = dyn_cast<FunctionDecl>(d))
        FunctionDeclWriter(sd, *this);
      else if (const auto *sd = dyn_cast<NamespaceDecl>(d)) {
        // Ignore, namespaces are handled in the Identifier class
      } else {
        std::cerr << "Warning: Unknown Decl kind " << d->getDeclKindName() << std::endl;
        _out.hf() << "// Warning: Unknown Decl kind " << d->getDeclKindName() << "\n";
      }
    } catch (const mangling_error err) {
      std::cerr << "Warning: Ignoring " << err.what() << " " << d->getQualifiedNameAsString()
                << " at " << d->getLocation().printToString(d->getASTContext().getSourceManager())
                << "\n";
    }
  }
}
