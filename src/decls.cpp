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

#include <sstream>

#include "identifier.hpp"

using namespace unplusplus;
using namespace clang;

void DeclWriterBase::forward(const QualType &qt) {
  const Type *t = qt.getTypePtrOrNull()->getUnqualifiedDesugaredType();
  if (t == nullptr) {
    return;
  } else if (const auto *tt = dyn_cast<TagType>(t)) {
    _dh.add(tt->getDecl());
  } else if (const auto *pt = dyn_cast<PointerType>(t)) {
    forward(pt->getPointeeType());
  } else if (const auto *pt = dyn_cast<ReferenceType>(t)) {
    forward(pt->getPointeeType());
  } else if (const auto *pt = dyn_cast<ConstantArrayType>(t)) {
    forward(pt->getElementType());
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
    const QualType &t = d->getUnderlyingType();
    forward(t);
    try {
      out.hf() << "#ifdef __cplusplus\n";
      if (_i.cpp == _i.c) out.hf() << "// ";
      Identifier ti(t, Identifier(_i.c, cfg()), cfg());
      out.hf() << "typedef " << ti.cpp << ";\n";
      out.hf() << "#else\n";
      out.hf() << "typedef " << ti.c << ";\n";
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
      QualType qr = d->getReturnType();
      forward(qr);
      bool ret_param = qr->isRecordType() || qr->isReferenceType();
      if (ret_param) qr = _d->getASTContext().VoidTy;

      // avoid returning structs by value?
      QualType qp;
      const auto *method = dyn_cast<CXXMethodDecl>(d);
      if (method) {
        qp = _d->getASTContext().getRecordType(method->getParent());
      }
      bool ctor = dyn_cast<CXXConstructorDecl>(d);
      bool dtor = dyn_cast<CXXDestructorDecl>(d);
      if (ctor) qr = _d->getASTContext().getPointerType(qp);
      std::stringstream proto;
      std::stringstream call;
      proto << _i.c << "(";
      bool firstP = true;
      if (ret_param) {
        QualType qr = d->getReturnType();
        if (qr->isRecordType()) {
          qr = d->getASTContext().getPointerType(qr);
        } else if (qr->isReferenceType()) {
          qr = d->getASTContext().getPointerType(qr.getNonReferenceType());
        }
        proto << Identifier(qr, Identifier(cfg()._return, cfg()), cfg()).c;
        firstP = false;
      }
      if (method && !ctor) {
        if (!firstP) proto << ", ";
        proto << Identifier(_d->getASTContext().getPointerType(qp), Identifier(cfg()._this, cfg()),
                            cfg())
                     .c;
        firstP = false;
      }
      bool firstC = true;
      for (const auto &p : d->parameters()) {
        if (!firstP) proto << ", ";
        if (!firstC) call << ", ";
        QualType pt = p->getType();
        if (pt->isRecordType()) {
          pt = _d->getASTContext().getPointerType(pt);
          call << "*";
        } else if (pt->isReferenceType()) {
          pt = _d->getASTContext().getPointerType(pt.getNonReferenceType());
          call << "*";
        }
        Identifier pn(p->getDeclName().getAsString(), cfg());
        Identifier pi(pt, pn, cfg());
        forward(pt);
        proto << pi.c;
        call << pn.c;
        firstC = firstP = false;
      }
      proto << ")";
      Identifier signature(qr, Identifier(proto.str()), cfg());
      out.hf() << signature.c << ";\n\n";
      out.sf() << signature.c << " {\n  ";
      if (dtor) {
        out.sf() << "delete " << cfg()._this;
      } else {
        if (ret_param)
          out.sf() << "*" << cfg()._return << " = " << _i.cpp;
        else if (ctor)
          out.sf() << "return new " << Identifier(method->getParent(), cfg()).cpp;
        else {
          out.sf() << "return ";
          if (method)
            out.sf() << cfg()._this << "->" << d->getDeclName().getAsString();
          else
            out.sf() << _i.cpp;
        }
        out.sf() << "(";
        out.sf() << call.str();
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
    if (d->isTemplated()) return;  // ignore unspecialized template decl
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
        _out.hf() << _i.c << " *" << name << "();\n\n";
        _out.sf() << _i.c << " *" << name << "() {\n";
        _out.sf() << "  return new " << _i.cpp << "();\n}\n\n";
      }
      if (!_d->hasUserDeclaredDestructor()) {
        std::string name = _i.c;
        name.insert(cfg().root_prefix.size(), cfg().dtor);
        _out.hf() << "// Implicit destructor of " << _i.cpp << "\n";
        _out.sf() << "// Implicit destructor of " << _i.cpp << "\n";
        _out.hf() << "void " << name << "(" << _i.c << " *" << cfg()._this << ");\n\n";
        _out.sf() << "void " << name << "(" << _i.c << " *" << cfg()._this << ") {\n";
        _out.sf() << "  delete " << cfg()._this << ";\n}\n\n";
      }
    } else {
      std::string warn = "Warning: Class " + _i.cpp + " lacks a definition\n";
      std::cerr << warn;
      _out.hf() << "// " << warn << "\n";
    }
  }
};

struct FunctionTemplateDeclWriter : public DeclWriter<FunctionTemplateDecl> {
  FunctionTemplateDeclWriter(const type *d, DeclHandler &dh) : DeclWriter(d, dh) {}

  ~FunctionTemplateDeclWriter() {
    for (auto as : _d->specializations()) {
      _dh.add(as);
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
    else if (const auto *sd = dyn_cast<FunctionTemplateDecl>(d))
      _decls[d].reset(new FunctionTemplateDeclWriter(sd, *this));
    else if (const auto *sd = dyn_cast<FieldDecl>(d))
      ;  // Ignore, fields are handled in the respective record
    else if (const auto *sd = dyn_cast<ClassTemplateDecl>(d))
      ;  // Ignore, the specializations are picked up elsewhere
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
  // destructing the writers may cause more decls to be processed.
  bool any = true;
  while (any) {
    any = false;
    for (auto &p : _decls) {
      if (p.second) {
        p.second.reset();
        any = true;
      }
    }
  }
}
