/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "jobs.hpp"

#include <clang/AST/DeclFriend.h>
#include <clang/Basic/SourceManager.h>

#include "filter.hpp"
#include "function.hpp"

using namespace clang;
using namespace unplusplus;

JobBase::JobBase(JobManager &manager, clang::Sema &S)
    : _manager(manager), _out(manager.out()), _s(S) {
  _manager._jobs.emplace_back(this);
}

IdentifierConfig &JobBase::cfg() { return _manager.cfg(); }

void JobBase::depends(JobBase *other) {
  if (!other->_done) {
    _depends.emplace(other);
    other->_dependent.emplace(this);
  }
}

void JobBase::depends(clang::Decl *D, bool define) {
  _manager.create(D, _s);
  depends(_manager._declarations.at(D));
  if (define)
    depends(_manager._definitions.at(D));
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
  if (_depends.empty()) _manager._ready.push(this);
}

void JobBase::run() {
  if (_done) {
    //std::cerr << "Error: " << _name << " ran again" << std::endl;
    return;
  }
  impl();
  _done = true;
  std::cout << "Job Done: " << _name << std::endl;
  for (auto *d : _dependent) {
    d->satisfy(this);
  }
  _dependent.clear();
}

void JobBase::satisfy(JobBase *dependency) {
  _depends.erase(dependency);
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
  std::cout << "Job Created: " << _name << std::endl;
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
      _replacesInternal = manager().renameFiltered(td, _d);
      if (td->isUnion())
        _keyword = "union";
      else if (td->isEnum())
        _keyword = "enum";
      else
        _keyword = "struct";
    }
  }
  checkReady();
}

void TypedefJob::impl() {
  if (_anonymousStruct) return;
  _out.hf() << "// " << _location << "\n";
  _out.hf() << "// " << _name << "\n";
  Identifier i(_d, cfg());
  _out.hf() << "#ifdef __cplusplus\n";
  if (_replacesInternal) {
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
}

ClassDefineJob::ClassDefineJob(ClassDefineJob::type *D, clang::Sema &S, JobManager &jm)
    : Job<ClassDefineJob::type>(D, S, jm) {
  _name += " (Definition)";
  std::cout << "Job Created: " << _name << std::endl;
  manager()._definitions[_d] = this;

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

  // initializer lists are special snowflakes that stop existing after an expression is evaluated
  bool no_ctor = getName(_d) == "initializer_list" &&
                 getName(dyn_cast_or_null<Decl>(_d->getParent())) == "std";

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
void ClassDefineJob::impl() {
  _out.hf() << "// " << _location << "\n";
  _out.hf() << "// " << _name << "\n";
}

void JobManager::flush() {
  while (_ready.size()) {
    _ready.front()->run();
    _ready.pop();
  }
}

JobManager::~JobManager() {
  for (auto &j : _jobs) {
    if (!j->isDone()) {
      std::cerr << "Incomplete job: " << j->name();
    }
  }
}

void JobManager::create(QualType QT, clang::Sema &S) {
  const Type *t = QT.getTypePtrOrNull()->getUnqualifiedDesugaredType();
  if (t == nullptr) {
    return;
  } else if (const auto *tt = dyn_cast<TagType>(t)) {
    create(tt->getDecl(), S);
  } else if (isa<PointerType>(t) || isa<ReferenceType>(t)) {
    create(t->getPointeeType(), S);
  } else if (const auto *at = dyn_cast<ConstantArrayType>(t)) {
    create(at->getElementType(), S);
  } else if (const auto *pt = dyn_cast<InjectedClassNameType>(t)) {
    create(pt->getDecl(), S);
  } else if (const auto *pt = dyn_cast<BuiltinType>(t)) {
    ; // No job
  } else if (const auto *pt = dyn_cast<FunctionProtoType>(t)) {
    create(pt->getReturnType(), S);
    for (size_t i = 0; i < pt->getNumParams(); i++) {
      create(pt->getParamType(i), S);
    }
  } else {
    std::cerr << "Don't know how to create job for type " << t->getTypeClassName() << std::endl;
  }
}

void JobManager::create(Decl *D, clang::Sema &S) {
  if (!D) return;
  if (_decls.count(D)) return;
  _decls.emplace(D);

  SourceManager &SM = D->getASTContext().getSourceManager();
  FileID FID = SM.getFileID(SM.getFileLoc(D->getLocation()));
  bool Invalid = false;
  const SrcMgr::SLocEntry &SEntry = SM.getSLocEntry(FID, &Invalid);
  SrcMgr::CharacteristicKind ck = SEntry.getFile().getFileCharacteristic();
  // The declaration is from a C header that can just be included by the library
  if (ck == SrcMgr::CharacteristicKind::C_ExternCSystem) {
    _out.addCHeader(SEntry.getFile().getName().str());
    return;
  }

  if (filterOut(D)) return;

  if (auto *SD = dyn_cast<TypedefDecl>(D)) {
    // Can't emit code if the typedef depends on unprovided template parameters
    if (!SD->getUnderlyingType()->isDependentType()) new TypedefJob(SD, S, *this);
  } else if (const auto *SD = dyn_cast<ClassTemplatePartialSpecializationDecl>(D)) {
    // We can't emit code for a template that is only partially specialized
  } else if (auto *SD = dyn_cast<CXXRecordDecl>(D)) {
    new ClassDeclareJob(SD, S, *this);
    if (auto *CTSD = dyn_cast<ClassTemplateSpecializationDecl>(SD)) {
      if (CTSD->getSpecializedTemplate()->getTemplatedDecl()->isCompleteDefinition()) {
        new ClassDefineJob(SD, S, *this);
      }
    } else {
      new ClassDefineJob(SD, S, *this);
    }
  } else if (auto *SD = dyn_cast<FunctionDecl>(D)) {
    if (FunctionJob::accept(SD)) new FunctionJob(SD, S, *this);
  } else if (auto *SD = dyn_cast<TemplateDecl>(D)) {
    _templates.push(SD);
    if (auto *CTD = dyn_cast<ClassTemplateDecl>(SD)) {
      if (CTD->getTemplatedDecl()->isCompleteDefinition()) {
        for (auto *Special : CTD->specializations()) {
          if (!_definitions.count(Special)) {
            Special->setSpecializedTemplate(CTD);
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
    if (SD->getFriendDecl())
      create(SD->getFriendDecl(), S);
    else if (SD->getFriendType())
      create(SD->getFriendType()->getType(), S);
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

void JobManager::create(const llvm::ArrayRef<clang::TemplateArgument> &Args, clang::Sema &S) {
  for (const auto &Arg : Args) {
    switch (Arg.getKind()) {
      case TemplateArgument::Type:
        create(Arg.getAsType(), S);
        break;
      case TemplateArgument::Declaration:
        create(Arg.getAsDecl(), S);
        break;
      case TemplateArgument::Pack:
        create(Arg.pack_elements(), S);
        break;
      case TemplateArgument::Template:
        create(Arg.getAsTemplate().getAsTemplateDecl(), S);
        break;
      case TemplateArgument::Null:
      case TemplateArgument::NullPtr:
      case TemplateArgument::Integral:
        break;  // Ignore
      case TemplateArgument::TemplateExpansion:
        std::cerr << "Warning: can't forward template expansion" << std::endl;
        break;
      case TemplateArgument::Expression:
        std::cerr << "Warning: can't forward expression" << std::endl;
        break;
    }
  }
}

bool JobManager::renameFiltered(NamedDecl *D, NamedDecl *New) {
  if (!_renamed.count(D) && filterOut(D)) {
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
      } else {
        std::cerr << "Warning: template " << cfg().getCXXQualifiedName(TD) << " has unknown kind "
                  << TD->getDeclKindName() << std::endl;
      }
      _templates.pop();
    }
    flush();
  }
}
