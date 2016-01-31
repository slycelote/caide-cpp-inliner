cmake_minimum_required(VERSION 2.8)
project(Caide)

# Detect LLVM version
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(MINIMAL_SUPPORTED_CLANG_VERSION "3.6")

if(LLVM_PACKAGE_VERSION VERSION_LESS MINIMAL_SUPPORTED_CLANG_VERSION)
    message(FATAL_ERROR "clang/llvm version >= ${MINIMAL_SUPPORTED_CLANG_VERSION} is required")
endif()

# Find out which clang libraries we need. Note that the order of libraries matters.
if(LLVM_PACKAGE_VERSION VERSION_EQUAL "3.6")
    set(clang_lib_names "clangTooling;clangFrontend;clangDriver;clangSerialization;clangParse;clangSema;clangAnalysis;clangRewrite;clangEdit;clangAST;clangLex;clangBasic")

else()
    if(LLVM_PACKAGE_VERSION VERSION_GREATER "3.7")
        message(WARNING "A high clang/llvm version is detected. Build failure is possible.")
    endif()

    set(clang_lib_names "clangTooling;clangFrontend;clangDriver;clangSerialization;clangParse;clangSema;clangAnalysis;clangRewrite;clangEdit;clangAST;clangLex;clangBasic")
endif()


# CXX_STANDARD() is available only in cmake >= 3
# Set c++11 flag explicitly
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(COMPILER_SUPPORTS_CXX11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
    # Ignore. It may be VS compiler that doesn't need any special flag
endif()


include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Now define build artifacts and how to link them

add_library(caideInliner STATIC caideInliner.cpp inliner.cpp optimizer.cpp
    RemoveInactivePreprocessorBlocks.cpp SmartRewriter.cpp
    StmtParentMap.cpp util.cpp)

add_executable(cmd cmd.cpp)

target_link_libraries(cmd caideInliner)

# Link against clang libraries
set(clang_libs "")
foreach(clang_lib IN LISTS clang_lib_names)
    # Generate a unique name for a variable in which the location
    # of the library will be stored in CMake cache
    set(var_name "${clang_lib}_loc")

    find_library(${var_name} ${clang_lib} HINTS "${LLVM_LIBRARY_DIRS}")

    if(${var_name})
        list(APPEND clang_libs ${${var_name}})
    else()
        message(WARNING "lib${clang_lib}.a not found")
    endif()
endforeach()

target_link_libraries(cmd ${clang_libs})

# Find the libraries that correspond to the LLVM components that we wish to use.
# Run `llvm-config --libs <components>' to get the actual list of libraries.
llvm_map_components_to_libnames(llvm_libs bitreader core mcparser option)

# Link against LLVM libraries
target_link_libraries(cmd ${llvm_libs})

