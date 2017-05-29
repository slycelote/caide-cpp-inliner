#!/bin/bash
set -ev

export CXX=g++-4.9
export CC=gcc-4.9

env
cmake --version
"${CXX}" --version
"${CC}" --version
date

git submodule init
git submodule update src/clang
if [ "$CAIDE_USE_SYSTEM_CLANG" = "OFF" ]
then
    git submodule update src/llvm
else
    # Tell CMake where to look for LLVMConfig
    case "$CAIDE_CLANG_VERSION" in
        3.6|3.7|3.8)
            export LLVM_DIR=/usr/share/llvm-$CAIDE_CLANG_VERSION/
            ;;

        *)
            export LLVM_DIR=/usr/lib/llvm-$CAIDE_CLANG_VERSION/
            ;;
    esac
fi

date

mkdir build
cd build
cmake -DCAIDE_USE_SYSTEM_CLANG=$CAIDE_USE_SYSTEM_CLANG ../src
# First build may run out of memory
make -j2 || make -j1

date

ctest --verbose

