#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <list>
#include <filesystem>
#include <json/json.h>

using namespace std;
namespace fs = std::filesystem;

unordered_map<string, string> type_map = {
  {":long", "long"},
  {":int", "int"},
  {":void", "void"}
};
unordered_map<int, string> namespaces;
unordered_map<int, string> namespaces_cpp;
struct strct {
  string c;
  string cpp;
};
unordered_map<int, strct> structs;

string separator = "_";
string prefix = "upp_";
string structprefix = "_struct_";
string upp_this = "_upp_this";
string upp_return = "_upp_return";

void handle_namespace(Json::Value &v, ostream &hf, ostream &sf) {
  int id = v["id"].asInt();
  int parent = v["ns"].asInt();
  string name = v["name"].asString();
  if (parent == 0) {
    namespaces[id] = prefix + name;
    namespaces_cpp[id] = name;
  }
  else {
    namespaces[id] = namespaces[parent] + separator + name;
    namespaces_cpp[id] = namespaces_cpp[parent] + "::" + name;
  }
}

void handle_type(Json::Value &v, ostream &out, bool top = true) {
  string tag = v["tag"].asString();
  if (type_map.count(tag)) {
    out << type_map.at(tag) << " ";
  } else if (tag == ":pointer") {
    handle_type(v["type"], out, false);
    out << "*";
  } else if (tag == ":struct") {
    out << structs[v["id"].asInt()].c << " ";
    // avoid passing/returning structs by value
    if (top) {
      out << "*";
    }
  } else {
    out << "/*" << v << "*/ ";
  }
}

void handle_parameters(Json::Value &v, ostream &out) {
  for (unsigned i = 0; i < v.size(); i++) {
    Json::Value p = v[i];
    handle_type(p["type"], out);
    out << p["name"].asString();
    if (i < v.size() - 1) {
      out << ", ";
    }
  }
}

void handle_ctor(Json::Value &v, ostream &hf, ostream &sf, int parent) {
  string name = v["name"].asString();
  string &pname = structs[parent].c;
  string fname = prefix + "new" + separator + pname;
  hf << "// location: " << v["location"].asString() << "\n";
  stringstream ctor_sig;
  ctor_sig << pname << " *" << fname << "(";
  Json::Value ps = v["parameters"];
  handle_parameters(ps, ctor_sig);
  ctor_sig << ")";
  hf << ctor_sig.str() << ";\n";

  sf << "// location: " << v["location"].asString() << "\n";
  sf << ctor_sig.str() << " {\n  ";
  sf << "return new " << pname << "(";
  for (unsigned i = 0; i < ps.size(); i++) {
    Json::Value p = ps[i];
    sf << p["name"].asString();
    if (i < ps.size() - 1) {
      sf << ", ";
    }
  }
  sf << ");\n}\n";
}

void handle_method(Json::Value &v, ostream &hf, ostream &sf, int parent) {
  string name = v["name"].asString();
  string &pname = structs[parent].c;
  string fname = pname + separator + name;
  hf << "// location: " << v["location"].asString() << "\n";
  sf << "// location: " << v["location"].asString() << "\n";
  stringstream method_sig;
  // check if the return type is a struct, and turn it into a pointer argument.
  Json::Value rt = v["return-type"];
  string rs;
  if (rt["tag"].asString() == ":struct") {
    method_sig << "void ";
    rs = structs.at(rt["id"].asInt()).c;
  } else {
    handle_type(rt, method_sig);
  }
  method_sig << fname << "(";
  // a substitute for the C++ this
  method_sig << pname << " *" << upp_this;
  // if needed, return through ptr argument
  if (rs.size() > 0) {
    method_sig << ", " << rs << " *" << upp_return;
  }
  Json::Value ps = v["parameters"];
  if (ps.size() > 0) {
    method_sig << ", ";
    handle_parameters(ps, method_sig);
  }
  method_sig << ")";

  hf << method_sig.str() << ";\n";

  sf << method_sig.str() << " {\n  ";
  if (rs.size() > 0) {
    sf << "*" << upp_return << " = " << upp_this << "->" << name << "(";
  } else {
    if (rt["tag"].asString() != ":void") {
      sf << "return ";
    }
    sf << upp_this << "->" << name << "(";
  }
  for (unsigned i = 0; i < ps.size(); i++) {
    Json::Value p = ps[i];
    sf << p["name"].asString();
    if (i < ps.size() - 1) {
      sf << ", ";
    }
  }

  sf << ");\n}\n";
}

void handle_struct(Json::Value &v, ostream &hf, ostream &sf) {
  string name = v["name"].asString();
  int id = v["id"].asInt();
  int parent = v["ns"].asInt();
  strct s;
  if (parent == 0) {
    s = {prefix + name, name};
  } else {
    s = {namespaces[parent] + separator + name,
      namespaces_cpp[parent] + "::" + name};
  }
  structs[id] = s;

  sf << "// location: " << v["location"].asString() << "\n";
  sf << "// Stubs for C++ struct: " << s.cpp << "\n";

  hf << "// location: " << v["location"].asString() << "\n";
  hf << "#ifdef __cplusplus\n";
  hf << "typedef " << s.cpp << " " << s.c << ";\n";
  hf << "#else\n";
  hf << "typedef struct " << structprefix << s.c << " " << s.c << ";\n";
  hf << "#endif\n";
  Json::Value ms = v["methods"];
  for (unsigned int i = 0; i < ms.size(); i++) {
    Json::Value m = ms[i];
    string tag = m["tag"].asString();
    if (tag == "function") {
      string mname = m["name"].asString();
      if (mname == name) {
        handle_ctor(m, hf, sf, id);
      } else {
        handle_method(m, hf, sf, id);
      }
    } else {
      cout << "Warning: Unkown tag " << tag << endl;
      hf << "/* Could not process: " << m << " */\n";
    }
  }
  string dtor_sig = "void " + prefix + "del" + separator + s.c + "("
                    + s.c + " *" + upp_this + ")";
  hf << dtor_sig << ";\n\n";

  sf << dtor_sig << " {\n  delete " << upp_this << ";\n}\n\n";
}

void handle_entry(Json::Value &v, ostream &hf, ostream &sf) {
  string tag = v["tag"].asString();
  if (tag == "namespace") {
    handle_namespace(v, hf, sf);
  } else if (tag == "struct") {
    handle_struct(v, hf, sf);
  } else {
    cout << "Warning: Unknown tag " << tag << endl;
    hf << "/* " << v << " */\n\n";
  }
}

void header_begin(ostream &out, const string &prefix, const string &header) {
  out << "/*\n";
  out << " * This file was generated automatically by unplusplus.\n";
  out << " */\n";
  out << "#ifndef " << prefix << "_CIFGEN_H\n";
  out << "#define " << prefix << "_CIFGEN_H\n";
  out << "#ifdef __cplusplus\n";
  out << "#include \"" << header << "\"\n";
  out << "extern \"C\" {\n";
  out << "#endif\n\n";
}

void header_end(ostream &out) {
  out << "#ifdef __cplusplus\n";
  out << "}\n";
  out << "#endif\n";
  out << "#endif\n";
}

void source_begin(ostream &out, const string &header) {
  out << "/*\n";
  out << " * This file was generated automatically by unplusplus.\n";
  out << " */\n";
  out << "#include \"" << header << "\"\n\n";
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    cout << "Supply an input json file" << endl;
    return 1;
  }
  fs::path inpath(argv[1]);
  Json::Value root;
  ifstream inf;
  inf.open(inpath);


  if (inf.is_open()) {
    inf >> root;

    string stem = inpath.stem().string();
    fs::path header_path(inpath.parent_path() / (stem + ".h"));
    fs::path source_path(inpath.parent_path() / (stem + ".cc"));
    cout << "Writing code to " << header_path << " and " << source_path << endl;
    ofstream hf(header_path);
    ofstream sf(source_path);

    if (hf.is_open() && sf.is_open()) {
      header_begin(hf, stem, stem + ".hh");
      source_begin(sf, header_path.filename());
      if (root.isArray()) {
        for (unsigned int i = 0; i < root.size(); i++) {
          handle_entry(root[i], hf, sf);
        }
      }
      header_end(hf);
    } else {
      cout << "Error opening header or source file " << header_path << endl;
      return 1;
    }
  } else {
    cout << "Error opening " << argv[1] << endl;
    return 1;
  }
  return 0;
}
