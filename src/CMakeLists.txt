cmake_minimum_required(VERSION 2.8.12)
project(CaideInliner)

option(CAIDE_USE_SYSTEM_CLANG "Use system clang/llvm instead of compiling it from scratch" OFF)

if(CAIDE_USE_SYSTEM_CLANG)
    find_package(LLVM REQUIRED CONFIG)
    if(NOT LLVM_FOUND)
        message(FATAL_ERROR "LLVM not found! Make sure packages llvm-dev and libclang-dev are installed")
    endif()

    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
else()
    set(LLVM_EXTERNAL_CLANG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/clang" CACHE PATH "" FORCE)
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
    option(LLVM_ENABLE_TERMINFO "" OFF)
    option(WITH_POLLY "" OFF)
    set(HAVE_LIBEDIT false CACHE BOOL "" FORCE)
    # Enable RTTI, even though it may decrease performance.
    # It's consistent with how LLVM is packaged in Debian and doesn't require -fno-rtti code for user code.
    option(LLVM_ENABLE_RTTI "" ON)

    add_subdirectory(llvm EXCLUDE_FROM_ALL)

    # TODO: can we do this instead of hardcoding LLVM_ variables below?
    #include("${CMAKE_BINARY_DIR}/cmake/modules/CMakeFiles/LLVMConfig.cmake")

    set(LLVM_PACKAGE_VERSION "8.0.1")
    message(STATUS "Using built-in LLVM ${LLVM_PACKAGE_VERSION}")

    set(LLVM_LIBRARY_DIRS "${CMAKE_CURRENT_BINARY_DIR}/llvm/lib")
    set(LLVM_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/llvm/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/clang/include" "${CMAKE_CURRENT_BINARY_DIR}/llvm/include"
        "${CMAKE_CURRENT_BINARY_DIR}/llvm/tools/clang/include")
    set(LLVM_DEFINITIONS "-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS")
endif()

if(LLVM_PACKAGE_VERSION VERSION_LESS "3.6")
    message(WARNING "A version of clang/llvm that is too old is detected. Build failure is possible.")
endif()
if(LLVM_PACKAGE_VERSION VERSION_GREATER "8.0.255")
    message(WARNING "A version of clang/llvm that is too recent is detected. Build failure is possible.")
endif()

# List of clang libraries that we use, including recursive dependencies.
# Must be in topological order.
# Need to include only libraries required for linking.
set(clang_libs clangTooling clangFrontend clangDriver clangSerialization clangParse
    clangSema clangAnalysis clangRewrite clangEdit clangAST clangLex clangBasic)

# LLVM components that clang_libs depend on.
if(LLVM_PACKAGE_VERSION VERSION_LESS "3.9")
    set(llvm_components bitreader mc mcparser option core support)
else()
    set(llvm_components bitreader mc mcparser option core support profiledata)
endif()

llvm_map_components_to_libnames(llvm_libs ${llvm_components})

if(CAIDE_USE_SYSTEM_CLANG)
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
endif()

message(STATUS "LLVM_INCLUDE_DIRS=${LLVM_INCLUDE_DIRS}")
message(STATUS "LLVM_DEFINITIONS=${LLVM_DEFINITIONS}")

# Find libraries corresponding to LLVM components that clang_libs (recursively) depend on.
# The result is in topological order.
# Equivalent of `llvm-config --link-static --libs <components>'.
explicit_map_components_to_libraries(all_llvm_libs ${llvm_components})
message(STATUS "LLVM libs: ${all_llvm_libs}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/caide-libs.txt" "${clang_libs};${all_llvm_libs}")

add_definitions(${LLVM_DEFINITIONS})


include(CheckCXXCompilerFlag)

function(add_compiler_option_if_supported opt)
    CHECK_CXX_COMPILER_FLAG("${opt}" compiler_supports_option)
    if(compiler_supports_option)
        add_compile_options("${opt}")
    endif()
endfunction(add_compiler_option_if_supported)

# CXX_STANDARD() is available only in cmake >= 3
# Set c++11 flag explicitly
add_compiler_option_if_supported("-std=c++11")

if(MSVC)
    # Use the highest warning level for Visual Studio.
    add_compile_options("/W4")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    # Add warnings for g++
    foreach(flag -Wall -Wextra -Wshadow -Wlogical-op -Werror=return-type)
        add_compiler_option_if_supported("${flag}")
    endforeach()
endif()


set(inlinerSources caideInliner.cpp clang_compat.cpp DependenciesCollector.cpp inliner.cpp MergeNamespacesVisitor.cpp
    optimizer.cpp OptimizerVisitor.cpp RemoveInactivePreprocessorBlocks.cpp SmartRewriter.cpp
    SourceInfo.cpp SourceLocationComparers.cpp util.cpp)

add_library(caideInliner STATIC ${inlinerSources})

target_include_directories(caideInliner SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(caideInliner PRIVATE ${clang_libs} ${llvm_libs})

add_subdirectory(cmd)

enable_testing()
add_subdirectory(test-tool)

