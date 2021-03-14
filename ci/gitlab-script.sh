#!/usr/bin/env bash
set -ev

red="\e[0;91m"
blue="\e[0;94m"
reset="\e[0m"

function caide_timer {
    echo -e "${blue}    Current time: $(date +%H:%M:%S) ${reset}"
}

# Must match the value in .gitlab-ci.yml
export CCACHE_DIR="$PWD/ccache-cache"

caide_timer
apt-get update
caide_timer
apt-get install -y wget software-properties-common apt-transport-https cmake ninja-build ccache g++-5 gcc-5
export CXX=g++-5
export CC=gcc-5
caide_timer

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
    caide_timer
    apt-get install -y -t llvm-toolchain-xenial-"$CAIDE_CLANG_VERSION" clang-"$CAIDE_CLANG_VERSION" libclang-"$CAIDE_CLANG_VERSION"-dev llvm-"$CAIDE_CLANG_VERSION"-dev
    caide_timer

    export CMAKE_PREFIX_PATH=$Clang_ROOT

    # Debug
    llvm-config-"$CAIDE_CLANG_VERSION" --cxxflags --cflags --ldflags --has-rtti
else
    apt-get install -y git
    caide_timer
    git submodule sync
    git submodule update --init
    caide_timer
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
caide_timer

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
    cp --recursive llvm-project/llvm/lib/clang/ lib/
fi

ctest --verbose

if [ "$CAIDE_USE_SYSTEM_CLANG" = "ON" ]
then
    # Work around some packaging errors...
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
            ls -lah /usr/include/clang/
            ls -lah /usr/include/clang/*
            ln -s /usr/lib/llvm-9/lib/clang/9.0.1 /usr/include/clang/9.0.1 || true
            ;;
    esac
else
    ninja install-clang-resource-headers
    # The previous target installs builtin clang headers under llvm-project/, but clang libraries expect to find them under lib/
    # (a bug in clang when it's built as a CMake subproject?)
    cp --recursive llvm-project/llvm/lib/clang/ lib/
fi

ctest --verbose
cat ../tests/temp/gcclog.txt || true

caide_timer

if [ "$CAIDE_USE_SYSTEM_CLANG" = "OFF" ]
then
    # Create an artifact only for this job
    # Must match the path in .gitlab-ci.yml
    echo cp cmd/cmd ..
fi

