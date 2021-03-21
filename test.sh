#!/usr/bin/env bash
set -e
set -x
cmake -B build . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
args="-i /usr/include -i /usr/lib/gcc/x86_64-pc-linux-gnu/10.2.0/include/ -x c++ -A x86_64-pc-linux-gnu"
orig=test.hh
base=$(basename -s .hh "$orig")
build/bin/c2ffi "$orig" -D null -M "$base.m.hh" -T "$base.t.hh" $args
#echo "#include \"$orig\"" > "$base.h.hh"
echo "#include \"test.t.hh\"" > "$base.h.hh"
echo "#include \"test.m.hh\"" >> "$base.h.hh"
build/bin/c2ffi "$base.h.hh" -D json $args -o test.spec
