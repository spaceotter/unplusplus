/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "function.hpp"

using namespace clang;
using namespace unplusplus;

FunctionDeclWriter::FunctionDeclWriter(const type *d, Sema &S, DeclHandler &dh)
    : DeclWriter(d, dh) {
  if (_d->isTemplated()) {
    std::cerr << "Warning: Ignored templated function " << _i.cpp << std::endl;
    return;  // ignore unspecialized template decl
  }
  bool extc = _d->isExternC();
  if (_d->isDeleted() || _d->isDeletedAsWritten()) return;

  SubOutputs out(_out);
  preamble(out.hf());
  if (!extc) preamble(out.sf());
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

  // Types redefined by unplusplus may conflict with the C++ ones
  if (extc) out.hf() << "#ifndef __cplusplus\n";

  try {
    QualType qr = d->getReturnType();
    _dh.forward(qr, S);
    bool ret_param = qr->isRecordType() || qr->isReferenceType();
    if (ret_param) qr = _d->getASTContext().VoidTy;

    // avoid returning structs by value?
    QualType qp;
    const auto *method = dyn_cast<CXXMethodDecl>(d);
    if (method) {
      qp = _d->getASTContext().getRecordType(method->getParent());
      _dh.forward(method->getParent(), S);
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
      _dh.forward(pt, S);
      proto << pi.c;
      call << pn.c;
      firstC = firstP = false;
    }
    proto << ")";
    Identifier signature(qr, Identifier(proto.str()), cfg());
    out.hf() << signature.c << ";\n";
    if (extc) {
      out.hf() << "#endif // !__cplusplus\n\n";
      out.sf() << "// defined externally\n\n";
    } else {
      out.hf() << "\n";
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

bool FunctionJob::accept(const type *D) {
  return !D->isTemplated() && !D->isDeleted() && !D->isDeletedAsWritten();
}

FunctionJob::FunctionJob(FunctionJob::type *D, clang::Sema &S, JobManager &jm)
    : Job<FunctionJob::type>(D, S, jm) {
  std::cout << "Job Created: " << _name << std::endl;
  if (auto *l = _d->getTemplateSpecializationArgs()) {
    manager().create(_d->getDescribedFunctionTemplate(), S);
    manager().create(l->asArray(), S);
  }
  depends(_d->getReturnType(), false);
  _paramTypes.resize(_d->getNumParams());
  _paramDeref.resize(_d->getNumParams(), false);
  for (size_t i = 0; i < _d->getNumParams(); i++) {
    QualType QP(_d->getParamDecl(i)->getType());
    if (QP->isRecordType()) {
      QP = _d->getASTContext().getPointerType(QP);
      _paramDeref[i] = true;
    } else if (QP->isReferenceType()) {
      QP = _d->getASTContext().getPointerType(QP.getNonReferenceType());
      _paramDeref[i] = true;
    }
    _paramTypes[i] = QP;
    depends(QP, false);
  }
  if (auto *M = dyn_cast<CXXMethodDecl>(_d)) {
    depends(M->getParent(), false);
  }
  checkReady();
}

void FunctionJob::impl() {
  bool extc = _d->isExternC();
  _out.hf() << "// " << _location << "\n";
  _out.hf() << "// " << _name << "\n";
  if (!extc) {
    _out.sf() << "// " << _location << "\n";
    _out.sf() << "// " << _name << "\n";
  }

  // Types redefined by unplusplus may conflict with the C++ ones
  if (extc) _out.hf() << "#ifndef __cplusplus\n";

  QualType qr = _d->getReturnType();
  bool ret_param = qr->isRecordType() || qr->isReferenceType();
  if (ret_param) qr = _d->getASTContext().VoidTy;

  QualType qp;
  const auto *method = dyn_cast<CXXMethodDecl>(_d);
  if (method) {
    qp = _d->getASTContext().getRecordType(method->getParent());
    if (method->isConst()) qp.addConst();
  }
  bool ctor = isa<CXXConstructorDecl>(_d);
  bool dtor = isa<CXXDestructorDecl>(_d);
  if (ctor) qr = _d->getASTContext().getPointerType(qp);

  std::stringstream proto;
  std::stringstream call;
  Identifier i(_d, cfg());
  proto << i.c << "(";
  bool firstP = true;
  if (ret_param) {
    QualType retParamT = _d->getReturnType().getDesugaredType(_d->getASTContext());
    retParamT.removeLocalConst();
    if (retParamT->isRecordType()) {
      retParamT = _d->getASTContext().getPointerType(retParamT);
    } else if (retParamT->isReferenceType()) {
      retParamT = retParamT.getNonReferenceType();
      retParamT.removeLocalConst();
      retParamT = _d->getASTContext().getPointerType(retParamT);
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
  for (size_t i = 0; i < _d->getNumParams(); i++) {
    const auto &p = _d->getParamDecl(i);
    if (!firstP) proto << ", ";
    if (!firstC) call << ", ";
    QualType pt = _paramTypes[i];
    if (_paramDeref[i]) call << "*";
    std::string pname = getName(p);
    if (pname.empty()) pname = cfg()._root + "arg_" + std::to_string(i);
    Identifier pn(pname, cfg());
    Identifier pi(pt, pn, cfg());
    proto << pi.c;
    call << pn.c;
    firstC = firstP = false;
  }
  proto << ")";
  Identifier signature(qr, Identifier(proto.str()), cfg());
  _out.hf() << signature.c << ";\n";
  if (extc) {
    _out.hf() << "#endif // !__cplusplus\n\n";
    _out.sf() << "// defined externally\n\n";
  } else {
    _out.hf() << "\n";
    std::string fname = getName(_d);
    _out.sf() << signature.c << " {\n  ";
    if (dtor) {
      _out.sf() << "delete " << cfg()._this;
    } else {
      if (ret_param) {
        _out.sf() << "*" << cfg()._return << " = ";
        if (method)
          _out.sf() << cfg()._this << "->" << fname;
        else
          _out.sf() << i.cpp;
      } else if (ctor)
        _out.sf() << "return new " << Identifier(method->getParent(), cfg()).cpp;
      else {
        _out.sf() << "return ";
        if (method)
          _out.sf() << cfg()._this << "->" << fname;
        else
          _out.sf() << i.cpp;
      }
      _out.sf() << "(";
      _out.sf() << call.str();
      _out.sf() << ")";
    }
    _out.sf() << ";\n}\n\n";
  }
}
