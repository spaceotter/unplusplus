/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "cxxrecord.hpp"

#include <clang/AST/RecordLayout.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>

using namespace unplusplus;
using namespace clang;

CXXRecordDeclWriter::CXXRecordDeclWriter(const type *d, DeclHandler &dh) : DeclWriter(d, dh) {
  if (d->isTemplated()) return;  // ignore unspecialized template decl
  SubOutputs out(_out);
  preamble(out.hf());
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
  if (_d->hasDefinition()) {
    writeMembers();
  }
}

void CXXRecordDeclWriter::writeFields(Outputs &out, const CXXRecordDecl *d) {
  const ASTRecordLayout &layout = d->getASTContext().getASTRecordLayout(d);

  for (const auto *f : d->fields()) {
    QualType qt = f->getType();
    _dh.forward(qt);

    std::string name = getName(f);
    if (_fields.count(name)) {
      int i = 2;
      while (_fields.count(name + cfg().c_separator + std::to_string(i))) i++;
      name += cfg().c_separator + std::to_string(i);
    }

    Identifier fi(qt, Identifier(name, cfg()), cfg());
    out.hf() << "  " << fi.c;
    if (f->isBitField()) {
      out.hf() << " : " << f->getBitWidthValue(d->getASTContext());
    }
    std::string location = f->getLocation().printToString(d->getASTContext().getSourceManager());
    out.hf() << "; // " << f->getQualifiedNameAsString() << " at: " << location << "\n";
    _fields.emplace(name);
  }
}

void CXXRecordDeclWriter::writeNonVirtualBases(Outputs &out, const CXXRecordDecl *d) {
  const ASTRecordLayout &layout = d->getASTContext().getASTRecordLayout(d);
  const CXXRecordDecl *PrimaryBase = layout.getPrimaryBase();
  if (PrimaryBase) {
    writeNonVirtualBases(out, PrimaryBase);
    writeFields(out, PrimaryBase);
    if (layout.isPrimaryBaseVirtual()) {
      _indirect.insert(PrimaryBase);
      _vbases.emplace(PrimaryBase);
    }
  } else if (d->isDynamicClass()) {
    std::string vtable = cfg().getCName(d, false) + "_vtable";
    if (_fields.count(vtable)) {
      int i = 2;
      while (_fields.count(vtable + cfg().c_separator + std::to_string(i))) i++;
      vtable += cfg().c_separator + std::to_string(i);
    }
    Identifier vti(d->getASTContext().VoidPtrTy, Identifier(vtable), cfg());
    out.hf() << "  " << vti.c << ";\n";
    _fields.emplace(vtable);
  }

  for (const auto base : d->bases()) {
    if (base.isVirtual()) continue;
    const CXXRecordDecl *based = base.getType()->getAsCXXRecordDecl();
    if (based == PrimaryBase && !layout.isPrimaryBaseVirtual()) continue;
    writeNonVirtualBases(out, based);
    writeFields(out, based);
  }
}

void CXXRecordDeclWriter::writeVirtualBases(Outputs &out, const CXXRecordDecl *d) {
  const ASTRecordLayout &layout = d->getASTContext().getASTRecordLayout(d);
  const CXXRecordDecl *PrimaryBase = layout.getPrimaryBase();
  for (const auto base : d->bases()) {
    const CXXRecordDecl *based = base.getType()->getAsCXXRecordDecl();
    if (base.isVirtual()) {
      if (!(based == PrimaryBase && layout.isPrimaryBaseVirtual())) {
        if (!_indirect.count(based)) {
          if (!_vbases.insert(based).second) continue;
          writeNonVirtualBases(out, based);
          writeFields(out, based);
        }
      }
    }
    if (!based->getNumVBases()) continue;
    writeVirtualBases(out, based);
  }
}

void CXXRecordDeclWriter::writeMembers() {
  if (_wroteMembers) return;
  SubOutputs out(_out);
  out.hf() << "// Members of type " << _i.cpp << "\n";
  out.hf() << "struct " << _i.c << cfg()._struct << " {\n";
  // The procedure here has to mimic clang's RecordLayoutBuilder.cpp to order the fields of the base
  // classes correctly. It may not work with the Microsoft ABI.
  _d->getIndirectPrimaryBases(_indirect);
  writeNonVirtualBases(out, _d);
  writeFields(out, _d);
  writeVirtualBases(out, _d);
  out.hf() << "};\n";
  out.hf() << "#ifdef __cplusplus\n";
  out.hf() << "static_assert(sizeof(struct " << _i.c << cfg()._struct << ") == sizeof(" << _i.cpp
           << "), \"Size of C struct must match C++\");\n";
  out.hf() << "#else\n";
  out.hf() << "_Static_assert(sizeof(struct " << _i.c << cfg()._struct
           << ") == " << _d->getASTContext().getTypeSizeInChars(_d->getTypeForDecl()).getQuantity()
           << ", \"Size of C struct must match C++\");\n";
  out.hf() << "#endif\n\n";
  _wroteMembers = true;
}

// writer destructors should run after forward declarations are written
CXXRecordDeclWriter::~CXXRecordDeclWriter() {
  if (_d->hasDefinition()) {
    writeMembers();

    SubOutputs out(_out);
    bool any_ctor = false;
    bool any_dtor = false;
    for (const auto d : _d->decls()) {
      if (d->getAccess() == AccessSpecifier::AS_public ||
          d->getAccess() == AccessSpecifier::AS_none) {
        if (const auto *nd = dyn_cast<NamedDecl>(d)) {
          // Drop it if this decl will be ambiguous with a constructor
          if (getName(nd) == getName(_d) && !isa<CXXConstructorDecl>(nd)) continue;
        }
        _dh.add(d);
      }
      if (isa<CXXConstructorDecl>(d)) any_ctor = true;
      if (isa<CXXDestructorDecl>(d)) any_dtor = true;
    }
    if (!any_ctor && _d->hasTrivialDefaultConstructor()) {
      std::string name = _i.c;
      name.insert(cfg()._root.size(), cfg()._ctor);
      out.hf() << "// Implicit constructor of " << _i.cpp << "\n";
      out.sf() << "// Implicit constructor of " << _i.cpp << "\n";
      out.hf() << _i.c << " *" << name << "();\n\n";
      out.sf() << _i.c << " *" << name << "() {\n";
      out.sf() << "  return new " << _i.cpp << "();\n}\n\n";
    }
    if (!any_dtor) {
      std::string name = _i.c;
      name.insert(cfg()._root.size(), cfg()._dtor);
      out.hf() << "// Implicit destructor of " << _i.cpp << "\n";
      out.sf() << "// Implicit destructor of " << _i.cpp << "\n";
      out.hf() << "void " << name << "(" << _i.c << " *" << cfg()._this << ");\n\n";
      out.sf() << "void " << name << "(" << _i.c << " *" << cfg()._this << ") {\n";
      out.sf() << "  delete " << cfg()._this << ";\n}\n\n";
    }
    if (_d->hasTrivialDefaultConstructor() || _d->hasUserProvidedDefaultConstructor()) {
      std::string name = _i.c;
      name.insert(cfg()._root.size(), cfg()._ctor + "array_");
      Identifier sizet(_d->getASTContext().getSizeType(), Identifier("length"), cfg());
      out.hf() << "// Array constructor of " << _i.cpp << "\n";
      out.sf() << "// Array constructor of " << _i.cpp << "\n";
      out.hf() << _i.c << " *" << name << "(" << sizet.c << ");\n\n";
      out.sf() << _i.c << " *" << name << "(" << sizet.c << ") {\n";
      out.sf() << "  return new " << _i.cpp << "[length];\n}\n\n";
      name = _i.c;
      name.insert(cfg()._root.size(), cfg()._dtor + "array_");
      out.hf() << "// Array destructor of " << _i.cpp << "\n";
      out.sf() << "// Array destructor of " << _i.cpp << "\n";
      out.hf() << "void " << name << "(" << _i.c << " *" << cfg()._this << ");\n\n";
      out.sf() << "void " << name << "(" << _i.c << " *" << cfg()._this << ") {\n";
      out.sf() << "  delete[] " << cfg()._this << ";\n}\n\n";
    }
  } else {
    std::string warn = "Warning: Class " + _i.cpp + " lacks a definition\n";
    std::cerr << warn;
    _out.hf() << "// " << warn << "\n";
  }
}
