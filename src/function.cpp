/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "function.hpp"

using namespace clang;
using namespace unplusplus;

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
