/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "outputs.hpp"

using namespace unplusplus;
using std::filesystem::path;

static void sanitize(std::string &name) {
  for (char &c : name) {
    if (!std::isalnum(c) && c != '_') {
      c = '_';
    }
  }
}

FileOutputs::FileOutputs(const path &stem, const std::vector<std::string> &sources)
    : _outheader(path(stem).concat(".h")),
      _hf(_outheader),
      _sf(path(stem).concat(".cpp")) {
  _macroname = stem.filename().string();
  sanitize(_macroname);
  _hf << "/*\n";
  _hf << " * This header file was generated automatically by unplusplus.\n";
  _hf << " */\n";
  _hf << "#ifndef " << _macroname << "_CIFGEN_H\n";
  _hf << "#define " << _macroname << "_CIFGEN_H\n";
  _hf << "#ifdef __cplusplus\n";
  for (const auto &src : sources) {
    _hf << "#include \"" << src << "\"\n";
  }
  _hf << "extern \"C\" {\n";
  _hf << "#endif // __cplusplus\n\n";

  _sf << "/*\n";
  _sf << " * This source file was generated automatically by unplusplus.\n";
  _sf << " */\n";
  _sf << "#include \"" << (std::string)_outheader << "\"\n\n";
}

FileOutputs::~FileOutputs() {
  _hf << "#ifdef __cplusplus\n";
  _hf << "} // extern \"C\"\n";
  _hf << "#endif // __cplusplus\n";
  _hf << "#endif // " << _macroname << "_CIFGEN_H\n";
}

void FileOutputs::finalize() {
  if (!_cheaders.empty()) {
    _hf << "// These C system headers were used by the C++ library\n";
    _hf << "#ifndef __cplusplus\n";
    for (const auto &p : _cheaders) {
      _hf << "#include \"" << p << "\"\n";
    }
    _hf << "#endif // __cplusplus\n\n";
  }
}

SubOutputs::~SubOutputs() {
  _parent.hf() << _hf.str();
  _parent.sf() << _sf.str();
}
