#!/usr/bin/env bash
set -e
set -x
bindir="${1:-./build/bin}"
testdir="${2:-./test}"
UPP="$(realpath $bindir/unplusplus)"

rm -rf "$testdir/out"
mkdir -p "$testdir/out"
cd "$testdir"

# TEST 2
clang++ "geom.cpp" -c -o "out/geom.o"
"$UPP" "geom.hpp" -o "out/clib-geom"
clang++ out/clib-geom.cpp -c -o out/clib-geom.o -I "."
clang geom_main.c -c -o "out/main.o" -I "out"
clang++ out/geom.o out/clib-geom.o out/main.o -o out/geom
./out/geom
