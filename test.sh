#!/usr/bin/env bash
set -e
set -x
cmake -B build . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
args="-i /usr/include -i /usr/lib/gcc/x86_64-pc-linux-gnu/10.2.0/include/ -x c++ -A x86_64-pc-linux-gnu"
build/bin/c2ffi test.h -D null -M test.h.m $args
echo "#include \"test.h\"" > test.h.h
echo "#include \"test.h.m\"" >> test.h.h
build/bin/c2ffi test.h.h -D json $args -o test.spec
