#!/usr/bin/env bash
set -ev

red="\e[0;91m"
blue="\e[0;94m"
reset="\e[0m"

function caide_timer {
    echo -e "${blue}    Current time: $(date +%H:%M:%S) ${reset}"
}

# Must match the value in .cirrus.yml
export CCACHE_DIR="$PWD/ccache-cache"

if [ "$CIRRUS_OS" = "linux" ]
then
    caide_timer
    apt-get update
    caide_timer
    apt-get install -y wget software-properties-common apt-transport-https cmake ninja-build ccache
    caide_timer

    add-apt-repository ppa:ubuntu-toolchain-r/test
    apt-get update
    caide_timer
    apt-get install -y g++-9 gcc-9
    caide_timer

    export CXX=g++-9
    export CC=gcc-9

else
    caide_timer
    pkg update -f
    caide_timer
    pkg install -y gcc9 cmake ninja ccache
    caide_timer
    export CXX=g++9
    export CC=gcc9
fi

date

if [ "$CAIDE_USE_SYSTEM_CLANG" = "ON" ]
then
    export Clang_ROOT=/usr/lib/llvm-$CAIDE_CLANG_VERSION

    case "$CAIDE_CLANG_VERSION" in
        3.8|3.9|4.0)
            # CMake packaging is broken in these
            export Clang_ROOT="$PWD/travis/cmake/$CAIDE_CLANG_VERSION"
            export LLVM_ROOT="$Clang_ROOT"
            ;;
        *)
            wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
            add-apt-repository "deb http://apt.llvm.org/xenial/   llvm-toolchain-xenial-$CAIDE_CLANG_VERSION  main"
            apt-get update
            caide_timer
            ;;
    esac

    apt-get install -y clang-"$CAIDE_CLANG_VERSION" libclang-"$CAIDE_CLANG_VERSION"-dev llvm-"$CAIDE_CLANG_VERSION"-dev

    export CMAKE_PREFIX_PATH=$Clang_ROOT

    # Debug
    llvm-config-"$CAIDE_CLANG_VERSION" --cxxflags --cflags --ldflags --has-rtti
else
    if [ "$CIRRUS_OS" = "linux" ]
    then
        apt-get install -y git
    else
        pkg install -y git
    fi
    caide_timer
    git submodule sync
    git submodule update --init
fi

env | sort
cmake --version
"$CXX" --version
"$CC" --version
caide_timer

mkdir build
cd build
cmake -GNinja -DCAIDE_USE_SYSTEM_CLANG=$CAIDE_USE_SYSTEM_CLANG \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_BUILD_TYPE=MinSizeRel ../src

ninja -j2 || ninja -j1

caide_timer

ctest --verbose

caide_timer

