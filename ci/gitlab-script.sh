#!/usr/bin/env bash
set -ev

. ci/ci-functions.sh

# Must match the value in .gitlab-ci.yml
export CCACHE_DIR="$PWD/ccache-cache"

ci_timer

apt-get update
ci_timer

apt-get install -y lsb-release wget software-properties-common gnupg ninja-build ccache cmake g++-11 gcc-11
export CXX=g++-11
export CC=gcc-11
ci_timer

if [ "$CAIDE_USE_SYSTEM_CLANG" = "ON" ]
then
    wget https://apt.llvm.org/llvm.sh
    chmod +x llvm.sh
    ./llvm.sh $CAIDE_CLANG_VERSION
    ci_timer
    apt-get install -y clang-"$CAIDE_CLANG_VERSION" libclang-"$CAIDE_CLANG_VERSION"-dev llvm-"$CAIDE_CLANG_VERSION"-dev
    ci_timer

    export Clang_ROOT=/usr/lib/llvm-$CAIDE_CLANG_VERSION
    export CMAKE_PREFIX_PATH=$Clang_ROOT

    # Debug
    llvm-config-"$CAIDE_CLANG_VERSION" --cxxflags --cflags --ldflags --has-rtti
else
    apt-get install -y git
    ci_timer
    git submodule sync
    git submodule update --init
    ci_timer
fi

env | sort
cmake --version
"$CXX" --version
"$CC" --version

mkdir build
cd build
cmake -GNinja -DCAIDE_USE_SYSTEM_CLANG=$CAIDE_USE_SYSTEM_CLANG \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_BUILD_TYPE=MinSizeRel ../src

# First build may run out of memory
ninja || ninja -j1
ci_timer

ctest --verbose

ci_timer

if [ "$CAIDE_USE_SYSTEM_CLANG" = "OFF" ]
then
    # Create an artifact only for this job
    # Must match the path in .gitlab-ci.yml
    cp cmd/cmd ..
fi

