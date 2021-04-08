#pragma once

#include <iostream>
#include <json/json.h>
#include <string>
#include <unordered_map>

namespace upp {

  struct Identifier {
    static std::string cpp_separator;
    // these determine how flattened C names are assembled
    static std::string root_prefix;
    static std::string c_separator;
    static std::string struct_prefix;
    static std::string _this;
    static std::string _return;

    Identifier(const std::string &name);
    Identifier(const Identifier &parent, const std::string &name);
    Identifier(const std::string &c, const std::string &cpp) :
        c(c), cpp(cpp) {}
    Identifier(const Identifier &parent, const std::string &c,
               const std::string &cpp);

    std::string c;   // a name-mangled identifier for C
    std::string cpp; // the fully qualified C++ name
  };

  struct Writer {
    std::ostream &hf;
    std::ostream &sf;
    std::unordered_map<int, Identifier> ns; // namespaces and classes
    Writer(std::ostream &hf, std::ostream &sf, Json::Value &v);
    void write_decl(Json::Value &v);
    void write_struct(Json::Value &v);
    void write_ctor(Json::Value &v, Identifier &p);
    void write_method(Json::Value &v, Identifier &p);
    void write_type(Json::Value &v, std::ostream &out, bool top = true);
    void write_params(Json::Value &v, std::ostream &out, bool sig);
  };

} //namespace upp
