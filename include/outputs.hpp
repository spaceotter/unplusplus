/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "identifier.hpp"

namespace unplusplus {
class Outputs {
 public:
  virtual std::ostream &hf() = 0;
  virtual std::ostream &sf() = 0;
  virtual void addCHeader(const std::string &path) = 0;
};

class FileOutputs : public Outputs {
  std::filesystem::path _outheader;
  std::filesystem::path _outsource;
  std::ofstream _hf;
  std::ofstream _sf;
  std::string _macroname;
  std::unordered_set<std::string> _cheaders;

 public:
  FileOutputs(const std::filesystem::path &stem, const std::vector<std::string> &sources);
  ~FileOutputs();
  std::ostream &hf() override { return _hf; };
  std::ostream &sf() override { return _sf; };
  void finalize();
  void addCHeader(const std::string &path) override { _cheaders.emplace(path); }
};

class SubOutputs : public Outputs {
  Outputs &_parent;
  std::ostringstream _hf;
  std::ostringstream _sf;

 public:
  explicit SubOutputs(Outputs &parent) : _parent(parent) {}
  ~SubOutputs();
  std::ostream &hf() override { return _hf; };
  std::ostream &sf() override { return _sf; };
  void addCHeader(const std::string &path) override { _parent.addCHeader(path); }
  void erase() {
    _hf.str("");
    _sf.str("");
  }
};
}  // namespace unplusplus
