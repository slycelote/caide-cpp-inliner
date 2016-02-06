#!/bin/bash
set -ev

export CXX=g++-4.9
export CC=gcc-4.9

git submodule init
git submodule update --depth 1 src/clang
if [ "$CAIDE_USE_SYSTEM_CLANG" = "OFF" ]
then
    git submodule update --depth 1 src/llvm
fi

mkdir build
cd build
cmake -DCAIDE_USE_SYSTEM_CLANG=$CAIDE_USE_SYSTEM_CLANG ../src
make -j2 cmd test-tool
ctest

