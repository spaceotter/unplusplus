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
#include "identifier.hpp"

using namespace unplusplus;
using namespace clang;

static bool isAnonStruct(const QualType &qt) {
  const Type *t = qt.getTypePtrOrNull()->getUnqualifiedDesugaredType();
  if (const auto *tt = dyn_cast<TagType>(t)) {
    TagDecl *td = tt->getDecl();
    return td->isEmbeddedInDeclarator();
  }
  return false;
}

static std::unordered_set<const TagDecl *> renamedInternals;

struct TypedefDeclWriter : public DeclWriter<TypedefDecl> {
  TypedefDeclWriter(const type *d, DeclHandler &dh) : DeclWriter(d, dh) {
    const QualType &t = d->getUnderlyingType();
    // This typedef was already handled by the anonymous struct or enum decl
    if (isAnonStruct(t)) return;

    SubOutputs out(_out);
    preamble(out.hf());

    bool replacesInternal = false;
    if (const auto *tt = dyn_cast<TagType>(t->getUnqualifiedDesugaredType())) {
      const TagDecl *td = tt->getDecl();
      if (!renamedInternals.count(td) && isLibraryInternal(td)) {
        // this typedef renames a library internal class. Its decl was dropped earlier, so we can't
        // refer to it. Substitute the name of this typedef instead, and forward declare the missing
        // type.
        Identifier::ids[td] = _i;
        replacesInternal = true;
        renamedInternals.emplace(td);
      }
    }

    try {
      // need to add a forward declaration if the target type is a struct - it may not have been
      // declared already.
      _dh.forward(t);
      Identifier ti(t, Identifier(_i.c, cfg()), cfg());
      out.hf() << "#ifdef __cplusplus\n";
      out.hf() << "typedef " << ti.cpp << ";\n";
      out.hf() << "#else\n";
      if (replacesInternal)
        out.hf() << "typedef struct " << _i.c << cfg()._struct << " " << _i.c << ";\n";
      else
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
    if (_d->isDeleted() || _d->isDeletedAsWritten()) return;

    SubOutputs out(_out);
    preamble(out.hf());
    preamble(out.sf());
    // dump the original c++ parameters
    std::string s;
    llvm::raw_string_ostream cxx_params(s);
    cxx_params << "// (";
    bool first = true;
    for (const auto *p : _d->parameters()) {
      if (!first) cxx_params << ", ";
      p->getType().print(cxx_params, cfg().PP, getName(p));
      first = false;
    }
    cxx_params << ")\n";
    out.hf() << cxx_params.str();
    out.sf() << cxx_params.str();

    try {
      QualType qr = d->getReturnType();
      _dh.forward(qr);
      bool ret_param = qr->isRecordType() || qr->isReferenceType();
      if (ret_param) qr = _d->getASTContext().VoidTy;

      // avoid returning structs by value?
      QualType qp;
      const auto *method = dyn_cast<CXXMethodDecl>(d);
      if (method) {
        qp = _d->getASTContext().getRecordType(method->getParent());
        _dh.forward(method->getParent());
        if (method->isConst()) qp.addConst();
      }
      bool ctor = dyn_cast<CXXConstructorDecl>(d);
      bool dtor = dyn_cast<CXXDestructorDecl>(d);
      if (ctor) qr = _d->getASTContext().getPointerType(qp);
      std::stringstream proto;
      std::stringstream call;
      proto << _i.c << "(";
      bool firstP = true;
      if (ret_param) {
        QualType retParamT = d->getReturnType().getDesugaredType(d->getASTContext());
        retParamT.removeLocalConst();
        if (retParamT->isRecordType()) {
          retParamT = d->getASTContext().getPointerType(retParamT);
        } else if (retParamT->isReferenceType()) {
          retParamT = retParamT.getNonReferenceType();
          retParamT.removeLocalConst();
          retParamT = d->getASTContext().getPointerType(retParamT);
        }
        proto << Identifier(retParamT, Identifier(cfg()._return, cfg()), cfg()).c;
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
      for (size_t i = 0; i < d->getNumParams(); i++) {
        const auto &p = d->getParamDecl(i);
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
        std::string pname = getName(p);
        if (pname.empty()) pname = cfg()._root + "arg_" + std::to_string(i);
        Identifier pn(pname, cfg());
        Identifier pi(pt, pn, cfg());
        _dh.forward(pt);
        proto << pi.c;
        call << pn.c;
        firstC = firstP = false;
      }
      proto << ")";
      Identifier signature(qr, Identifier(proto.str()), cfg());
      out.hf() << signature.c << ";\n\n";
      if (d->getDeclContext()->isExternCContext()) {
        out.sf() << "// defined externally\n";
      } else {
        std::string fname = getName(d);
        out.sf() << signature.c << " {\n  ";
        if (dtor) {
          out.sf() << "delete " << cfg()._this;
        } else {
          if (ret_param) {
            out.sf() << "*" << cfg()._return << " = ";
            if (method)
              out.sf() << cfg()._this << "->" << fname;
            else
              out.sf() << _i.cpp;
          } else if (ctor)
            out.sf() << "return new " << Identifier(method->getParent(), cfg()).cpp;
          else {
            out.sf() << "return ";
            if (method)
              out.sf() << cfg()._this << "->" << fname;
            else
              out.sf() << _i.cpp;
          }
          out.sf() << "(";
          out.sf() << call.str();
          out.sf() << ")";
        }
        out.sf() << ";\n}\n\n";
      }
    } catch (const mangling_error err) {
      std::cerr << "Error: " << err.what() << std::endl;
      out.hf() << "// ERROR: " << err.what() << "\n\n";
    }
  }
};

struct EnumDeclWriter : public DeclWriter<EnumDecl> {
  EnumDeclWriter(const type *d, DeclHandler &dh) : DeclWriter(d, dh) {
    SubOutputs out(_out);
    preamble(out.hf());

    // try to figure out if C will have the correct type for the enum.

    // if there's a negative value, signed int will be assumed
    bool negative = false;
    for (const auto *e : _d->enumerators())
      if (e->getInitVal().isNegative()) negative = true;

    const ASTContext &AC = _d->getASTContext();
    const Type *int_type = _d->getIntegerType()->getUnqualifiedDesugaredType();
    const Type *expected = (negative ? AC.IntTy : AC.UnsignedIntTy).getTypePtr();
    // generate some macros instead of a typedef enum
    bool macros = int_type != expected;

    const TypedefDecl *tdd = getAnonTypedef(_d);
    // is it truly anonymous, with no typedef even?
    bool anon = getName(d).empty() && tdd == nullptr;

    if (!anon) {
      out.hf() << "#ifdef __cplusplus\n";
      out.hf() << "typedef ";
      if (tdd == nullptr) out.hf() << "enum ";
      out.hf() << _i.cpp << " " << _i.c << ";\n";
      out.hf() << "#else\n";
      if (!macros) out.hf() << "typedef ";
    }

    if (macros) {
      for (const auto *e : _d->enumerators()) {
        Identifier entry(e, cfg());
        Identifier entryt(d->getIntegerType(), Identifier(), cfg());
        out.hf() << "#define " << entry.c << " ((" << entryt.c << ")";
        out.hf() << e->getInitVal().toString(10) << ")\n";
      }
      if (!anon) {
        Identifier ii(d->getIntegerType(), Identifier(_i.c, cfg()), cfg());
        out.hf() << "typedef " << ii.c << ";\n";
      }
    } else {
      out.hf() << "enum ";
      if (!anon) out.hf() << _i.c << cfg()._enum << " ";
      out.hf() << "{\n";
      for (const auto *e : _d->enumerators()) {
        Identifier entry(e, cfg());
        out.hf() << "  " << entry.c << " = ";
        out.hf() << e->getInitVal().toString(10) << ",\n";
      }
      out.hf() << "}";
      if (!anon) out.hf() << " " << _i.c;
      out.hf() << ";\n";
    }

    if (!anon)
      out.hf() << "#endif // __cplusplus\n\n";
    else
      out.hf() << "\n";
  }
};

struct VarDeclWriter : DeclWriter<VarDecl> {
  VarDeclWriter(const type *d, DeclHandler &dh) : DeclWriter(d, dh) {
    SubOutputs out(_out);
    preamble(out.hf());
    preamble(out.sf());

    QualType ptr = _d->getASTContext().getPointerType(_d->getType());
    _dh.forward(ptr);
    ptr.addConst();
    Identifier v(ptr, _i, cfg());
    out.hf() << "extern " << v.c << ";\n\n";

    out.sf() << v.c << " = &(" << _i.cpp << ");\n\n";
  }
};

void DeclHandler::forward(const QualType &qt) {
  const Type *t = qt.getTypePtrOrNull()->getUnqualifiedDesugaredType();
  if (t == nullptr) {
    return;
  } else if (const auto *tt = dyn_cast<TagType>(t)) {
    forward(tt->getDecl());
  } else if (const auto *pt = dyn_cast<PointerType>(t)) {
    forward(pt->getPointeeType());
  } else if (const auto *pt = dyn_cast<ReferenceType>(t)) {
    forward(pt->getPointeeType());
  } else if (const auto *pt = dyn_cast<ConstantArrayType>(t)) {
    forward(pt->getElementType());
  } else if (qt->isBuiltinType()) {
    if (qt->isBooleanType()) {
      _out.addCHeader("stdbool.h");
    } else if (qt->isWideCharType()) {
      _out.addCHeader("wchar.h");
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

void DeclHandler::forward(const Decl *d) {
  if (_decls.count(d)) return;
  _decls.emplace(d);
  // skip if any previous declaration of the same thing was already processed
  const Decl *pd = d->getPreviousDecl();
  while (pd != nullptr) {
    if (_decls.count(pd)) return;
    pd = pd->getPreviousDecl();
  }

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

  if (const auto *nd = dyn_cast<NamedDecl>(d))
    if (isLibraryInternal(nd)) return;

  try {
    if (const auto *sd = dyn_cast<TypedefDecl>(d))
      _unfinished.emplace(new TypedefDeclWriter(sd, *this));
    else if (const auto *sd = dyn_cast<FunctionTemplateDecl>(d))
      _templates.push(sd);
    else if (const auto *sd = dyn_cast<ClassTemplateDecl>(d))
      _templates.push(sd);
    else if (const auto *sd = dyn_cast<ClassTemplateSpecializationDecl>(d)) {
      _unfinished_templates.emplace(new CXXRecordDeclWriter(sd, *this));
      _templates.push(sd->getSpecializedTemplate());
    } else if (const auto *sd = dyn_cast<CXXRecordDecl>(d))
      _unfinished.emplace(new CXXRecordDeclWriter(sd, *this));
    else if (const auto *sd = dyn_cast<FunctionDecl>(d))
      _unfinished.emplace(new FunctionDeclWriter(sd, *this));
    else if (const auto *sd = dyn_cast<EnumDecl>(d))
      _unfinished.emplace(new EnumDeclWriter(sd, *this));
    else if (const auto *sd = dyn_cast<VarDecl>(d))
      _unfinished.emplace(new VarDeclWriter(sd, *this));
    else if (const auto *sd = dyn_cast<FieldDecl>(d))
      ;  // Ignore, fields are handled in the respective record
    else if (const auto *sd = dyn_cast<AccessSpecDecl>(d))
      ;  // Ignore, access for members comes from getAccess
    else if (const auto *sd = dyn_cast<StaticAssertDecl>(d))
      ;  // Ignore
    else if (const auto *sd = dyn_cast<UsingShadowDecl>(d))
      forward(sd->getTargetDecl());
    else if (const auto *sd = dyn_cast<UsingDecl>(d)) {
      const NestedNameSpecifier *nn = sd->getQualifier();
      switch (nn->getKind()) {
        case NestedNameSpecifier::TypeSpec:
          forward(QualType(nn->getAsType(), 0));
          break;
        default:
          std::cerr << "Warning: Unhandled Using Declaration" << std::endl;
          break;
      }
    } else if (const auto *sd = dyn_cast<NamespaceAliasDecl>(d))
      forward(sd->getNamespace());
    else if (const auto *sd = dyn_cast<FriendDecl>(d)) {
      if (sd->getFriendDecl())
        forward(sd->getFriendDecl());
      else if (sd->getFriendType())
        forward(sd->getFriendType()->getType());
    } else if (const auto *sd = dyn_cast<NamespaceDecl>(d))
      for (const auto ssd : sd->decls()) forward(ssd);
    else if (const auto *sd = dyn_cast<LinkageSpecDecl>(d))
      for (const auto ssd : sd->decls()) forward(ssd);
    else if (const auto *sd = dyn_cast<UsingDirectiveDecl>(d))
      forward(sd->getNominatedNamespace());
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

void DeclHandler::forward(const ArrayRef<clang::TemplateArgument> &Args) {
  for (const auto &Arg : Args) {
    switch (Arg.getKind()) {
      case TemplateArgument::Type:
        forward(Arg.getAsType());
        break;
      case TemplateArgument::Declaration:
        forward(Arg.getAsDecl());
        break;
      case TemplateArgument::Pack:
        forward(Arg.pack_elements());
        break;
      case TemplateArgument::Template:
        forward(Arg.getAsTemplate().getAsTemplateDecl());
        break;
      default:
        std::cerr << "Warning: can't forward template argument" << std::endl;
        break;
    }
  }
}

void DeclHandler::add(const Decl *d) {
  forward(d);
  while (_unfinished.size()) {
    _unfinished.pop();
  }
}

void DeclHandler::finishTemplates(clang::Sema &S) {
  while (_templates.size() || _unfinished_templates.size()) {
    if (_templates.size()) {
      if (const auto *ctd = dyn_cast<ClassTemplateDecl>(_templates.front())) {
        for (const auto *ctsd : ctd->specializations()) {
          add(ctsd);
        }
      } else if (const auto *ftd = dyn_cast<FunctionTemplateDecl>(_templates.front())) {
        for (const auto *ftsd : ftd->specializations()) {
          add(ftsd);
        }
      }
      _templates.pop();
    }
    if (_unfinished_templates.size()) {
      const CXXRecordDeclWriter *writer =
          dynamic_cast<const CXXRecordDeclWriter *>(_unfinished_templates.front().get());
      ClassTemplateSpecializationDecl *crd = const_cast<ClassTemplateSpecializationDecl *>(
          dyn_cast<ClassTemplateSpecializationDecl>(writer->decl()));
      // clang is "lazy" and doesn't add any members that weren't used. We can force them to be
      // added.
      if (!crd->hasDefinition()) {
        SourceLocation L = crd->getLocation();
        S.InstantiateClassTemplateSpecialization(L, crd, TSK_ExplicitInstantiationDeclaration);
        S.InstantiateClassTemplateSpecializationMembers(L, crd,
                                                        TSK_ExplicitInstantiationDeclaration);
      }
      _unfinished_templates.pop();
      while (_unfinished.size()) {
        _unfinished.pop();
      }
    }
  }
}
