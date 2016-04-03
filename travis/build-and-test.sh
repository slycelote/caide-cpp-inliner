#!/bin/bash
set -ev

export CXX=g++-4.9
export CC=gcc-4.9

git submodule init
git submodule update src/clang
if [ "$CAIDE_USE_SYSTEM_CLANG" = "OFF" ]
then
    git submodule update src/llvm
fi

mkdir build
cd build
cmake -DCAIDE_USE_SYSTEM_CLANG=$CAIDE_USE_SYSTEM_CLANG ../src
make -j2
ctest --verbose

