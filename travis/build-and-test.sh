#!/bin/bash
set -ev

if [ $(uname) == "Linux" ]
then
    export CXX=g++-9
    export CC=gcc-9
else
    export HOMEBREW_NO_INSTALL_CLEANUP=1
    export HOMEBREW_NO_AUTO_UPDATE=1
    brew install ccache
fi

date

if [ "$CAIDE_USE_SYSTEM_CLANG" = "ON" ]
then
    if [ $(uname) == "Linux" ]
    then
        # Debug
        llvm-config-"$CAIDE_CLANG_VERSION" --cxxflags --cflags --ldflags --has-rtti

        export Clang_ROOT=/usr/lib/llvm-$CAIDE_CLANG_VERSION

        case "$CAIDE_CLANG_VERSION" in
            3.8|3.9|4.0)
                # CMake packaging is broken in these
                export Clang_ROOT="$TRAVIS_BUILD_DIR/travis/cmake/$CAIDE_CLANG_VERSION"
                export LLVM_ROOT="$Clang_ROOT"
                ;;
        esac

        export CMAKE_PREFIX_PATH=$Clang_ROOT
    fi
    cmake_options=""
else
    git submodule update --init --depth 1
    cmake_options="-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache"
fi

env | sort
cmake --version
"$CXX" --version
"$CC" --version
"$CXX" -x c++ -c -v /dev/null -o /dev/null
date

mkdir build
cd build
cmake -DCAIDE_USE_SYSTEM_CLANG=$CAIDE_USE_SYSTEM_CLANG \
    $cmake_options \
    -DCMAKE_BUILD_TYPE=MinSizeRel ../src

# First build may run out of memory
make -j3 || make -j1

date

ctest --verbose

