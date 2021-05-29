/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "cxxrecord.hpp"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RecordLayout.h>

#include "filter.hpp"

using namespace unplusplus;
using namespace clang;

CXXRecordDeclWriter::CXXRecordDeclWriter(const type *d, Sema &S, DeclHandler &dh)
    : DeclWriter(d, dh), _S(S) {
  if (d->isTemplated()) {
    std::cerr << "Warning: Ignored templated class " << _i.cpp << std::endl;
    return;  // ignore unspecialized template decl
  }
  std::cerr << "Declare " << _i.cpp << std::endl;
  SubOutputs out(_out);
  preamble(out.hf());

  auto *CTSD = dyn_cast<ClassTemplateSpecializationDecl>(d);
  if (CTSD) {
    _dh.forward(CTSD->getSpecializedTemplate(), S);
    _dh.forward(CTSD->getTemplateArgs().asArray(), S);
  }

  if (_d->isUnion())
    _keyword = "union";
  else
    _keyword = "struct";

  // print only the forward declaration
  out.hf() << "#ifdef __cplusplus\n";
  out.hf() << "typedef " << _i.cpp << " " << _i.c << ";\n";
  out.hf() << "#else\n";
  out.hf() << "typedef " << _keyword << " " << _i.c << cfg()._struct << " " << _i.c << ";\n";
  out.hf() << "#endif // __cplusplus\n\n";
  maybeDefine();
  writeMembers(out);
}

void CXXRecordDeclWriter::maybeDefine() {
  const auto *CCTSD = dyn_cast<ClassTemplateSpecializationDecl>(_d);
  if (!_d->hasDefinition() && CCTSD &&
      CCTSD->getTemplateSpecializationKind() != TSK_ExplicitSpecialization) {
    if (CCTSD->getSpecializedTemplate()->getTemplatedDecl()->isCompleteDefinition()) {
      // clang is "lazy" and doesn't add any members that weren't used. We can force them to be
      // added.
      auto *CTSD = const_cast<ClassTemplateSpecializationDecl *>(CCTSD);
      std::cerr << "Instantiating " << _i.cpp << std::endl;
      SourceLocation L = CTSD->getLocation();
      TemplateSpecializationKind TSK = TSK_ExplicitInstantiationDeclaration;
      if (!_S.InstantiateClassTemplateSpecialization(L, CTSD, TSK, true)) {
        _S.InstantiateClassTemplateSpecializationMembers(L, CTSD, TSK);
        if (!_dh.isRenamedInternal(CTSD)) _makeInstantiation = true;
      } else {
        std::cerr << "Error: Couldn't instantiate " << _i.cpp << std::endl;
      }
    }  // Else: the template isn't defined yet, so we can't instantiate
  }

  if (_d->hasDefinition() && _d->isCompleteDefinition()) {
    _S.ForceDeclarationOfImplicitMembers(const_cast<CXXRecordDecl *>(_d));
  }
}

void CXXRecordDeclWriter::writeFields(Outputs &out, const CXXRecordDecl *d, std::string indent) {
  const ASTContext &AC = _d->getASTContext();
  const ASTRecordLayout &layout = AC.getASTRecordLayout(d);

  for (const auto *f : d->fields()) {
    QualType qt = f->getType();
    std::string name = getName(f);

    if (qt->isRecordType()) {
      // handles an anonymous struct or union defined as a field.
      const CXXRecordDecl *rd = dyn_cast<CXXRecordDecl>(qt->getAsRecordDecl());
      if (!rd) throw std::runtime_error("Record Type is not CXX");
      const CXXRecordDecl *dc = dyn_cast_or_null<CXXRecordDecl>(rd->getParent());
      if (rd->isEmbeddedInDeclarator() && rd->isThisDeclarationADefinition() &&
          !Identifier::ids.count(rd) && dc == d) {
        if (rd->isUnion())
          out.hf() << indent << "union {\n";
        else
          out.hf() << indent << "struct {\n";
        writeFields(out, rd, indent + "  ");
        out.hf() << indent << "}";
        if (name.size()) out.hf() << " " << name;
        out.hf() << ";\n";
        continue;
      }
    } else if (qt->isEnumeralType()) {
      // handles an anonymous enum defined as a field.
      const EnumDecl *ed =
          dyn_cast<EnumDecl>(qt->getUnqualifiedDesugaredType()->getAs<EnumType>()->getDecl());
      const CXXRecordDecl *dc = dyn_cast_or_null<CXXRecordDecl>(ed->getParent());
      if (ed->isEmbeddedInDeclarator() && ed->isThisDeclarationADefinition() &&
          !Identifier::ids.count(ed) && dc == d) {
        // give the anonymous enum the name of the field.
        Identifier::ids[ed] = Identifier(f, cfg());
      }
    }

    _dh.forward(qt, _S);

    if (name.size() && _fields.count(name)) {
      int i = 2;
      while (_fields.count(name + cfg().c_separator + std::to_string(i))) i++;
      name += cfg().c_separator + std::to_string(i);
    }

    sanitizeType(qt, AC);

    std::string location = f->getLocation().printToString(AC.getSourceManager());
    try {
      Identifier fi(qt, Identifier(name, cfg()), cfg());
      out.hf() << indent << fi.c;
      if (f->isBitField()) {
        out.hf() << " : " << f->getBitWidthValue(AC);
      }
      out.hf() << "; // " << f->getQualifiedNameAsString() << " at: " << location << "\n";
    } catch (const mangling_error err) {
      std::stringstream ss;
      ss << indent << "// Error: " << cfg().getCXXQualifiedName(f) << " is " << err.what() << " at "
         << location;
      std::cerr << ss.str() << std::endl;
      out.hf() << ss.str() << "\n";
    }
    if (name.size()) _fields.emplace(name);
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

void CXXRecordDeclWriter::writeMembers(Outputs &out) {
  if (!_d->hasDefinition() || !_d->isCompleteDefinition()) return;
  if (_wroteMembers) return;
  out.hf() << "// Members of type " << _i.cpp << "\n";
  out.hf() << _keyword << " " << _i.c << cfg()._struct << " {\n";
  // The procedure here has to mimic clang's RecordLayoutBuilder.cpp to order the fields of the base
  // classes correctly. It may not work with the Microsoft ABI.
  _d->getIndirectPrimaryBases(_indirect);
  writeNonVirtualBases(out, _d);
  writeFields(out, _d);
  writeVirtualBases(out, _d);
  if (_d->isEmpty()) {
    out.hf() << "  char __empty;\n";
  }
  out.hf() << "};\n";
  out.hf() << "#ifdef __cplusplus\n";
  out.hf() << "static_assert(sizeof(" << _keyword << " " << _i.c << cfg()._struct << ") == sizeof("
           << _i.cpp << "), \"Size of C struct must match C++\");\n";
  out.hf() << "#else\n";
  out.hf() << "_Static_assert(sizeof(" << _keyword << " " << _i.c << cfg()._struct
           << ") == " << _d->getASTContext().getTypeSizeInChars(_d->getTypeForDecl()).getQuantity()
           << ", \"Size of C struct must match C++\");\n";
  out.hf() << "#endif\n\n";
  _wroteMembers = true;
}

// writer destructors should run after forward declarations are written
CXXRecordDeclWriter::~CXXRecordDeclWriter() {
  maybeDefine();

  if (_d->hasDefinition() && _d->isCompleteDefinition()) {
    SubOutputs out(_out);

    if (_makeInstantiation) {
      out.hf() << "#ifdef __cplusplus\n";
      out.hf() << "extern template class " << _i.cpp << ";\n";
      out.hf() << "#endif\n\n";
      out.sf() << "template class " << _i.cpp << ";\n\n";
    }

    writeMembers(out);

    // initializer lists are special snowflakes that stop existing after an expression is evaluated
    bool no_ctor = getName(_d) == "initializer_list" &&
                   getName(dyn_cast_or_null<Decl>(_d->getParent())) == "std";

    for (const auto d : _d->decls()) {
      if (d->getAccess() == AccessSpecifier::AS_public ||
          d->getAccess() == AccessSpecifier::AS_none) {
        if (const auto *nd = dyn_cast<NamedDecl>(d)) {
          // Drop it if this decl will be ambiguous with a constructor
          if (getName(nd) == getName(_d) && !isa<CXXConstructorDecl>(nd)) continue;
        }
        if (const auto *cd = dyn_cast<CXXRecordDecl>(d)) {
          // this one should have been handled already during field enumeration.
          if (cd->isAnonymousStructOrUnion() && cd->isEmbeddedInDeclarator() &&
              cd->isThisDeclarationADefinition())
            continue;
        }
        if (no_ctor && isa<CXXConstructorDecl>(d)) continue;
        _dh.forward(d, _S);
      }
    }
    if (!no_ctor && _d->hasDefaultConstructor()) {
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
    std::string warn = "Class " + _i.cpp + " lacks a definition\n";
    _out.hf() << "// " << warn << "\n";
  }
}

bool ClassDeclareJob::accept(const type *D) { return getAnonTypedef(D) || !getName(D).empty(); }

ClassDeclareJob::ClassDeclareJob(ClassDeclareJob::type *D, clang::Sema &S, JobManager &jm)
    : Job<ClassDeclareJob::type>(D, S, jm) {
  _name += " (Declaration)";
  std::cout << "Job Created: " << _name << std::endl;
  manager()._declarations[D] = this;

  auto *CTSD = dyn_cast<ClassTemplateSpecializationDecl>(_d);
  if (CTSD) {
    manager().create(CTSD->getSpecializedTemplate(), S);
    manager().create(CTSD->getTemplateArgs().asArray(), S);
  }

  checkReady();
}
void ClassDeclareJob::impl() {
  _out.hf() << "// " << _location << "\n";
  _out.hf() << "// " << _name << "\n";

  std::string keyword;
  if (_d->isUnion())
    keyword = "union";
  else
    keyword = "struct";

  // print only the forward declaration
  Identifier i(_d, cfg());
  _out.hf() << "#ifdef __cplusplus\n";
  _out.hf() << "typedef " << i.cpp << " " << i.c << ";\n";
  _out.hf() << "#else\n";
  _out.hf() << "typedef " << keyword << " " << i.c << cfg()._struct << " " << i.c << ";\n";
  _out.hf() << "#endif // __cplusplus\n\n";
}

bool ClassDefineJob::accept(const type *D) {
  auto *CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D);
  return ClassDeclareJob::accept(D) &&
         (!CTSD || CTSD->getSpecializedTemplate()->getTemplatedDecl()->isCompleteDefinition());
}

ClassDefineJob::ClassDefineJob(ClassDefineJob::type *D, clang::Sema &S, JobManager &jm)
    : Job<ClassDefineJob::type>(D, S, jm) {
  _name += " (Definition)";
  std::cout << "Job Created: " << _name << " " << D << std::endl;
  manager()._definitions[_d] = this;
  depends(D, false);

  const auto *CCTSD = dyn_cast<ClassTemplateSpecializationDecl>(_d);
  if (!_d->hasDefinition() && CCTSD &&
      CCTSD->getTemplateSpecializationKind() != TSK_ExplicitSpecialization) {
    if (CCTSD->getSpecializedTemplate()->getTemplatedDecl()->isCompleteDefinition()) {
      // clang is "lazy" and doesn't add any members that weren't used. We can force them to be
      // added.
      auto *CTSD = const_cast<ClassTemplateSpecializationDecl *>(CCTSD);
      std::cout << "Instantiating " << _name << std::endl;
      SourceLocation L = CTSD->getLocation();
      TemplateSpecializationKind TSK = TSK_ExplicitInstantiationDeclaration;
      if (!_s.InstantiateClassTemplateSpecialization(L, CTSD, TSK, true)) {
        _s.InstantiateClassTemplateSpecializationMembers(L, CTSD, TSK);
        if (!manager().isRenamed(CTSD)) _makeInstantiation = true;
      } else {
        std::cerr << "Error: Couldn't instantiate " << _name << std::endl;
      }
    }  // Else: the template isn't defined yet, so we can't instantiate
  }

  _s.ForceDeclarationOfImplicitMembers(_d);

  findFields();

  // initializer lists are special snowflakes that stop existing after an expression is evaluated
  bool no_ctor = (getName(_d) == "initializer_list" &&
                  getName(dyn_cast_or_null<Decl>(_d->getParent())) == "std") ||
                 _d->isAbstract();

  for (auto *d : _d->decls()) {
    if (d->getAccess() == AccessSpecifier::AS_public ||
        d->getAccess() == AccessSpecifier::AS_none) {
      if (const auto *nd = dyn_cast<NamedDecl>(d)) {
        // Drop it if this decl will be ambiguous with a constructor
        if (getName(nd) == getName(_d) && !isa<CXXConstructorDecl>(nd)) continue;
      }
      if (const auto *cd = dyn_cast<CXXRecordDecl>(d)) {
        // this one should have been handled already during field enumeration.
        if (cd->isAnonymousStructOrUnion() && cd->isEmbeddedInDeclarator() &&
            cd->isThisDeclarationADefinition())
          continue;
      }
      if (no_ctor && isa<CXXConstructorDecl>(d)) continue;
      manager().create(d, _s);
    }
  }

  checkReady();
}

void ClassDefineJob::findFields() {
  if (!_d->hasDefinition() || !_d->isCompleteDefinition()) return;
  if (_fields.size()) return;

  // The procedure here has to mimic clang's RecordLayoutBuilder.cpp to order the fields of the base
  // classes correctly. It may not work with the Microsoft ABI.
  _d->getIndirectPrimaryBases(_indirect);
  findNonVirtualBaseFields(_d);
  addFields(_d, _fields);
  findVirtualBaseFields(_d);
  if (_d->isEmpty()) {
    _fields.push_back({nullptr, _d, "__empty", _d->getASTContext().CharTy});
  }
}

void ClassDefineJob::addFields(const clang::CXXRecordDecl *d, std::vector<FieldInfo> &list) {
  const ASTContext &AC = _d->getASTContext();

  for (const auto *f : d->fields()) {
    QualType QT = f->getType();
    std::string name = getName(f);

    if (QT->isRecordType()) {
      // handles an anonymous struct or union defined as a field.
      const CXXRecordDecl *rd = dyn_cast<CXXRecordDecl>(QT->getAsRecordDecl());
      if (!rd) throw std::runtime_error("Record Type is not CXX");
      const CXXRecordDecl *dc = dyn_cast_or_null<CXXRecordDecl>(rd->getParent());
      if (rd->isEmbeddedInDeclarator() && rd->isThisDeclarationADefinition() &&
          !Identifier::ids.count(rd) && dc == d) {
        list.push_back({f, d, name, QT, rd->isUnion()});
        addFields(rd, list.back().subFields);
        _nameCount[name] += 1;
        continue;
      }
    } else if (QT->isEnumeralType()) {
      // handles an anonymous enum defined as a field.
      const EnumDecl *ed =
          dyn_cast<EnumDecl>(QT->getUnqualifiedDesugaredType()->getAs<EnumType>()->getDecl());
      const CXXRecordDecl *dc = dyn_cast_or_null<CXXRecordDecl>(ed->getParent());
      if (ed->isEmbeddedInDeclarator() && ed->isThisDeclarationADefinition() &&
          !Identifier::ids.count(ed) && dc == d) {
        // give the anonymous enum the name of the field.
        Identifier::ids[ed] = Identifier(f, cfg());
      }
    }

    sanitizeType(QT, AC);
    list.push_back({f, d, name, QT});
    depends(QT, true);
    _nameCount[name] += 1;
  }
}

void ClassDefineJob::findVirtualBaseFields(const clang::CXXRecordDecl *d) {
  const ASTRecordLayout &layout = d->getASTContext().getASTRecordLayout(d);
  const CXXRecordDecl *PrimaryBase = layout.getPrimaryBase();
  for (const auto base : d->bases()) {
    const CXXRecordDecl *based = base.getType()->getAsCXXRecordDecl();
    if (base.isVirtual()) {
      if (!(based == PrimaryBase && layout.isPrimaryBaseVirtual())) {
        if (!_indirect.count(based)) {
          if (!_vbases.insert(based).second) continue;
          findNonVirtualBaseFields(based);
          addFields(based, _fields);
        }
      }
    }
    if (!based->getNumVBases()) continue;
    findVirtualBaseFields(based);
  }
}

void ClassDefineJob::findNonVirtualBaseFields(const clang::CXXRecordDecl *d) {
  const ASTRecordLayout &layout = d->getASTContext().getASTRecordLayout(d);
  const CXXRecordDecl *PrimaryBase = layout.getPrimaryBase();
  if (PrimaryBase) {
    findNonVirtualBaseFields(PrimaryBase);
    addFields(PrimaryBase, _fields);
    if (layout.isPrimaryBaseVirtual()) {
      _indirect.insert(PrimaryBase);
      _vbases.emplace(PrimaryBase);
    }
  } else if (d->isDynamicClass()) {
    std::string name("vtable");
    _fields.push_back({nullptr, d, name, d->getASTContext().VoidPtrTy});
    _nameCount[name] += 1;
  }

  for (const auto base : d->bases()) {
    if (base.isVirtual()) continue;
    const CXXRecordDecl *based = base.getType()->getAsCXXRecordDecl();
    if (based == PrimaryBase && !layout.isPrimaryBaseVirtual()) continue;
    findNonVirtualBaseFields(based);
    addFields(based, _fields);
  }
}

void ClassDefineJob::writeFields(std::vector<FieldInfo> &list, std::string indent) {
  const ASTContext &AC = _d->getASTContext();
  std::unordered_set<std::string> names;
  for (auto &f : list) {
    if (f.name.size()) {
      if (_nameCount[f.name] > 1) {
        f.name =
            Identifier(f.parent, cfg()).c.substr(cfg()._root.size()) + cfg().c_separator + f.name;
      }
      if (names.count(f.name)) {
        int i = 2;
        while (names.count(f.name + cfg().c_separator + std::to_string(i))) i++;
        f.name += cfg().c_separator + std::to_string(i);
      } else {
        names.emplace(f.name);
      }
    }

    if (f.subFields.size()) {
      if (f.isUnion)
        _out.hf() << indent << "union {\n";
      else
        _out.hf() << indent << "struct {\n";
      writeFields(f.subFields, indent + "  ");
      _out.hf() << indent << "}";
      if (f.name.size()) _out.hf() << " " << f.name;
      _out.hf() << ";\n";
    } else {
      Identifier fi(f.type, Identifier(f.name, cfg()), cfg());
      _out.hf() << indent << fi.c;
      Decl *LocD = f.field ? (Decl *)f.field : (Decl *)f.parent;
      std::string location = LocD->getLocation().printToString(AC.getSourceManager());
      _out.hf() << "; // " << location << "\n";
    }
  }
}

void ClassDefineJob::impl() {
  const ASTContext &AC = _d->getASTContext();
  _out.hf() << "// " << _location << "\n";
  _out.hf() << "// " << _name << "\n";

  std::string keyword;
  if (_d->isUnion())
    keyword = "union";
  else
    keyword = "struct";

  Identifier i(_d, cfg());
  _out.hf() << keyword << " " << i.c << cfg()._struct << " {\n";
  writeFields(_fields);
  _out.hf() << "};\n";

  _out.hf() << "#ifdef __cplusplus\n";
  _out.hf() << "static_assert(sizeof(" << keyword << " " << i.c << cfg()._struct << ") == sizeof("
            << i.cpp << "), \"Size of C struct must match C++\");\n";
  _out.hf() << "#else\n";
  _out.hf() << "_Static_assert(sizeof(" << keyword << " " << i.c << cfg()._struct
            << ") == " << _d->getASTContext().getTypeSizeInChars(_d->getTypeForDecl()).getQuantity()
            << ", \"Size of C struct must match C++\");\n";
  _out.hf() << "#endif\n\n";
}
