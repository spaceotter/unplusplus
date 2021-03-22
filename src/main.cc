#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <list>
#include <filesystem>
#include <json/json.h>

using namespace std;
namespace fs = std::filesystem;

unordered_map<int, string> namespaces;
unordered_map<int, string> namespaces_cpp;
struct strct {
  string c;
  string cpp;
};
list<strct> structs;

string separator = "_";
string prefix = "cif_";
string structprefix = "_struct_";

void handle_namespace(Json::Value &v) {
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

void handle_struct(Json::Value &v) {
  string name = v["name"].asString();
  int parent = v["ns"].asInt();
  if (parent == 0) {
    structs.push_back({prefix + name, name});
  } else {
    structs.push_back({namespaces[parent] + separator + name,
                       namespaces_cpp[parent] + "::" + name});
  }
}

void handle_entry(Json::Value &v) {
  cout << v["tag"] << endl;
  string tag = v["tag"].asString();
  if (tag == "namespace") {
    handle_namespace(v);
  } else if (tag == "struct") {
    handle_struct(v);
  }
}

void print_header(ostream &out, const string &prefix) {
  out << "#ifndef " << prefix << "_CIFGEN_H\n";
  out << "#define " << prefix << "_CIFGEN_H\n";
  out << "#ifdef __cplusplus\n";
  out << "extern \"C\" {\n";
  out << "#endif\n";
}

void print_type_declarations(ostream &out, const string &header) {
  out << "#ifdef __cplusplus\n";
  out << "#include \"" << header << "\"\n";
  for (strct st : structs) {
    out << "typedef " << st.cpp << " " << st.c << ";\n";
  }
  out << "#else\n";
  for (strct st : structs) {
    string s = st.c;
    out << "typedef struct " << structprefix << s << " " << s << ";\n";
  }
  out << "#endif\n\n";
}

void print_footer(ostream &out) {
  out << "#ifdef __cplusplus\n";
  out << "}\n";
  out << "#endif\n";
  out << "#endif\n";
}

int main(int argc, char *argv[])
{
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

    if (root.isArray()) {
      for (int i = 0; i < root.size(); i++) {
        handle_entry(root[i]);
      }
    }
    if (hf.is_open()) {
      print_header(hf, inpath.stem().string());
      print_type_declarations(hf, inpath.stem().string() + ".hh");
      print_footer(hf);
    } else {
      cout << "Error opening header file " << header_path << endl;
      return 1;
    }
  } else {
    cout << "Error opening " << argv[1] << endl;
    return 1;
  }
  return 0;
}
