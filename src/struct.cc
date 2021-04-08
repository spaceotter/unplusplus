#include "struct.hh"

using namespace std;
using namespace upp;

std::string Identifier::root_prefix = "upp_";
std::string Identifier::c_separator = "_";
std::string Identifier::cpp_separator = "::";
std::string Identifier::struct_prefix = "_struct_";
std::string Identifier::_this = "_upp_this";
std::string Identifier::_return = "_upp_return";

Identifier::Identifier(const std::string &name) :
    c(root_prefix + name), cpp(name) {}

Identifier::Identifier(const Identifier &parent, const std::string &name) :
    Identifier(parent, name, name) {}

Identifier::Identifier(const Identifier &parent, const std::string &c,
                       const std::string &cpp)
    : c(parent.c + c_separator + c),
      cpp(parent.cpp + cpp_separator + cpp) {}

Writer::Writer(std::ostream &hf, std::ostream &sf, Json::Value &v)
    : hf(hf), sf(sf) {
  if (v.isArray()) {
    for (unsigned int i = 0; i < v.size(); i++) {
      write_decl(v[i]);
    }
  }
}

void Writer::write_decl(Json::Value &v) {
  string tag = v["tag"].asString();
  if (tag == "namespace") {
    int id = v["id"].asInt();
    int parent = v["ns"].asInt();
    string name = v["name"].asString();
    if (parent == 0)
      ns.emplace(id, Identifier(name));
    else
      ns.emplace(id, Identifier(ns.at(parent), name));
  } else if (tag == "struct") {
    write_struct(v);
  } else {
    cout << "Warning: Unknown tag " << tag << endl;
    hf << "/* " << v << " */\n\n";
  }
}

void Writer::write_struct(Json::Value &v) {
  string name = v["name"].asString();
  int id = v["id"].asInt();
  int parent = v["ns"].asInt();
  if (v.isMember("template")) {
    Identifier tp = tparams(v["template"]);
    if (parent == 0)
      ns.emplace(id, Identifier(Identifier::root_prefix + name + Identifier::c_separator + tp.c, name + tp.cpp));
    else
      ns.emplace(
          id, Identifier(ns.at(parent), name + Identifier::c_separator + tp.c, name + tp.cpp));
  } else {
    if (parent == 0)
      ns.emplace(id, Identifier(name));
    else
      ns.emplace(id, Identifier(ns.at(parent), name));
  }
  Identifier &s = ns.at(id);

  sf << "// location: " << v["location"].asString() << "\n";
  sf << "// Stubs for C++ struct: " << s.cpp << "\n";

  hf << "// location: " << v["location"].asString() << "\n";
  hf << "#ifdef __cplusplus\n";
  hf << "typedef " << s.cpp << " " << s.c << ";\n";
  hf << "#else\n";
  hf << "typedef struct " << Identifier::struct_prefix << s.c << " " << s.c << ";\n";
  hf << "#endif\n";
  Json::Value ms = v["methods"];
  for (unsigned int i = 0; i < ms.size(); i++) {
    Json::Value m = ms[i];
    string tag = m["tag"].asString();
    if (tag == "function") {
      string mname = m["name"].asString();
      if (mname == name) {
        write_ctor(m, s);
      } else {
        write_method(m, s);
      }
    } else {
      cout << "Warning: Unkown tag " << tag << endl;
      hf << "/* Could not process: " << m << " */\n";
    }
  }
  string dtor_sig = "void " + Identifier::root_prefix + "del" + Identifier::c_separator + s.c + "("
                    + s.c + " *" + Identifier::_this + ")";
  hf << dtor_sig << ";\n\n";

  sf << dtor_sig << " {\n  delete " << Identifier::_this << ";\n}\n\n";
}

static unordered_map<string, string> type_map = {
    {":long", "long"}, {":int", "int"}, {":void", "void"}};

void Writer::write_type(Json::Value &v, ostream &out, bool top) const {
  string tag = v["tag"].asString();
  if (type_map.count(tag)) {
    out << type_map.at(tag) << " ";
  } else if (tag == ":pointer" || tag == ":reference") {
    write_type(v["type"], out, false);
    out << "*";
  } else if (tag == ":struct") {
    out << ns.at(v["id"].asInt()).c << " ";
    // avoid passing/returning structs by value
    if (top) {
      out << "*";
    }
  } else {
    out << "/*" << v << "*/ ";
  }
}

void Writer::write_params(Json::Value &v, ostream &out, bool sig) {
  for (unsigned i = 0; i < v.size(); i++) {
    Json::Value p = v[i];
    if (sig) {
      write_type(p["type"], out);
    } else if (p["type"]["tag"] == ":struct" ||
               p["type"]["tag"] == ":reference") {
      // change to pass-by pointer
      out << "*";
    }
    out << p["name"].asString();
    if (i < v.size() - 1) {
      out << ", ";
    }
  }
}

void Writer::write_ctor(Json::Value &v, Identifier &p) {
  string name = v["name"].asString();
  string fname = Identifier::root_prefix + "new" + Identifier::c_separator + p.c;
  hf << "// location: " << v["location"].asString() << "\n";
  stringstream ctor_sig;
  ctor_sig << p.c << " *" << fname << "(";
  Json::Value ps = v["parameters"];
  write_params(ps, ctor_sig, true);
  ctor_sig << ")";
  hf << ctor_sig.str() << ";\n";

  sf << "// location: " << v["location"].asString() << "\n";
  sf << ctor_sig.str() << " {\n  ";
  sf << "return new " << p.c << "(";
  write_params(ps, sf, false);
  sf << ");\n}\n";
}

unordered_map<string, string> operator_map = {
    {"+", "add"},
    {"*", "mul"},
    {"/", "div"},
    {"-", "sub"},
    {"=", "set"},
    {"()", "call"},
    {"[]", "idx"},
    {" ", "_"}
};

static string sanitize_identifier(string name) {
  string::size_type s;
  for (auto &pair : operator_map) {
    string replace = Identifier::c_separator + pair.second + Identifier::c_separator;
    while (1) {
      s = name.find(pair.first);
      if (s == string::npos) {
        break;
      }
      name.replace(s, pair.first.size(), replace);
    }
  }
  return name;
}

void Writer::write_method(Json::Value &v, Identifier &p) {
  string name = v["name"].asString();
  if (name[0] == '~') {
    // ignore the destructor, we always generate it.
    return;
  }
  string fname = p.c + Identifier::c_separator + sanitize_identifier(name);
  hf << "// location: " << v["location"].asString() << "\n";
  sf << "// location: " << v["location"].asString() << "\n";
  stringstream method_sig;
  // check if the return type is a struct, and turn it into a pointer argument.
  Json::Value rt = v["return-type"];
  string rs;
  if (rt["tag"].asString() == ":struct") {
    method_sig << "void ";
    rs = ns.at(rt["id"].asInt()).c;
  } else {
    write_type(rt, method_sig);
  }
  method_sig << fname << "(";
  // a substitute for the C++ this
  method_sig << p.c << " *" << Identifier::_this;
  // if needed, return through ptr argument
  if (rs.size() > 0) {
    method_sig << ", " << rs << " *" << Identifier::_return;
  }
  Json::Value ps = v["parameters"];
  if (ps.size() > 0) {
    method_sig << ", ";
    write_params(ps, method_sig, true);
  }
  method_sig << ")";

  hf << method_sig.str() << ";\n";

  sf << method_sig.str() << " {\n  ";
  if (rs.size() > 0) {
    sf << "*" << Identifier::_return << " = " << Identifier::_this << "->" << name << "(";
  } else {
    if (rt["tag"].asString() != ":void") {
      sf << "return ";
    }
    sf << Identifier::_this << "->" << name << "(";
  }
  write_params(ps, sf, false);

  sf << ");\n}\n";
}

Identifier Writer::tparam_type(Json::Value &v) const {
  std::stringstream sscpp;
  std::stringstream ssc;
  string tag = v["tag"].asString();
  if (type_map.count(tag)) {
    sscpp << type_map.at(tag);
    ssc << type_map.at(tag);
  } else if (tag == ":pointer" || tag == ":reference") {
    Identifier i = tparam_type(v["type"]);
    ssc << i.c;
    sscpp << i.cpp;
    if (tag == ":pointer") {
      ssc << Identifier::c_separator << "ptr";
      sscpp << "*";
    } else {
      ssc << Identifier::c_separator << "ref";
      sscpp << "&";
    }
  } else if (tag == ":struct") {
    return ns.at(v["id"].asInt());
  } else {
    sscpp << "/*" << v << "*/ ";
    ssc << "unknown";
  }
  return Identifier(sanitize_identifier(ssc.str()), sscpp.str());
}

Identifier Writer::tparams(Json::Value &v) const {
  std::stringstream sscpp;
  std::stringstream ssc;
  sscpp << "<";
  for (unsigned i = 0; i < v.size(); i++) {
    Json::Value &p = v[i];
    string tag = p["tag"].asString();
    if (tag != "parameter") {
      sscpp << "/* " << p << " */";
      ssc << "unknown";
    } else {
      if (!p.isMember("value")) {
        Identifier i = tparam_type(p["type"]);
        ssc << i.c;
        sscpp << i.cpp;
      } else {
        string val = p["value"].asString();
        sscpp << val;
        ssc << val;
      }
    }
    if (i < v.size() - 1) {
      sscpp << ", ";
      ssc << Identifier::c_separator;
    }
  }
  sscpp << ">";
  return Identifier(ssc.str(), sscpp.str());
}
