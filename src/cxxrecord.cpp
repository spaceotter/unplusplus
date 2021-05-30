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

bool ClassDeclareJob::accept(const type *D) { return getAnonTypedef(D) || !getName(D).empty(); }

ClassDeclareJob::ClassDeclareJob(ClassDeclareJob::type *D, clang::Sema &S, JobManager &jm)
    : Job<ClassDeclareJob::type>(D, S, jm) {
  _name += " (Declaration)";
  manager().declare(_d, this);

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

bool ClassDefineJob::accept(type *D, const IdentifierConfig &cfg, Sema &S) {
  auto *CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D);
  // If an explicit specialiation already appeared, it may be that it was put there to create a
  // hidden definition.
  if (!D->hasDefinition() && CTSD &&
      CTSD->getTemplateSpecializationKind() != TSK_ExplicitSpecialization &&
      CTSD->getSpecializedTemplate()->getTemplatedDecl()->isCompleteDefinition()) {
    // clang is "lazy" and doesn't add any members that weren't used. We can force them to be
    // added.
    std::cout << "Instantiating " << cfg.getDebugName(D) << std::endl;
    SourceLocation L = CTSD->getLocation();
    TemplateSpecializationKind TSK = TSK_ExplicitInstantiationDeclaration;
    if (!S.InstantiateClassTemplateSpecialization(L, CTSD, TSK, true)) {
      S.InstantiateClassTemplateSpecializationMembers(L, CTSD, TSK);
    } else {
      std::cerr << "Error: Couldn't instantiate " << cfg.getDebugName(D) << std::endl;
    }
  }

  return ClassDeclareJob::accept(D) && D->hasDefinition();
}

ClassDefineJob::ClassDefineJob(ClassDefineJob::type *D, clang::Sema &S, JobManager &jm)
    : Job<ClassDefineJob::type>(D, S, jm) {
  _name += " (Definition)";
  manager().define(D, this);
  depends(D, false);

  _s.ForceDeclarationOfImplicitMembers(_d);

  findFields();

  // initializer lists are special snowflakes that stop existing after an expression is evaluated
  _no_ctor = (getName(_d) == "initializer_list" &&
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
      if (_no_ctor && isa<CXXConstructorDecl>(d)) continue;
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

  if (!_no_ctor && _d->hasDefaultConstructor()) {
    std::string name = i.c;
    name.insert(cfg()._root.size(), cfg()._ctor + "array_");
    Identifier sizet(_d->getASTContext().getSizeType(), Identifier("length"), cfg());
    _out.hf() << "// Array constructor of " << i.cpp << "\n";
    _out.sf() << "// Array constructor of " << i.cpp << "\n";
    _out.hf() << i.c << " *" << name << "(" << sizet.c << ");\n\n";
    _out.sf() << i.c << " *" << name << "(" << sizet.c << ") {\n";
    _out.sf() << "  return new " << i.cpp << "[length];\n}\n\n";
    name = i.c;
    name.insert(cfg()._root.size(), cfg()._dtor + "array_");
    _out.hf() << "// Array destructor of " << i.cpp << "\n";
    _out.sf() << "// Array destructor of " << i.cpp << "\n";
    _out.hf() << "void " << name << "(" << i.c << " *" << cfg()._this << ");\n\n";
    _out.sf() << "void " << name << "(" << i.c << " *" << cfg()._this << ") {\n";
    _out.sf() << "  delete[] " << cfg()._this << ";\n}\n\n";
  }
}
