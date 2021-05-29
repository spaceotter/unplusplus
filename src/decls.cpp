/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "decls.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclFriend.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/Basic/SourceManager.h>
#include <llvm/ADT/TypeSwitch.h>

#include <sstream>

#include "cxxrecord.hpp"
#include "enum.hpp"
#include "filter.hpp"
#include "function.hpp"
#include "identifier.hpp"

using namespace unplusplus;
using namespace clang;

static std::string TSKstring(TemplateSpecializationKind TSK) {
  switch (TSK) {
    case clang::TSK_Undeclared:
      return "TSK_Undeclared";
      break;
    case clang::TSK_ImplicitInstantiation:
      return "TSK_ImplicitInstantiation";
      break;
    case clang::TSK_ExplicitSpecialization:
      return "TSK_ExplicitSpecialization";
      break;
    case clang::TSK_ExplicitInstantiationDefinition:
      return "TSK_ExplicitInstantiationDefinition";
      break;
    case clang::TSK_ExplicitInstantiationDeclaration:
      return "TSK_ExplicitInstantiationDeclaration";
      break;
  }
  return "TSK Unknown";
}

static bool isAnonStruct(const QualType &qt) {
  const Type *t = qt.getTypePtrOrNull()->getUnqualifiedDesugaredType();
  if (const auto *tt = dyn_cast<TagType>(t)) {
    TagDecl *td = tt->getDecl();
    return td->isEmbeddedInDeclarator();
  }
  return false;
}

struct TypedefDeclWriter : public DeclWriter<TypedefDecl> {
  TypedefDeclWriter(const type *d, clang::Sema &S, DeclHandler &dh) : DeclWriter(d, dh) {
    const QualType &t = d->getUnderlyingType();
    // This typedef was already handled by the anonymous struct or enum decl
    if (isAnonStruct(t)) return;

    SubOutputs out(_out);
    preamble(out.hf());

    bool replacesInternal = false;
    std::string keyword;
    if (const auto *tt = dyn_cast<TagType>(t->getUnqualifiedDesugaredType())) {
      const TagDecl *td = tt->getDecl();
      // this typedef renames a library internal class. Its decl was dropped earlier, so we can't
      // refer to it. Substitute the name of this typedef instead, and forward declare the missing
      // type.
      replacesInternal = _dh.renameInternal(td, _i);
      if (td->isUnion())
        keyword = "union";
      else if (td->isEnum())
        keyword = "enum";
      else
        keyword = "struct";
    } else {
      out.hf() << "// TCN " << t->getUnqualifiedDesugaredType()->getTypeClassName() << "\n";
    }

    // need to add a forward declaration if the target type is a struct - it may not have been
    // declared already.
    _dh.forward(t, S);
    out.hf() << "#ifdef __cplusplus\n";
    if (replacesInternal) {
      out.hf() << "typedef " << _i.cpp << " " << _i.c << ";\n";
      out.hf() << "#else\n";
      out.hf() << "typedef " << keyword << " " << _i.c << cfg()._struct << " " << _i.c << ";\n";
    } else {
      Identifier ti(t, Identifier(_i.c, cfg()), cfg());
      out.hf() << "typedef " << ti.cpp << ";\n";
      out.hf() << "#else\n";
      out.hf() << "typedef " << ti.c << ";\n";
    }
    out.hf() << "#endif // __cplusplus\n\n";
  }
};

struct VarDeclWriter : DeclWriter<VarDecl> {
  VarDeclWriter(const type *d, clang::Sema &S, DeclHandler &dh) : DeclWriter(d, dh) {
    if (d->isTemplated()) {
      std::cerr << "Warning: Ignored templated var " << _i.cpp << std::endl;
      return;  // ignore unspecialized template decl
    }
    SubOutputs out(_out);
    preamble(out.hf());
    preamble(out.sf());

    QualType ptr = _d->getASTContext().getPointerType(_d->getType());
    _dh.forward(ptr, S);
    ptr.addConst();
    Identifier v(ptr, _i, cfg());
    out.hf() << "extern " << v.c << ";\n\n";

    out.sf() << v.c << " = &(" << _i.cpp << ");\n\n";
  }
};

void DeclHandler::forward(const QualType &qt, clang::Sema &S) {
  const Type *t = qt.getTypePtrOrNull()->getUnqualifiedDesugaredType();
  if (t == nullptr) {
    return;
  } else if (const auto *tt = dyn_cast<TagType>(t)) {
    forward(tt->getDecl(), S);
  } else if (const auto *pt = dyn_cast<PointerType>(t)) {
    forward(pt->getPointeeType(), S);
  } else if (const auto *pt = dyn_cast<ReferenceType>(t)) {
    forward(pt->getPointeeType(), S);
  } else if (const auto *pt = dyn_cast<ConstantArrayType>(t)) {
    forward(pt->getElementType(), S);
  } else if (const auto *pt = dyn_cast<InjectedClassNameType>(t)) {
    forward(pt->getDecl(), S);
  } else if (qt->isBuiltinType()) {
    if (qt->isBooleanType()) {
      _out.addCHeader("stdbool.h");
    } else if (qt->isWideCharType()) {
      _out.addCHeader("wchar.h");
    } else if (qt->isChar16Type()) {
      _out.addCHeader("uchar.h");
    } else if (qt->isChar32Type()) {
      _out.addCHeader("uchar.h");
    }
  } else if (qt->isFunctionProtoType()) {
    return;
  } else {
    std::string s;
    llvm::raw_string_ostream ss(s);
    qt.print(ss, cfg().PP);
    ss.flush();
    std::cerr << "Error: couldn't ensure that type `" << ss.str() << "` is declared." << std::endl;
  }
}

void DeclHandler::forward(const Decl *d, Sema &S) {
  if (!d) return;
  if (_decls.count(d)) return;
  _decls.emplace(d);

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

  if (filterOut(d)) return;

  if (d->isTemplated()) {
    forward(d->getDescribedTemplate(), S);
  }

  try {
    if (const auto *sd = dyn_cast<TypedefDecl>(d)) {
      // Can't emit code if the typedef depends on unprovided template parameters
      if (!sd->getUnderlyingType()->isDependentType())
        _unfinished.emplace(new TypedefDeclWriter(sd, S, *this));
    } else if (const auto *sd = dyn_cast<TypeAliasTemplateDecl>(d))
      ;  // Ignore and hope desugaring looks through it
    else if (const auto *sd = dyn_cast<TemplateDecl>(d)) {
      _templates.push(sd);
      if (const auto *ctd = dyn_cast<ClassTemplateDecl>(sd)) {
        if (ctd->getTemplatedDecl()->isCompleteDefinition()) {
          for (const auto *s : ctd->specializations()) {
            if (_ctsd.count(s)) {
              // complete the template that was postponed due to a missing definition
              const_cast<ClassTemplateSpecializationDecl *>(s)->setSpecializedTemplate(
                  const_cast<ClassTemplateDecl *>(ctd));
              _ctsd.erase(s);
            }
          }
        }
      }
    } else if (const auto *sd = dyn_cast<ClassTemplatePartialSpecializationDecl>(d)) {
      // We can't emit code for a template that is only partially specialized
    } else if (const auto *sd = dyn_cast<ClassTemplateSpecializationDecl>(d)) {
      auto *cd = const_cast<ClassTemplateSpecializationDecl *>(sd);
      if (sd->getSpecializedTemplate()->getTemplatedDecl()->isCompleteDefinition()) {
        _unfinished.emplace(new CXXRecordDeclWriter(cd, S, *this));
      } else {
        // Delay printing the members until the template definition is available. This is caused by
        // forward declarations of templates.
        _ctsd.emplace(std::make_pair(sd, new CXXRecordDeclWriter(cd, S, *this)));
      }
    } else if (const auto *sd = dyn_cast<CXXRecordDecl>(d))
      _unfinished.emplace(new CXXRecordDeclWriter(const_cast<CXXRecordDecl *>(sd), S, *this));
    else if (const auto *sd = dyn_cast<FunctionDecl>(d)) {
      if (!sd->isTemplated()) _unfinished.emplace(new FunctionDeclWriter(sd, S, *this));
    } else if (const auto *sd = dyn_cast<EnumDecl>(d))
      _unfinished.emplace(new EnumDeclWriter(sd, *this));
    else if (const auto *sd = dyn_cast<VarDecl>(d)) {
      if (!sd->isTemplated()) _unfinished.emplace(new VarDeclWriter(sd, S, *this));
    } else if (const auto *sd = dyn_cast<FieldDecl>(d))
      ;  // Ignore, fields are handled in the respective record writer
    else if (const auto *sd = dyn_cast<IndirectFieldDecl>(d))
      ;  // Ignore, fields are handled in the respective record writer
    else if (const auto *sd = dyn_cast<AccessSpecDecl>(d))
      ;  // Ignore, access for members comes from getAccess
    else if (const auto *sd = dyn_cast<StaticAssertDecl>(d))
      ;  // Ignore
    else if (const auto *sd = dyn_cast<TypeAliasDecl>(d))
      forward(sd->getUnderlyingType(), S);
    else if (const auto *sd = dyn_cast<UsingShadowDecl>(d))
      forward(sd->getTargetDecl(), S);
    else if (const auto *sd = dyn_cast<UsingDecl>(d)) {
      const NestedNameSpecifier *nn = sd->getQualifier();
      std::string unhandled;
      switch (nn->getKind()) {
        case NestedNameSpecifier::Identifier:
          unhandled = "Identifier";
          break;
        case NestedNameSpecifier::Namespace:
          forward(nn->getAsNamespace(), S);
          break;
        case NestedNameSpecifier::NamespaceAlias:
          forward(nn->getAsNamespaceAlias(), S);
          break;
        case NestedNameSpecifier::TypeSpec:
        case NestedNameSpecifier::TypeSpecWithTemplate:
          forward(QualType(nn->getAsType(), 0), S);
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
        std::cerr << "Warning: Unhandled Using Declaration " << cfg().getCXXQualifiedName(sd)
                  << " of kind " << unhandled << " at: "
                  << d->getLocation().printToString(d->getASTContext().getSourceManager())
                  << std::endl;
    } else if (const auto *sd = dyn_cast<NamespaceAliasDecl>(d))
      forward(sd->getNamespace(), S);
    else if (const auto *sd = dyn_cast<FriendDecl>(d)) {
      if (sd->getFriendDecl())
        forward(sd->getFriendDecl(), S);
      else if (sd->getFriendType())
        forward(sd->getFriendType()->getType(), S);
    } else if (const auto *sd = dyn_cast<NamespaceDecl>(d))
      for (const auto ssd : sd->decls()) forward(ssd, S);
    else if (const auto *sd = dyn_cast<LinkageSpecDecl>(d))
      for (const auto ssd : sd->decls()) forward(ssd, S);
    else if (const auto *sd = dyn_cast<UsingDirectiveDecl>(d))
      forward(sd->getNominatedNamespace(), S);
    else if (const auto *sd = dyn_cast<NamedDecl>(d)) {
      std::cerr << "Warning: Unknown Decl kind " << sd->getDeclKindName() << " "
                << cfg().getCXXQualifiedName(sd) << std::endl;
      _out.hf() << "// Warning: Unknown Decl kind " << sd->getDeclKindName() << " "
                << cfg().getCXXQualifiedName(sd) << "\n\n";
    } else {
      std::cerr << "Warning: Ignoring unnamed Decl of kind " << d->getDeclKindName() << "\n";
    }
  } catch (const mangling_error err) {
    std::string name = "<none>";
    if (const auto *sd = dyn_cast<NamedDecl>(d)) {
      name = cfg().getCXXQualifiedName(sd);
    }
    std::cerr << "Warning: Ignoring " << err.what() << " " << name << " at "
              << d->getLocation().printToString(d->getASTContext().getSourceManager()) << "\n";
  }
}

void DeclHandler::forward(const ArrayRef<clang::TemplateArgument> &Args, clang::Sema &S) {
  for (const auto &Arg : Args) {
    switch (Arg.getKind()) {
      case TemplateArgument::Type:
        forward(Arg.getAsType(), S);
        break;
      case TemplateArgument::Declaration:
        forward(Arg.getAsDecl(), S);
        break;
      case TemplateArgument::Pack:
        forward(Arg.pack_elements(), S);
        break;
      case TemplateArgument::Template:
        forward(Arg.getAsTemplate().getAsTemplateDecl(), S);
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

void DeclHandler::add(const Decl *d, clang::Sema &S) {
  const Decl *p = dyn_cast<Decl>(d->getDeclContext());
  if (p) {
    switch (p->getKind()) {
      case Decl::Namespace:
      case Decl::TranslationUnit:
        forward(d, S);
        break;
      case Decl::ClassTemplateSpecialization:
      case Decl::CXXRecord:
        forward(p, S);
        break;
      default:
        std::cerr << "Unhandled decl " << cfg().getCXXQualifiedName(d) << " parent is "
                  << p->getDeclKindName() << std::endl;
    }
  }
  while (_unfinished.size()) {
    _unfinished.pop();
  }
}

void DeclHandler::finishTemplates(clang::Sema &S) {
  while (_templates.size() || _unfinished.size() || _ctsd.size()) {
    if (_templates.size()) {
      const TemplateDecl *TD = _templates.front();
      if (const auto *ctd = dyn_cast<ClassTemplateDecl>(TD)) {
        for (const auto *ctsd : ctd->specializations()) {
          if (!_decls.count(ctsd)) {
            std::cerr << "new class specialization" << std::endl;
          }
          add(ctsd, S);
        }
      } else if (const auto *ftd = dyn_cast<FunctionTemplateDecl>(TD)) {
        for (const auto *ftsd : ftd->specializations()) {
          if (!_decls.count(ftsd)) {
            std::cerr << "new function specialization" << std::endl;
          }
          add(ftsd, S);
        }
      } else if (const auto *vtd = dyn_cast<VarTemplateDecl>(TD)) {
        for (const auto *vtsd : vtd->specializations()) {
          if (!_decls.count(vtsd)) {
            std::cerr << "new variable specialization" << std::endl;
          }
          add(vtsd, S);
        }
      } else {
        std::cerr << "Warning: template " << cfg().getCXXQualifiedName(TD) << " has unknown kind "
                  << TD->getDeclKindName() << std::endl;
      }
      _templates.pop();
    }
    _ctsd.clear();
    if (_unfinished.size()) {
      _unfinished.pop();
    }
  }
}

bool DeclHandler::renameInternal(const clang::NamedDecl *d, const Identifier &i) {
  if (!_renamedInternals.count(d) && filterOut(d)) {
    // this typedef renames a filtered-out class. Its decl was dropped earlier, so we can't refer to
    // it. Substitute the name of this typedef instead, and forward declare the missing type.
    Identifier::ids[d] = i;
    _renamedInternals.emplace(d);
    return true;
  }
  return false;
}

bool DeclHandler::isRenamedInternal(const clang::NamedDecl *d) {
  return _renamedInternals.count(d);
}
