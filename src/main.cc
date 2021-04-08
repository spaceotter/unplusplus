#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include <list>
#include <filesystem>
#include <json/json.h>
#include "struct.hh"

using namespace std;
namespace fs = std::filesystem;

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
      upp::Writer(hf, sf, root);
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
