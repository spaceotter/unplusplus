#!/usr/bin/env bash
set -e
set -x
cmake -B build . -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
args="-x c++ -A x86_64-pc-linux-gnu"
orig=test.hh
base=$(basename -s .hh "$orig")
rm -f "$base.*.hh" "$base.spec"
build/bin/c2ffi "$orig" -D null -M "$base.m.hh" -T "$base.t.hh" $args #-o "$base-1.spec"
# the ordering from c2ffi is not determinstic
sort "$base.m.hh" -o "$base.m.hh"
echo "#include \"$base.t.hh\"" > "$base.h.hh"
echo "#include \"$base.m.hh\"" >> "$base.h.hh"
# find out which automatic macro definitions cause problems
build/bin/c2ffi "$base.h.hh" -D null $args 2> "$base.err" || echo "Filtering some bad macro constants"
# erase the lines causing errors from the generated code
grep -F 'test.m.hh' test.err | grep 'error:' | sed 's/^[^:]*:\([0-9]*\):.*/\1 d/' > "$base.sed"
cp "$base.m.hh" "$base.m.hh.orig"
sed -i -f "$base.sed" "$base.m.hh"
build/bin/c2ffi "$base.h.hh" -D json $args -o "$base.spec"
