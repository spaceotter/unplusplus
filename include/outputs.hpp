/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "identifier.hpp"

namespace unplusplus {
class Outputs {
 protected:
  const IdentifierConfig &_cfg;
 public:
  Outputs(const IdentifierConfig &cfg) : _cfg(cfg) {}
  virtual std::ostream &hf() = 0;
  virtual std::ostream &sf() = 0;
  const IdentifierConfig &cfg() const { return _cfg; }
};

class FileOutputs : public Outputs {
  std::filesystem::path _outheader;
  std::ofstream _hf;
  std::ofstream _sf;
  std::string _macroname;
 public:
  FileOutputs(const std::filesystem::path &stem, const std::vector<std::string> &sources,
              const IdentifierConfig &cfg);
  ~FileOutputs();
  std::ostream &hf() override { return _hf; };
  std::ostream &sf() override { return _sf; };
};

class SubOutputs : public Outputs {
  Outputs &_parent;
  std::ostringstream _hf;
  std::ostringstream _sf;
 public:
  SubOutputs(Outputs &parent) : Outputs(parent.cfg()), _parent(parent) {}
  ~SubOutputs();
  std::ostream &hf() override { return _hf; };
  std::ostream &sf() override { return _sf; };
};
}
