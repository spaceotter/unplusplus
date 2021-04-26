/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "decls.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/TypeSwitch.h>

#include "identifier.hpp"

using namespace unplusplus;
using namespace clang;

void DeclWriterBase::forward(const Type *t) {
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

template <class T>
struct DeclWriter : public DeclWriterBase {
  typedef T type;
  const T *_d;
  const Identifier _i;

  DeclWriter(const T *d, DeclHandler &dh) : DeclWriterBase(dh), _d(d), _i(d, _out.cfg()) {}

  void preamble(Outputs &out) {
    std::string location = _d->getLocation().printToString(_d->getASTContext().getSourceManager());
    out.hf() << "// location: " << location << "\n";
    out.hf() << "// C++ name: " << _i.cpp << "\n";
    out.sf() << "// location: " << location << "\n";
    out.sf() << "// C++ name: " << _i.cpp << "\n";
  }
};

struct TypedefDeclWriter : public DeclWriter<TypedefDecl> {
  TypedefDeclWriter(const type *d, DeclHandler &dh) : DeclWriter(d, dh) {
    SubOutputs out(_out);
    preamble(out);

    // need to add a forward declaration if the target type is a struct - it may not have been
    // declared already.
    const Type *t = d->getUnderlyingType().getTypePtrOrNull()->getUnqualifiedDesugaredType();
    forward(t);
    try {
      Identifier ti(t, cfg());
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
};

struct FunctionDeclWriter : public DeclWriter<FunctionDecl> {
  FunctionDeclWriter(const type *d, DeclHandler &dh) : DeclWriter(d, dh) {
    SubOutputs out(_out);
    preamble(out);

    try {
      const Type *rt = d->getReturnType().getTypePtrOrNull()->getUnqualifiedDesugaredType();
      Identifier ri(rt, cfg());
      forward(rt);
      // avoid returning structs by value?
      bool ret_param = rt->isRecordType() || rt->isReferenceType();
      Identifier parent;
      if (const auto *m = dyn_cast<CXXMethodDecl>(d)) {
        parent = Identifier(m->getParent(), cfg());
      }
      bool ctor = dyn_cast<CXXConstructorDecl>(d);
      bool dtor = dyn_cast<CXXDestructorDecl>(d);
      std::string sig_start;
      if (ret_param)
        sig_start = "void";
      else if (ctor)
        sig_start = parent.c + "*";
      else
        sig_start = ri.c;
      sig_start += " " + _i.c + "(";
      out.hf() << sig_start;
      out.sf() << sig_start;
      std::vector<std::string> args;
      args.reserve(d->param_size());
      bool first = true;
      if (ret_param) {
        out.hf() << ri.c << "* " << cfg()._return;
        out.sf() << ri.c << "* " << cfg()._return;
        first = false;
      }
      if (!parent.empty() && !ctor) {
        out.hf() << parent.c << "* " << cfg()._this;
        out.sf() << parent.c << "* " << cfg()._this;
        first = false;
      }
      for (const auto &p : d->parameters()) {
        if (!first) {
          out.hf() << ", ";
          out.sf() << ", ";
        }
        const Type *pt = p->getType().getTypePtrOrNull()->getUnqualifiedDesugaredType();
        bool ptrize = pt->isRecordType() || pt->isReferenceType();
        Identifier pi(pt, cfg());
        forward(pt);
        if (ptrize) pi.c += "*";
        std::string san = cfg().sanitize(p->getDeclName().getAsString());
        out.hf() << pi.c << " " << san;
        out.sf() << pi.c << " " << san;
        if (ptrize)
          args.push_back("*" + san);
        else
          args.push_back(san);
        first = false;
      }
      out.hf() << ");\n\n";
      out.sf() << ") {\n  ";
      if (dtor) {
        out.sf() << "delete " << cfg()._this;
      } else {
        if (ret_param)
          out.sf() << "*" << cfg()._return << " = " << _i.cpp;
        else if (ctor)
          out.sf() << "return new " << parent.cpp;
        else {
          out.sf() << "return ";
          if (parent.empty())
            out.sf() << _i.cpp;
          else
            out.sf() << cfg()._this << "->" << d->getDeclName().getAsString();
        }
        out.sf() << "(";
        first = true;
        for (const auto &a : args) {
          if (!first) out.sf() << ", ";
          out.sf() << a;
          first = false;
        }
        out.sf() << ")";
      }
      out.sf() << ";\n}\n\n";
    } catch (const mangling_error err) {
      std::cerr << "Error: " << err.what() << std::endl;
      out.hf() << "// ERROR: " << err.what() << "\n\n";
    }
  }
};

struct CXXRecordDeclWriter : public DeclWriter<CXXRecordDecl> {
  CXXRecordDeclWriter(const type *d, DeclHandler &dh) : DeclWriter(d, dh) {
    SubOutputs out(_out);
    preamble(out);
    // print only the forward declaration
    out.hf() << "#ifdef __cplusplus\n";
    out.hf() << "typedef " << _i.cpp << " " << _i.c << ";\n";
    out.hf() << "#else\n";
    out.hf() << "typedef struct " << _i.c << cfg()._struct << " " << _i.c << ";\n";
    out.hf() << "#endif // __cplusplus\n\n";
    // make sure this gets fully instantiated later
    if (dyn_cast<ClassTemplateSpecializationDecl>(d) && !_d->hasDefinition()) {
      _dh.addTemplate(_i.cpp);
    }
  }
  // writer destructors should run after forward declarations are written
  virtual ~CXXRecordDeclWriter() override {
    if (_d->hasDefinition()) {
      for (const auto method : _d->methods()) {
        _dh.add(method);
      }
      if (_d->hasDefaultConstructor() && !_d->hasUserProvidedDefaultConstructor()) {
        std::string name = _i.c;
        name.insert(cfg().root_prefix.size(), cfg().ctor);
        _out.hf() << "// Implicit constructor of " << _i.cpp << "\n";
        _out.sf() << "// Implicit constructor of " << _i.cpp << "\n";
        _out.hf() << _i.c << "* " << name << "();\n\n";
        _out.sf() << _i.c << "* " << name << "() {\n";
        _out.sf() << "  return new " << _i.cpp << "();\n}\n\n";
      }
      if (!_d->hasUserDeclaredDestructor()) {
        std::string name = _i.c;
        name.insert(cfg().root_prefix.size(), cfg().dtor);
        _out.hf() << "// Implicit destructor of " << _i.cpp << "\n";
        _out.sf() << "// Implicit destructor of " << _i.cpp << "\n";
        _out.hf() << "void " << name << "(" << _i.c << "* " << cfg()._this << ");\n\n";
        _out.sf() << "void " << name << "(" << _i.c << "* " << cfg()._this << ") {\n";
        _out.sf() << "  delete " << cfg()._this << ";\n}\n\n";
      }
    } else {
      std::string warn = "Warning: Class " + _i.cpp + " lacks a definition\n";
      std::cerr << warn;
      _out.hf() << "// " << warn << "\n";
    }
  }
};

void DeclHandler::add(const Decl *d) {
  const Decl *pd = d;
  while (pd != nullptr) {
    if (_decls.count(pd)) return;
    pd = pd->getPreviousDecl();
  }
  _decls[d] = nullptr;

  if (d->isTemplated()) return;  // ignore unspecialized template decl

  SourceManager &SM = d->getASTContext().getSourceManager();
  FileID FID = SM.getFileID(SM.getFileLoc(d->getLocation()));
  bool Invalid = false;
  const SrcMgr::SLocEntry &SEntry = SM.getSLocEntry(FID, &Invalid);
  SrcMgr::CharacteristicKind ck = SEntry.getFile().getFileCharacteristic();
  // The declaration is from a C header that can just be included by the library
  if (ck == SrcMgr::CharacteristicKind::C_ExternCSystem) {
    _out.addCHeader(SEntry.getFile().getName().str());
    return;
  }

  try {
    if (const auto *sd = dyn_cast<TypedefDecl>(d))
      _decls[d].reset(new TypedefDeclWriter(sd, *this));
    else if (const auto *sd = dyn_cast<CXXRecordDecl>(d))
      _decls[d].reset(new CXXRecordDeclWriter(sd, *this));
    else if (const auto *sd = dyn_cast<FunctionDecl>(d))
      _decls[d].reset(new FunctionDeclWriter(sd, *this));
    else if (const auto *sd = dyn_cast<FieldDecl>(d))
      ;  // Ignore, fields are handled in the respective record
    else if (const auto *sd = dyn_cast<NamespaceDecl>(d))
      for (const auto ssd : sd->decls()) add(ssd);
    else if (const auto *sd = dyn_cast<NamedDecl>(d)) {
      std::cerr << "Warning: Unknown Decl kind " << sd->getDeclKindName() << " "
                << sd->getQualifiedNameAsString() << std::endl;
      _out.hf() << "// Warning: Unknown Decl kind " << sd->getDeclKindName() << " "
                << sd->getQualifiedNameAsString() << "\n\n";
    } else {
      std::cerr << "Warning: Ignoring unnamed Decl of kind " << d->getDeclKindName() << "\n";
    }
  } catch (const mangling_error err) {
    std::string name = "<none>";
    if (const auto *sd = dyn_cast<NamedDecl>(d)) {
      name = sd->getQualifiedNameAsString();
    }
    std::cerr << "Warning: Ignoring " << err.what() << " " << name << " at "
              << d->getLocation().printToString(d->getASTContext().getSourceManager()) << "\n";
  }
}

void DeclHandler::finish() {
  for (auto &p : _decls) {
    p.second.reset();
  }
}
