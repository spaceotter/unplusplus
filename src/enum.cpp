/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "enum.hpp"

using namespace unplusplus;
using namespace clang;

EnumDeclWriter::EnumDeclWriter(const type *d, DeclHandler &dh) : DeclWriter(d, dh, false) {
  const TypedefDecl *tdd = getAnonTypedef(_d);
  // is it truly anonymous, with no typedef even?
  bool anon = getName(_d).empty() && tdd == nullptr;

  // if it's anonymous, check if somebody gave it a name already
  if (anon) {
    if (Identifier::ids.count(_d)) {
      _i = Identifier::ids[_d];
    }
  } else {
    _i = Identifier(_d, cfg());
  }
  // now, field _i.c should be empty only if there really is no name available.

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

  if (!anon) {
    out.hf() << "#ifdef __cplusplus\n";
    out.hf() << "typedef ";
    if (tdd == nullptr) out.hf() << "enum ";
    out.hf() << _i.cpp << " " << _i.c << ";\n";
    out.hf() << "#else\n";
  }
  if (!macros && !_i.c.empty()) out.hf() << "typedef ";

  if (macros) {
    for (const auto *e : _d->enumerators()) {
      Identifier entry(e, cfg());
      Identifier entryt(_d->getIntegerType(), Identifier(), cfg());
      out.hf() << "#define " << entry.c << " ((" << entryt.c << ")";
      out.hf() << e->getInitVal().toString(10) << ")\n";
    }
    if (!_i.c.empty()) {
      Identifier ii(_d->getIntegerType(), Identifier(_i.c, cfg()), cfg());
      out.hf() << "typedef " << ii.c << ";\n";
    }
  } else {
    out.hf() << "enum ";
    if (!_i.c.empty()) out.hf() << _i.c << cfg()._enum << " ";
    out.hf() << "{\n";
    for (const auto *e : _d->enumerators()) {
      Identifier entry(e, cfg());
      out.hf() << "  " << entry.c << " = ";
      out.hf() << e->getInitVal().toString(10) << ",\n";
    }
    out.hf() << "}";
    if (!_i.c.empty()) out.hf() << " " << _i.c;
    out.hf() << ";\n";
  }

  if (!anon)
    out.hf() << "#endif // __cplusplus\n\n";
  else
    out.hf() << "\n";
}
