cmake_minimum_required(VERSION 2.8)
project(CaideInliner)

option(CAIDE_USE_SYSTEM_CLANG "Use system clang/llvm instead of compiling it from scratch" ON)
option(CAIDE_SHARED_LIBRARY "Build and link to shared library instead of static" OFF)

enable_testing()

if(CAIDE_USE_SYSTEM_CLANG)
    # Detect LLVM version
    find_package(LLVM 3.6 REQUIRED CONFIG)

    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
else()
    #set(CMAKE_BUILD_TYPE "Release" CACHE STRING "")
    #set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/clanginstall" CACHE PATH "")
    set(LLVM_EXTERNAL_CLANG_SOURCE_DIR "${CMAKE_SOURCE_DIR}/clang" CACHE PATH "" FORCE)
    set(LLVM_TARGETS_TO_BUILD "X86" CACHE STRING "" FORCE)
    option(LLVM_INCLUDE_TOOLS "" ON)
    option(LLVM_BUILD_TOOLS "" ON)
    option(LLVM_BUILD_EXAMPLES "" OFF)
    option(LLVM_INCLUDE_EXAMPLES "" OFF)
    option(LLVM_BUILD_TESTS "" OFF)
    option(LLVM_INCLUDE_TESTS "" OFF)
    option(LLVM_ENABLE_ZLIB "" OFF)
    option(LLVM_BUILD_DOCS "" OFF)
    option(LLVM_ENABLE_DOXYGEN "" OFF)
    option(LLVM_ENABLE_SPHINX "" OFF)
    option(BUILD_SHARED_LIBS "" OFF)
    # Enable RTTI, even though it may decrease performance.
    # It's consistent with how LLVM is packaged in Debian and doesn't require -fno-rtti code for user code.
    option(LLVM_ENABLE_RTTI "" ON)

    add_subdirectory(llvm)

    # TODO: can we do this instead of hardcoding LLVM_ variables below?
    #include("${CMAKE_BINARY_DIR}/llvm/share/llvm/cmake/LLVMConfig.cmake")

    set(LLVM_PACKAGE_VERSION "3.7")
    message(STATUS "Using built-in LLVM ${LLVM_PACKAGE_VERSION}")

    set(LLVM_LIBRARY_DIRS "${CMAKE_BINARY_DIR}/llvm/lib")
    set(LLVM_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/llvm/include"
        "${CMAKE_SOURCE_DIR}/clang/include" "${CMAKE_BINARY_DIR}/llvm/include"
        "${CMAKE_BINARY_DIR}/llvm/tools/clang/include")
    set(LLVM_DEFINITIONS "-D__STDC_LIMIT_MACROS" "-D__STDC_CONSTANT_MACROS")
endif()

if(LLVM_PACKAGE_VERSION VERSION_GREATER "3.7")
    message(WARNING "A high clang/llvm version is detected. Build failure is possible.")
endif()

set(clang_libs clangTooling clangFrontend clangDriver clangSerialization clangParse
    clangSema clangAnalysis clangRewrite clangEdit clangAST clangLex clangBasic)

if(CAIDE_USE_SYSTEM_CLANG)
    # Find the libraries that correspond to the LLVM components that we wish to use.
    # Run `llvm-config --libs <components>' to get the actual list of libraries.
    llvm_map_components_to_libnames(llvm_libs bitreader core mcparser option)

    # There is no function similar to llvm_map_components_to_libnames for clang libraries.
    # Find them manually.
    foreach(clang_lib IN LISTS clang_libs)
        # Generate a unique name for a variable in which the location
        # of the library will be stored in CMake cache
        set(var_name "${clang_lib}_loc")
        find_library(${var_name} ${clang_lib} HINTS "${LLVM_LIBRARY_DIRS}")
        add_library(${clang_lib} STATIC IMPORTED)
        set_target_properties(${clang_lib} PROPERTIES
            IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
            IMPORTED_LOCATION "${${var_name}}"
        )
    endforeach()
else()
    set(llvm_libs "LLVMOption;LLVMMCParser;LLVMMC;LLVMBitReader;LLVMCore;LLVMSupport")
endif()


include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})


# CXX_STANDARD() is available only in cmake >= 3
# Set c++11 flag explicitly
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
    # Ignore. It may be VS compiler that doesn't need any special flag
endif()


if(MSVC)
    # Use the highest warning level for Visual Studio.
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    # Add warnings for g++
    foreach(flag -Wall -Wextra -Wshadow -Wlogical-op)
        if(CMAKE_CXX_FLAGS MATCHES "${flag}")
        else()
            CHECK_CXX_COMPILER_FLAG("${flag}" compiler_supports_flag)
            if (compiler_supports_flag)
                set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
            endif()
        endif()
    endforeach()
endif()


set(inlinerSources caideInliner.cpp inliner.cpp optimizer.cpp
    RemoveInactivePreprocessorBlocks.cpp SmartRewriter.cpp
    StmtParentMap.cpp util.cpp)

if(CAIDE_SHARED_LIBRARY)
    add_library(caideInliner SHARED ${inlinerSources})
else()
    add_library(caideInliner STATIC ${inlinerSources})
endif()

if(NOT CAIDE_USE_SYSTEM_CLANG)
    # caideInliner requires some headers that are generated during the build of clang and llvm
    add_dependencies(caideInliner ${llvm_libs})
    add_dependencies(caideInliner ${clang_libs})
endif()

add_subdirectory(cmd)
add_subdirectory(test-tool)

