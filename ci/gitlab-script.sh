#!/usr/bin/env bash
set -ev

apt-get update
apt-get install -y lsb-release wget software-properties-common
add-apt-repository ppa:ubuntu-toolchain-r/test

date

if [ "$CAIDE_USE_SYSTEM_CLANG" = "ON" ]
then
    export Clang_ROOT=/usr/lib/llvm-$CAIDE_CLANG_VERSION

    case "$CAIDE_CLANG_VERSION" in
        3.8|3.9|4.0)
            # CMake packaging is broken in these
            export Clang_ROOT="$TRAVIS_BUILD_DIR/travis/cmake/$CAIDE_CLANG_VERSION"
            export LLVM_ROOT="$Clang_ROOT"
            ;;
        *)
            wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
            add-apt-repository "deb http://apt.llvm.org/xenial/   llvm-toolchain-xenial-$CAIDE_CLANG_VERSION  main"
            ;;
    esac

    apt-get update
    apt-get install -y g++-9 gcc-9 ccache clang-"$CAIDE_CLANG_VERSION" libclang-"$CAIDE_CLANG_VERSION"-dev llvm-"$CAIDE_CLANG_VERSION"-dev

    export CMAKE_PREFIX_PATH=$Clang_ROOT

    # Debug
    llvm-config-"$CAIDE_CLANG_VERSION" --cxxflags --cflags --ldflags --has-rtti
else
    git submodule update --init --depth 1
fi

export CXX=g++-9
export CC=gcc-9

env | sort
cmake --version
"$CXX" --version
"$CC" --version
date

mkdir build
cd build
cmake -DCAIDE_USE_SYSTEM_CLANG=$CAIDE_USE_SYSTEM_CLANG \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_BUILD_TYPE=MinSizeRel ../src

# First build may run out of memory
make -j3 || make -j1

date

ctest --verbose

