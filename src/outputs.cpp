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
      _outsource(path(stem).concat(".cpp")),
      _hf(_outheader),
      _sf(_outsource) {
  if (_hf.fail()) {
    std::cerr << "Error: failed to open " << _outheader << " for writing!" << std::endl;
    std::exit(1);
  }
  if (_sf.fail()) {
    std::cerr << "Error: failed to open " << _outsource << " for writing!" << std::endl;
    std::exit(1);
  }
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
  _sf << "#include \"" << _outheader.string() << "\"\n\n";

  _exclude_headers.emplace("bits/mathcalls.h");
}

FileOutputs::~FileOutputs() {
  _hf << "#ifdef __cplusplus\n";
  _hf << "} // extern \"C\"\n";
  _hf << "#endif // __cplusplus\n";
  _hf << "#endif // " << _macroname << "_CIFGEN_H\n";
}

void FileOutputs::addCHeader(const std::string &path) {
  if (_cheaders.count(path)) return;
  bool use = true;
  for (auto &h : _exclude_headers) {
    if (path.size() >= h.size() && path.substr(path.size() - h.size()) == h) use = false;
  }
  if (use) _cheaders.emplace(path);
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
