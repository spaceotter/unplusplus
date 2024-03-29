/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "jobs.hpp"

#include <clang/AST/DeclFriend.h>

#include "cxxrecord.hpp"
#include "enum.hpp"
#include "filter.hpp"
#include "function.hpp"
#include "options.hpp"

using namespace clang;
using namespace unplusplus;

JobBase::JobBase(JobManager &manager, clang::Sema &S)
    : _manager(manager), _out(manager.out()), _s(S) {
  _manager._jobs.emplace_back(this);
}

IdentifierConfig &JobBase::cfg() { return _manager.cfg(); }
JsonConfig &JobBase::jcfg() { return _manager.jcfg(); }
ASTNameGenerator &JobBase::nameGen() { return _manager.nameGen(); }

void JobBase::depends(JobBase *other) {
  if (other && !other->_done) {
    _depends.emplace_back(other);
    other->_dependent.emplace_back(this);
  }
}

void JobBase::depends(clang::Decl *D, bool define) {
  _manager.create(D, _s);
  if (_manager._declarations.count(D)) {
    depends(_manager._declarations.at(D));
  } else {
    std::cerr << "Job " << _name << " depends on declaration " << cfg().getCXXQualifiedName(D)
              << " at " << D->getLocation().printToString(D->getASTContext().getSourceManager())
              << " but it does not exist." << std::endl;
    std::exit(1);
  }
  if (define) {
    if (_manager._definitions.count(D)) {
      depends(_manager._definitions.at(D));
    } else {
      std::cerr << "Job " << _name << " depends on definition " << cfg().getCXXQualifiedName(D)
                << " at " << D->getLocation().printToString(D->getASTContext().getSourceManager())
                << " but it does not exist." << std::endl;
      std::exit(1);
    }
  }
}

void JobBase::depends(clang::QualType QT, bool define) {
  _manager.create(QT, _s);
  const Type *t = QT.getTypePtrOrNull()->getUnqualifiedDesugaredType();
  if (t == nullptr) {
    return;
  } else if (const auto *tt = dyn_cast<RecordType>(t)) {
    depends(tt->getDecl(), define);
  } else if (const auto *tt = dyn_cast<EnumType>(t)) {
    depends(tt->getDecl(), false);
  } else if (isa<PointerType>(t) || isa<ReferenceType>(t)) {
    if (t->getPointeeType()->isRecordType()) {
      depends(t->getPointeeType()->getAsRecordDecl(), false);
    } else {
      depends(t->getPointeeType(), define);
    }
  } else if (const auto *at = dyn_cast<ConstantArrayType>(t)) {
    depends(at->getElementType(), define);
  } else if (const auto *pt = dyn_cast<InjectedClassNameType>(t)) {
    depends(pt->getDecl(), false);
  } else if (const auto *pt = dyn_cast<FunctionProtoType>(t)) {
    depends(pt->getReturnType(), false);
    for (size_t i = 0; i < pt->getNumParams(); i++) {
      depends(pt->getParamType(i), false);
    }
  } else if (QT->isBuiltinType()) {
    if (QT->isBooleanType()) {
      _out.addCHeader("stdbool.h");
    } else if (QT->isWideCharType()) {
      _out.addCHeader("wchar.h");
    } else if (QT->isChar16Type()) {
      _out.addCHeader("uchar.h");
    } else if (QT->isChar32Type()) {
      _out.addCHeader("uchar.h");
    }
  } else {
    std::cerr << "Don't know how to handle type " << t->getTypeClassName() << std::endl;
  }
}

void JobBase::checkReady() {
  if (Verbose) std::cout << "Job Created: " << _name << std::endl;
  if (_depends.empty()) _manager._ready.push(this);
}

void JobBase::run() {
  if (_done) {
    return;
  }
  if (Verbose) std::cout << "Job Started: " << _name << std::endl;
  try {
    impl();
  } catch (const mangling_error &err) {
    std::cerr << "Job Failed: " << _name << " from " << err.what() << std::endl;
    std::exit(1);
  }
  _done = true;
  if (Verbose) std::cout << "Job Done: " << _name << std::endl;
  for (auto *d : _dependent) {
    d->satisfy(this);
  }
  _dependent.clear();
}

void JobBase::satisfy(JobBase *dependency) {
  _depends.remove(dependency);
  if (_depends.empty()) {
    _manager._ready.push(this);
  }
}

template class unplusplus::Job<clang::TypedefDecl>;
template class unplusplus::Job<clang::FunctionDecl>;
template class unplusplus::Job<clang::CXXRecordDecl>;
template class unplusplus::Job<clang::VarDecl>;
template class unplusplus::Job<clang::EnumDecl>;

TypedefJob::TypedefJob(TypedefJob::type *D, Sema &S, JobManager &manage)
    : Job<TypedefJob::type>(D, S, manage) {
  const QualType &QT = _d->getUnderlyingType();
  const Type *t = QT.getTypePtrOrNull()->getUnqualifiedDesugaredType();
  if (const auto *tt = dyn_cast<TagType>(t)) {
    TagDecl *td = tt->getDecl();
    if (td->isEmbeddedInDeclarator()) {
      // This typedef is handled by the anonymous struct or enum decl
      depends(td, false);
      _anonymousStruct = true;
    } else {
      // this typedef renames a library internal class. Its decl was dropped earlier, so we can't
      // refer to it. Substitute the name of this typedef instead, and forward declare the missing
      // type.
      _replacesFiltered = manager().renameFiltered(td, _d);

      if (_replacesFiltered)
        manager().declare(td, this);
      else
        depends(td, false);

      if (td->isUnion())
        _keyword = "union";
      else if (td->isEnum())
        _keyword = "enum";
      else
        _keyword = "struct";
    }
  } else {
    depends(QT, false);
  }
  checkReady();
}

void TypedefJob::impl() {
  if (_anonymousStruct) return;
  _out.hf() << "// " << _location << "\n";
  _out.hf() << "// " << _name << "\n";
  Identifier i(_d, cfg());
  _out.hf() << "#ifdef __cplusplus\n";
  if (_replacesFiltered) {
    _out.hf() << "typedef " << i.cpp << " " << i.c << ";\n";
    _out.hf() << "#else\n";
    _out.hf() << "typedef " << _keyword << " " << i.c << cfg()._struct << " " << i.c << ";\n";
  } else {
    Identifier ti(_d->getUnderlyingType(), Identifier(i.c, cfg()), cfg());
    _out.hf() << "typedef " << ti.cpp << ";\n";
    _out.hf() << "#else\n";
    _out.hf() << "typedef " << ti.c << ";\n";
  }
  _out.hf() << "#endif // __cplusplus\n\n";
}

VarJob::VarJob(VarJob::type *D, Sema &S, JobManager &jm) : Job<VarJob::type>(D, S, jm) {
  if (auto *VTD = _d->getDescribedVarTemplate()) manager().lazyCreate(VTD, S);
  if (auto *VTSD = dyn_cast<VarTemplateSpecializationDecl>(_d))
    manager().lazyCreate(VTSD->getTemplateInstantiationArgs().asArray(), S);

  _ptr = _d->getASTContext().getPointerType(_d->getType());
  _ptr.addConst();
  depends(_ptr, false);
  checkReady();
}

void VarJob::impl() {
  Identifier i(_d, cfg());
  Identifier vi(_ptr, i, cfg());
  _out.hf() << "// " << _location << "\n";
  _out.hf() << "// " << _name << "\n";
  _out.hf() << "extern " << vi.c << ";\n\n";
  _out.sf() << "// " << _location << "\n";
  _out.sf() << "// " << _name << "\n";
  _out.sf() << vi.c << " = &(" << i.cpp << ");\n\n";
}

void JobManager::flush(Sema &S) {
  while (_lazy.size()) {
    create(_lazy.front(), S);
    _lazy.pop();
  }
  while (_ready.size()) {
    _ready.front()->run();
    _ready.pop();
  }
}

JobManager::~JobManager() {
  int incomplete = 0;
  for (auto &j : _jobs) {
    if (!j->isDone()) {
      std::cerr << "Incomplete job: " << j->name() << std::endl;
      for (auto *d : j->dependencies()) {
        std::cerr << "  -> Needs: " << d->name() << std::endl;
      }
      incomplete++;
    }
  }
  if (incomplete > 0) {
    std::cerr << "Error: " << incomplete << " jobs did not finish." << std::endl;
    std::exit(1);
  }
}

void JobManager::traverse(clang::QualType QT, std::function<void(clang::Decl *)> OP) {
  const Type *t = QT.getTypePtrOrNull()->getUnqualifiedDesugaredType();
  if (t == nullptr) {
    return;
  } else if (const auto *tt = dyn_cast<TagType>(t)) {
    OP(tt->getDecl());
  } else if (isa<PointerType>(t) || isa<ReferenceType>(t)) {
    traverse(t->getPointeeType(), OP);
  } else if (const auto *at = dyn_cast<ConstantArrayType>(t)) {
    traverse(at->getElementType(), OP);
  } else if (const auto *pt = dyn_cast<InjectedClassNameType>(t)) {
    OP(pt->getDecl());
  } else if (const auto *pt = dyn_cast<BuiltinType>(t)) {
    ;  // No job
  } else if (const auto *pt = dyn_cast<FunctionProtoType>(t)) {
    traverse(pt->getReturnType(), OP);
    for (size_t i = 0; i < pt->getNumParams(); i++) {
      traverse(pt->getParamType(i), OP);
    }
  } else {
    std::cerr << "Warning: Don't know how to traverse type " << t->getTypeClassName() << std::endl;
  }
}

void JobManager::traverse(const llvm::ArrayRef<clang::TemplateArgument> &Args,
                          std::function<void(clang::Decl *)> OP) {
  for (const auto &Arg : Args) {
    switch (Arg.getKind()) {
      case TemplateArgument::Type:
        traverse(Arg.getAsType(), OP);
        break;
      case TemplateArgument::Declaration:
        OP(Arg.getAsDecl());
        break;
      case TemplateArgument::Pack:
        traverse(Arg.pack_elements(), OP);
        break;
      case TemplateArgument::Template:
        OP(Arg.getAsTemplate().getAsTemplateDecl());
        break;
      case TemplateArgument::Null:
      case TemplateArgument::NullPtr:
      case TemplateArgument::Integral:
        break;  // Ignore
      case TemplateArgument::TemplateExpansion:
        std::cerr << "Warning: Can't traverse template expansion" << std::endl;
        break;
      case TemplateArgument::Expression:
        std::cerr << "Warning: Can't traverse expression" << std::endl;
        break;
    }
  }
}

void JobManager::create(Decl *D, clang::Sema &S) {
  if (!D) return;
  if (_decls.count(D)) return;
  _decls.emplace(D);

  // Test if from a C header that can just be included by the library
  if (_filter.isCHeader(D)) {
    _out.addCHeader(getDeclHeader(D));
    // Create a dummy for jobs needing this type to depend on
    if (isa<TypeDecl>(D)) {
      declare(D, nullptr);
      if (auto *TD = dyn_cast<TagDecl>(D)) {
        if (TD->isCompleteDefinition() || TD->isBeingDefined()) define(D, nullptr);
      }
    }
    return;
  }

  if (_filter.filterOut(D)) return;
  if (D->isTemplated()) create(D->getDescribedTemplate(), S);

  if (auto *SD = dyn_cast<TypedefDecl>(D)) {
    // Can't emit code if the typedef depends on unprovided template parameters
    if (!SD->getUnderlyingType()->isDependentType()) new TypedefJob(SD, S, *this);
  } else if (const auto *SD = dyn_cast<ClassTemplatePartialSpecializationDecl>(D)) {
    // We can't emit code for a template that is only partially specialized
  } else if (auto *SD = dyn_cast<CXXRecordDecl>(D)) {
    if (ClassDeclareJob::accept(SD)) new ClassDeclareJob(SD, S, *this);
    // discovering a template when creating the declaration job can cause the definition to have
    // already been created.
    if (ClassDefineJob::accept(SD, cfg(), S) && !isDefined(SD)) new ClassDefineJob(SD, S, *this);
  } else if (auto *SD = dyn_cast<FunctionDecl>(D)) {
    if (FunctionJob::accept(SD) && !prevDeclared(SD)) new FunctionJob(SD, S, *this);
  } else if (auto *SD = dyn_cast<VarDecl>(D)) {
    if (!SD->isTemplated() && !prevDeclared(SD)) new VarJob(SD, S, *this);
  } else if (auto *SD = dyn_cast<EnumDecl>(D)) {
    new EnumJob(SD, S, *this);
  } else if (auto *SD = dyn_cast<TemplateDecl>(D)) {
    _templates.push(SD);
    if (auto *CTD = dyn_cast<ClassTemplateDecl>(SD)) {
      if (CTD->getTemplatedDecl()->isCompleteDefinition()) {
        for (auto *Special : CTD->specializations()) {
          if (!isDefined(Special)) {
            Special->setSpecializedTemplate(CTD);
            if (ClassDefineJob::accept(Special, cfg(), S) && !isDefined(Special))
              new ClassDefineJob(Special, S, *this);
          }
        }
      }
    }
  } else if (const auto *SD = dyn_cast<TypeAliasTemplateDecl>(D)) {
    ;  // Ignore and hope desugaring looks through it
  } else if (auto *SD = dyn_cast<FieldDecl>(D)) {
    ;  // Ignore, fields are handled in the respective record writer
  } else if (auto *SD = dyn_cast<IndirectFieldDecl>(D)) {
    ;  // Ignore, fields are handled in the respective record writer
  } else if (auto *SD = dyn_cast<AccessSpecDecl>(D)) {
    ;  // Ignore, access for members comes from getAccess
  } else if (auto *SD = dyn_cast<StaticAssertDecl>(D)) {
    ;  // Ignore
  } else if (auto *SD = dyn_cast<TypeAliasDecl>(D)) {
    create(SD->getUnderlyingType(), S);
  } else if (auto *SD = dyn_cast<UsingShadowDecl>(D)) {
    create(SD->getTargetDecl(), S);
  } else if (auto *SD = dyn_cast<UsingDecl>(D)) {
    const NestedNameSpecifier *nn = SD->getQualifier();
    std::string unhandled;
    switch (nn->getKind()) {
      case NestedNameSpecifier::Identifier:
        unhandled = "Identifier";
        break;
      case NestedNameSpecifier::Namespace:
        create(nn->getAsNamespace(), S);
        break;
      case NestedNameSpecifier::NamespaceAlias:
        create(nn->getAsNamespaceAlias(), S);
        break;
      case NestedNameSpecifier::TypeSpec:
      case NestedNameSpecifier::TypeSpecWithTemplate:
        create(QualType(nn->getAsType(), 0), S);
        break;
      case NestedNameSpecifier::Global:
        // a global decl that was hopefully already handled was imported into the parent
        // namespace, but we don't care because it would be redundant.
        break;
      case NestedNameSpecifier::Super:
        unhandled = "Super";
        break;
    }

    if (unhandled.size())
      std::cerr << "Warning: Unhandled Using Declaration " << cfg().getCXXQualifiedName(SD)
                << " of kind " << unhandled
                << " at: " << D->getLocation().printToString(D->getASTContext().getSourceManager())
                << std::endl;
  } else if (auto *SD = dyn_cast<NamespaceAliasDecl>(D)) {
    create(SD->getNamespace(), S);
  } else if (auto *SD = dyn_cast<FriendDecl>(D)) {
    // Ignore: the friend declaration may be for something that doesn't exist
  } else if (auto *SD = dyn_cast<NamespaceDecl>(D)) {
    for (auto ssd : SD->decls()) create(ssd, S);
  } else if (auto *SD = dyn_cast<LinkageSpecDecl>(D)) {
    for (auto ssd : SD->decls()) create(ssd, S);
  } else if (auto *SD = dyn_cast<UsingDirectiveDecl>(D)) {
    create(SD->getNominatedNamespace(), S);
  } else if (auto *SD = dyn_cast<NamedDecl>(D)) {
    std::cerr << "Warning: Unknown Decl kind " << SD->getDeclKindName() << " "
              << cfg().getCXXQualifiedName(SD) << std::endl;
    _out.hf() << "// Warning: Unknown Decl kind " << SD->getDeclKindName() << " "
              << cfg().getCXXQualifiedName(SD) << "\n\n";
  } else {
    std::cerr << "Warning: Ignoring unnamed Decl of kind " << D->getDeclKindName() << "\n";
  }
}

void JobManager::create(QualType QT, clang::Sema &S) {
  traverse(QT, [&](Decl *D) { create(D, S); });
}

void JobManager::create(const llvm::ArrayRef<clang::TemplateArgument> &Args, clang::Sema &S) {
  traverse(Args, [&](Decl *D) { create(D, S); });
}

void JobManager::lazyCreate(clang::Decl *D, clang::Sema &S) {
  if (D && !_decls.count(D)) _lazy.push(D);
}

void JobManager::lazyCreate(clang::QualType QT, clang::Sema &S) {
  traverse(QT, [&](Decl *D) { lazyCreate(D, S); });
}

void JobManager::lazyCreate(const llvm::ArrayRef<clang::TemplateArgument> &Args, clang::Sema &S) {
  traverse(Args, [&](Decl *D) { lazyCreate(D, S); });
}

bool JobManager::isDefined(clang::Decl *D) {
  std::vector<Decl *> prev;
  while (D) {
    if (_definitions.count(D)) {
      for (auto *P : prev) {
        _definitions[P] = _definitions[D];
      }
      return true;
    }
    prev.push_back(D);
    D = D->getPreviousDecl();
  }
  return false;
}

bool JobManager::prevDeclared(clang::Decl *D) {
  Decl *P = D->getPreviousDecl();
  while (P) {
    if (_decls.count(P)) return true;
    P = P->getPreviousDecl();
  }
  return false;
}

bool JobManager::renameFiltered(NamedDecl *D, NamedDecl *New) {
  if (!_renamed.count(D) && _filter.filterOut(D)) {
    // this typedef renames a filtered-out class. Its decl was dropped earlier, so we can't refer to
    // it. Substitute the name of this typedef instead, and forward declare the missing type.
    try {
      Identifier i(New, cfg());
      Identifier::ids[D] = i;
      _renamed.emplace(D);
      return true;
    } catch (const mangling_error &err) {
      return false;
    }
  }
  return false;
}

void JobManager::visitMacros(const Preprocessor &PP) {
  for (const auto &m : PP.macros()) {
    const clang::MacroInfo *mi = m.getSecond().getLatest()->getMacroInfo();
    std::string name(m.getFirst()->getName());

    if (Identifier::dups.count(name)) {
      std::cerr << "Warning: The macro " << name << " at "
                << mi->getDefinitionLoc().printToString(PP.getSourceManager())
                << " shadows an existing declaration "
                << cfg().getDebugName(Identifier::dups.at(name)) << std::endl;
    }
  }
}

void JobManager::finishTemplates(clang::Sema &S) {
  while (_templates.size()) {
    if (_templates.size()) {
      const TemplateDecl *TD = _templates.front();
      if (auto *ctd = dyn_cast<ClassTemplateDecl>(TD)) {
        for (auto *ctsd : ctd->specializations()) {
          create(ctsd, S);
        }
      } else if (auto *ftd = dyn_cast<FunctionTemplateDecl>(TD)) {
        for (auto *ftsd : ftd->specializations()) {
          create(ftsd, S);
        }
      } else if (auto *vtd = dyn_cast<VarTemplateDecl>(TD)) {
        for (auto *vtsd : vtd->specializations()) {
          create(vtsd, S);
        }
      } else if (auto *vtd = dyn_cast<TypeAliasTemplateDecl>(TD)) {
        // ignore
      } else {
        std::cerr << "Warning: template " << cfg().getCXXQualifiedName(TD) << " has unknown kind "
                  << TD->getDeclKindName() << std::endl;
      }
      _templates.pop();
    }
    flush(S);
  }
}
