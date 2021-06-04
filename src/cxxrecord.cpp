/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "cxxrecord.hpp"

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/RecordLayout.h>

#include "filter.hpp"
#include "json.hpp"
#include "options.hpp"

using namespace unplusplus;
using namespace clang;

bool ClassDeclareJob::accept(const type *D) { return getAnonTypedef(D) || !getName(D).empty(); }

ClassDeclareJob::ClassDeclareJob(ClassDeclareJob::type *D, clang::Sema &S, JobManager &jm)
    : Job<ClassDeclareJob::type>(D, S, jm) {
  _name += " (Declaration)";
  manager().declare(_d, this);

  auto *CTSD = dyn_cast<ClassTemplateSpecializationDecl>(_d);
  if (CTSD) {
    manager().lazyCreate(CTSD->getSpecializedTemplate(), S);
    manager().lazyCreate(CTSD->getTemplateArgs().asArray(), S);
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

  Json::Value j(Json::ValueType::objectValue);
  j[jcfg()._cname] = i.c;
  j[jcfg()._qname] = jcfg().jsonQName(_d);
  j[jcfg()._location] = _location;
  _out.json()[jcfg()._class][i.cpp] = j;
}

SuperclassVisitor::SuperclassVisitor(Visitor F, const clang::CXXRecordDecl *D, Visitor H)
    : _fn(F), _fn2(H) {
  // The procedure here has to mimic clang's RecordLayoutBuilder.cpp to order the fields of the base
  // classes correctly. It may not work with the Microsoft ABI.
  D->getIndirectPrimaryBases(_indirect);
  visitNonVirtualBase(D, {});
  F(D, {});
  visitVirtualBase(D, {});
}

void SuperclassVisitor::visitNonVirtualBase(const clang::CXXRecordDecl *D, ClassList L) {
  std::vector<const clang::CXXRecordDecl *> newParents(L);
  newParents.push_back(D);

  const ASTRecordLayout &layout = D->getASTContext().getASTRecordLayout(D);
  const CXXRecordDecl *PrimaryBase = layout.getPrimaryBase();
  if (PrimaryBase) {
    visitNonVirtualBase(PrimaryBase, newParents);
    _fn(PrimaryBase, newParents);
    if (layout.isPrimaryBaseVirtual()) {
      _indirect.insert(PrimaryBase);
      _vbases.emplace(PrimaryBase);
    }
  }

  if (_fn2) _fn2(D, newParents);

  for (const auto base : D->bases()) {
    if (base.isVirtual()) continue;
    const CXXRecordDecl *based = base.getType()->getAsCXXRecordDecl();
    if (based == PrimaryBase && !layout.isPrimaryBaseVirtual()) continue;
    visitNonVirtualBase(based, newParents);
    _fn(based, newParents);
  }
}

void SuperclassVisitor::visitVirtualBase(const clang::CXXRecordDecl *D, ClassList L) {
  std::vector<const clang::CXXRecordDecl *> newParents(L);
  newParents.push_back(D);
  const ASTRecordLayout &layout = D->getASTContext().getASTRecordLayout(D);
  const CXXRecordDecl *PrimaryBase = layout.getPrimaryBase();
  for (const auto base : D->bases()) {
    const CXXRecordDecl *based = base.getType()->getAsCXXRecordDecl();
    if (base.isVirtual()) {
      if (!(based == PrimaryBase && layout.isPrimaryBaseVirtual())) {
        if (!_indirect.count(based)) {
          if (!_vbases.insert(based).second) continue;
          visitNonVirtualBase(based, newParents);
          _fn(based, newParents);
        }
      }
    }
    if (!based->getNumVBases()) continue;
    visitVirtualBase(based, newParents);
  }
}

void DiamondRenamer::submit(std::string &name, ClassList path) {
  _names[name].push_back({name, path});
}

void DiamondRenamer::disambiguate(const IdentifierConfig &cfg) {
  for (auto &n : _names) {
    if (n.second.size() > 1) {
      while (1) {
        size_t exhausted = 0;
        for (auto &o : n.second) {
          if (o.path.size()) {
            o.name = getName(o.path.back()) + cfg.c_separator + o.name;
            o.path.pop_back();
          } else {
            exhausted++;
          }
        }

        if (exhausted == n.second.size()) break;

        std::unordered_set<std::string> newNames;
        for (auto &o : n.second) {
          newNames.emplace(o.name);
        }

        if (newNames.size() == n.second.size()) break;
      }
    }
  }
}

void ClassDefineJob::FieldInfo::adjustNames(const IdentifierConfig &cfg) {
  DiamondRenamer DR;
  for (auto &sf : subFields) {
    sf.adjustNames(DR, cfg);
  }
  DR.disambiguate(cfg);
}

void ClassDefineJob::FieldInfo::adjustNames(DiamondRenamer &DR, const IdentifierConfig &cfg) {
  if (name.size()) {
    DR.submit(name, parents);
    if (subFields.size()) {
      // process the anonymous struct or union's members in a new scope
      adjustNames(cfg);
    }
  } else {
    // for an anonymous struct or union that didn't also declare a field, process the names in the
    // parent scope
    for (auto &sf : subFields) {
      sf.adjustNames(DR, cfg);
    }
  }
}

bool ClassDefineJob::accept(type *D, const IdentifierConfig &cfg, Sema &S) {
  auto *CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D);
  // If an explicit specialiation already appeared, it may be that it was put there to create a
  // hidden definition.
  if (!D->isCompleteDefinition() && CTSD &&
      CTSD->getTemplateSpecializationKind() != TSK_ExplicitSpecialization &&
      CTSD->getSpecializedTemplate()->getTemplatedDecl()->isCompleteDefinition()) {
    // clang is "lazy" and doesn't add any members that weren't used. We can force them to be
    // added.
    if (Verbose) std::cout << "Instantiating " << cfg.getDebugName(D) << std::endl;
    SourceLocation L = CTSD->getLocation();
    TemplateSpecializationKind TSK = TSK_ExplicitInstantiationDeclaration;
    if (!S.InstantiateClassTemplateSpecialization(L, CTSD, TSK, true)) {
      S.InstantiateClassTemplateSpecializationMembers(L, CTSD, TSK);
    } else {
      std::cerr << "Error: Couldn't instantiate " << cfg.getDebugName(D) << std::endl;
    }
  }

  return ClassDeclareJob::accept(D) && D->isCompleteDefinition();
}

ClassDefineJob::ClassDefineJob(ClassDefineJob::type *D, clang::Sema &S, JobManager &jm)
    : Job<ClassDefineJob::type>(D, S, jm) {
  _name += " (Definition)";
  manager().define(_d, this);
  depends(_d, false);

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
  if (_fields.subFields.size()) return;

  SuperclassVisitor([&](const clang::CXXRecordDecl *D, ClassList L) { addFields(D, L, _fields); },
                    _d,
                    [&](const clang::CXXRecordDecl *D, ClassList L) {
                      const ASTContext &AC = D->getASTContext();
                      const ASTRecordLayout &layout = AC.getASTRecordLayout(D);
                      if (!layout.getPrimaryBase() && D->isDynamicClass()) {
                        _fields.sub(nullptr, L, "vtable", AC.VoidPtrTy);
                      }
                    });
  _fields.adjustNames(cfg());

  if (_d->isEmpty()) {
    _fields.sub(nullptr, {_d}, "__empty", _d->getASTContext().CharTy);
  }
}

// extract a TagDecl which might be in the process of being declared anonymously.
static const TagDecl *getTagDecl(QualType QT) {
  if (QT->isRecordType()) {
    return QT->getAsRecordDecl();
  } else if (QT->isEnumeralType()) {
    return QT->getUnqualifiedDesugaredType()->getAs<EnumType>()->getDecl();
  } else if (QT->isAnyPointerType()) {
    return getTagDecl(QT->getPointeeType());
  } else if (QT->isArrayType()) {
    return getTagDecl(QT->castAsArrayTypeUnsafe()->getElementType());
  } else if (QT->isReferenceType()) {
    return getTagDecl(QT->getPointeeType());
  }
  return nullptr;
}

void ClassDefineJob::addFields(const clang::CXXRecordDecl *d, const ClassList parents,
                               FieldInfo &list) {
  std::vector<const clang::CXXRecordDecl *> newParents(parents);
  newParents.push_back(d);
  const ASTContext &AC = _d->getASTContext();

  for (const auto *f : d->fields()) {
    QualType QT = f->getType();
    std::string name = getName(f);

    const TagDecl *TD = getTagDecl(QT);
    if (TD) {
      const CXXRecordDecl *dc = dyn_cast_or_null<CXXRecordDecl>(TD->getParent());
      // detect whether the fields is also defining an anonymous type.
      if (TD->isEmbeddedInDeclarator() && TD->isThisDeclarationADefinition() &&
          !Identifier::ids.count(TD) && dc == d) {
        if (const auto *RD = dyn_cast<CXXRecordDecl>(TD)) {
          // FIXME: The struct here can theoretically have base classes which could theoretically be
          // handled. This case is quite unconventional.
          if (RD->getNumBases() > 0) {
            std::cerr << "Error: An anonymous class, struct, or union has a base class: "
                      << cfg().getCXXQualifiedName(RD) << " at "
                      << RD->getLocation().printToString(AC.getSourceManager()) << std::endl;
            std::exit(1);
          }

          // handles an anonymous struct or union used to group fields, where its fields appear in
          // the parent scope, or defined as a part of a field declaration.
          list.sub(f, newParents, name, QT, TD->isUnion());
          addFields(dyn_cast<CXXRecordDecl>(TD), newParents, list.subFields.back());
          Identifier::ids[RD] = Identifier();
          continue;
        } else {
          // the anonymous struct, union, or enum can be named using this field's name. A dependency
          // will be created on the anonymous type, but if we specify a name for it here, generation
          // will proceed later with this name instead of throwing an error.
          std::cout << "Renaming " << cfg().getCXXQualifiedName(TD);
          Identifier::ids[TD] = Identifier(f, cfg());
          std::cout << " to " << Identifier::ids[TD].cpp << std::endl;
        }
      }
    }

    manager().filter().sanitizeType(QT, AC);
    list.sub(f, newParents, name, QT);
    depends(QT, true);
  }
}

void ClassDefineJob::writeFields(FieldInfo &list, std::string indent,
                                 std::unordered_set<std::string> *names) {
  const ASTContext &AC = _d->getASTContext();
  std::unique_ptr<std::unordered_set<std::string>> mynames;
  if (!names) {
    mynames = std::make_unique<std::unordered_set<std::string>>();
    names = mynames.get();
  }
  for (auto &f : list.subFields) {
    if (f.name.size()) {
      if (names->count(f.name)) {
        int i = 2;
        while (names->count(f.name + cfg().c_separator + std::to_string(i))) i++;
        f.name += cfg().c_separator + std::to_string(i);
      } else {
        names->emplace(f.name);
      }
    }

    Identifier fi(f.type, Identifier(f.name, cfg()), cfg());
    if (f.subFields.size()) {
      if (f.isUnion)
        _out.hf() << indent << "union {\n";
      else
        _out.hf() << indent << "struct {\n";
      writeFields(f, indent + "  ", f.name.empty() ? names : nullptr);
      _out.hf() << indent << "}";
      if (f.name.size()) _out.hf() << " " << fi.c;
      _out.hf() << ";\n";
    } else {
      _out.hf() << indent << fi.c;
      if (f.field && f.field->isBitField()) {
        _out.hf() << " : " << f.field->getBitWidthValue(AC);
      }
      Decl *LocD = f.field ? (Decl *)f.field : (Decl *)f.parents.back();
      std::string location = LocD->getLocation().printToString(AC.getSourceManager());
      _out.hf() << "; // ";
      for (auto &p : f.parents) {
        _out.hf() << getName(p) << "->";
      }
      _out.hf() << getName(f.field);
      _out.hf() << " @ " << location << "\n";
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
  Json::Value &j = _out.json()[jcfg()._class][i.cpp];
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

  j[jcfg()._union] = _d->isUnion();

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
