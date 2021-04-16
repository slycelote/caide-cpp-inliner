#!/usr/bin/env bash
set -ev

. ci/ci-functions.sh

# Must match the value in .gitlab-ci.yml
export CCACHE_DIR="$PWD/ccache-cache"

ci_timer
apt-get update
ci_timer
apt-get install -y wget software-properties-common apt-transport-https ninja-build ccache g++-5 gcc-5
export CXX=g++-5
export CC=gcc-5
ci_timer
wget https://github.com/Kitware/CMake/releases/download/v3.20.1/cmake-3.20.1-linux-x86_64.tar.gz
tar xf cmake-3.20.1-linux-x86_64.tar.gz
rm *.tar.gz
export PATH="$PATH":"$PWD/cmake-3.20.1-linux-x86_64/bin"

if [ "$CAIDE_USE_SYSTEM_CLANG" = "ON" ]
then
    export Clang_ROOT=/usr/lib/llvm-$CAIDE_CLANG_VERSION

    case "$CAIDE_CLANG_VERSION" in
        3.8|3.9|4.0)
            # CMake packaging is broken in these
            export Clang_ROOT="$PWD/ci/cmake/$CAIDE_CLANG_VERSION"
            export LLVM_ROOT="$Clang_ROOT"
            ;;
    esac

    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
    add-apt-repository "deb http://apt.llvm.org/xenial/   llvm-toolchain-xenial-$CAIDE_CLANG_VERSION  main"
    apt-get update
    ci_timer
    apt-get install -y -t llvm-toolchain-xenial-"$CAIDE_CLANG_VERSION" clang-"$CAIDE_CLANG_VERSION" libclang-"$CAIDE_CLANG_VERSION"-dev llvm-"$CAIDE_CLANG_VERSION"-dev
    ci_timer

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

if [ "$CAIDE_USE_SYSTEM_CLANG" = "ON" ]
then
    # Work around some packaging issues...
    case "$CAIDE_CLANG_VERSION" in
        3.8)
            mkdir -p lib/clang
            ln -s /usr/include/clang/3.8 lib/clang/3.8.1
            ;;
        3.9)
            mkdir -p lib/clang
            ln -s /usr/include/clang/3.9 lib/clang/3.9.1
            ;;
        9)
            ls -lah /usr/include/clang/*
            ln -s /usr/lib/llvm-9/lib/clang/9.0.1 /usr/include/clang/9.0.1 || true
            ;;
    esac
else
    ninja install-clang-resource-headers
    # The previous target installs builtin clang headers under llvm-project/, but clang libraries expect to find them under lib/
    # (a bug in clang when it's built as a CMake subproject?)
    ln -s "$PWD"/llvm-project/llvm/lib/clang lib/clang
fi

ctest --verbose || true

ci_timer

if [ "$CAIDE_USE_SYSTEM_CLANG" = "OFF" ]
then
    # Create an artifact only for this job
    # Must match the path in .gitlab-ci.yml
    cp cmd/cmd ..
fi

