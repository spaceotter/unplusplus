/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "enum.hpp"

using namespace unplusplus;
using namespace clang;

EnumJob::EnumJob(type *D, clang::Sema &S, JobManager &jm) : Job<EnumJob::type>(D, S, jm) {
  std::cout << "Job Created: " << _name << std::endl;
  manager().declare(_d, this);
  manager().define(_d, this);
  depends(_d->getIntegerType(), false);
  checkReady();
}

void EnumJob::impl() {
  const TypedefDecl *tdd = getAnonTypedef(_d);
  // is it truly anonymous, with no typedef even?
  bool anon = getName(_d).empty() && tdd == nullptr;

  // if it's anonymous, check if somebody gave it a name already
  Identifier i;
  if (anon) {
    if (Identifier::ids.count(_d)) {
      i = Identifier::ids[_d];
    }
  } else {
    i = Identifier(_d, cfg());
  }
  // now, field i.c should be empty only if there really is no name available.

  _out.hf() << "// " << _location << "\n";
  _out.hf() << "// " << _name << "\n";

  // try to figure _out if C will have the correct type for the enum.

  // if there's a negative value, signed int will be assumed
  bool negative = false;
  for (const auto *e : _d->enumerators())
    if (e->getInitVal().isNegative()) negative = true;

  const ASTContext &AC = _d->getASTContext();
  const Type *int_type = _d->getIntegerType()->getUnqualifiedDesugaredType();
  const Type *expected = (negative ? AC.IntTy : AC.UnsignedIntTy).getTypePtr();
  // generate some macros instead of a typedef enum
  bool macros = int_type != expected;

  if (!anon) {
    _out.hf() << "#ifdef __cplusplus\n";
    _out.hf() << "typedef ";
    if (tdd == nullptr) _out.hf() << "enum ";
    _out.hf() << i.cpp << " " << i.c << ";\n";
    _out.hf() << "#else\n";
  }
  if (!macros && !i.c.empty()) _out.hf() << "typedef ";

  if (macros) {
    for (const auto *e : _d->enumerators()) {
      Identifier entry(e, cfg());
      Identifier entryt(_d->getIntegerType(), Identifier(), cfg());
      _out.hf() << "#define " << entry.c << " ((" << entryt.c << ")";
      _out.hf() << e->getInitVal().toString(10) << ")\n";
    }
    if (!i.c.empty()) {
      Identifier ii(_d->getIntegerType(), Identifier(i.c, cfg()), cfg());
      _out.hf() << "typedef " << ii.c << ";\n";
    }
  } else {
    _out.hf() << "enum ";
    if (!i.c.empty()) _out.hf() << i.c << cfg()._enum << " ";
    _out.hf() << "{\n";
    for (const auto *e : _d->enumerators()) {
      Identifier entry(e, cfg());
      _out.hf() << "  " << entry.c << " = ";
      _out.hf() << e->getInitVal().toString(10) << ",\n";
    }
    _out.hf() << "}";
    if (!i.c.empty()) _out.hf() << " " << i.c;
    _out.hf() << ";\n";
  }

  if (!anon)
    _out.hf() << "#endif // __cplusplus\n\n";
  else
    _out.hf() << "\n";
}
