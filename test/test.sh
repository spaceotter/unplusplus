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
g++ "test2.cpp" -c -o "out/test2.o"
"$UPP" "test2.hpp" -m -T -o "out/clib-test2" -I "."
g++ out/clib-test2.cpp -c -o out/clib-test2.o -I "."
gcc main2.c -c -o "out/main2.o" -I "out"
g++ out/test2.o out/clib-test2.o out/main2.o -o out/test2
./out/test2
