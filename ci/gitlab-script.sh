#!/usr/bin/env bash
set -ev

. ci/ci-functions.sh

# Must match the value in .gitlab-ci.yml
export CCACHE_DIR="$PWD/ccache-cache"

ci_timer

apt-get update
ci_timer

apt-get install -y lsb-release wget software-properties-common gnupg ninja-build ccache g++-7 gcc-7
export CXX=g++-7
export CC=gcc-7
ci_timer

wget https://github.com/Kitware/CMake/releases/download/v3.20.1/cmake-3.20.1-linux-x86_64.tar.gz
tar xf cmake-3.20.1-linux-x86_64.tar.gz
rm *.tar.gz
export PATH="$PATH":"$PWD/cmake-3.20.1-linux-x86_64/bin"
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

if [ "$CAIDE_USE_SYSTEM_CLANG" = "OFF" ]
then
    ninja install-clang-resource-headers
    # The previous target installs builtin clang headers under llvm-project/, but clang libraries expect to find them under lib/
    # (a bug in clang when it's built as a CMake subproject?)
    ln -s "$PWD"/llvm-project/llvm/lib/clang lib/clang
fi

ctest --verbose

ci_timer

if [ "$CAIDE_USE_SYSTEM_CLANG" = "OFF" ]
then
    # Create an artifact only for this job
    # Must match the path in .gitlab-ci.yml
    echo cp cmd/cmd ..
fi

