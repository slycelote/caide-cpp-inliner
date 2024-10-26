cmake_minimum_required(VERSION 2.8.12)
if(POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)
endif()
if(POLICY CMP0074)
    cmake_policy(SET CMP0074 NEW)
endif()

project(CaideInliner)

option(CAIDE_USE_SYSTEM_CLANG "Use system-installed clang/llvm instead of compiling them from scratch" OFF)

# https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project

if(CAIDE_USE_SYSTEM_CLANG)
    # Clang CMake files don't define Clang_PACKAGE_VERSION. To use a specific version,
    # run cmake with '-DClang_ROOT=/path/to/clang/cmake/files (typically /usr/lib/llvm-{version}).
    find_package(Clang REQUIRED CONFIG)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
else()
    set(LLVM_EXTERNAL_CLANG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/llvm-project/clang" CACHE PATH "" FORCE)
    set(LLVM_ENABLE_PROJECTS clang)
    set(LLVM_TARGETS_TO_BUILD "X86" CACHE STRING "" FORCE)
    option(BUILD_SHARED_LIBS "" OFF)
    option(LLVM_BUILD_TOOLS "" ON)
    option(LLVM_BUILD_EXAMPLES "" OFF)
    option(LLVM_BUILD_TESTS "" OFF)
    option(LLVM_BUILD_DOCS "" OFF)
    option(LLVM_ENABLE_DOXYGEN "" OFF)
    option(LLVM_ENABLE_SPHINX "" OFF)
    option(LLVM_ENABLE_TERMINFO "" OFF)
    option(LLVM_ENABLE_LIBEDIT "" OFF)
    option(LLVM_ENABLE_LIBPFM "" OFF)
    option(LLVM_ENABLE_LIBXML2 "" OFF)
    option(LLVM_ENABLE_OCAMLDOC "" OFF)
    option(LLVM_ENABLE_PLUGINS "" OFF)
    option(LLVM_ENABLE_RTTI "" ON)
    option(LLVM_ENABLE_ZLIB "" OFF)
    option(LLVM_INCLUDE_BENCHMARKS "" OFF)
    option(LLVM_INCLUDE_DOCS "" OFF)
    option(LLVM_INCLUDE_EXAMPLES "" OFF)
    option(LLVM_INCLUDE_GOTESTS "" OFF)
    option(LLVM_INCLUDE_RUNTIMES "" OFF)
    option(LLVM_INCLUDE_TESTS "" OFF)
    option(LLVM_INCLUDE_TOOLS "" ON)
    option(LLVM_INCLUDE_UTILS "" OFF)
    option(WITH_POLLY "" OFF)
    set(HAVE_LIBEDIT false CACHE BOOL "" FORCE)

    add_subdirectory(llvm-project/llvm EXCLUDE_FROM_ALL)

    if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/lib/cmake/clang/ClangConfig.cmake")
        if(POLICY CMP0024)
            cmake_policy(SET CMP0024 OLD)
        endif()
        include("${CMAKE_CURRENT_BINARY_DIR}/lib/cmake/clang/ClangConfig.cmake")
    else()
        # If the above doesn't work, we hardcode certain variables:
        set(LLVM_PACKAGE_VERSION "19.1.2")
        set(LLVM_LIBRARY_DIRS "${CMAKE_CURRENT_BINARY_DIR}/llvm-project/llvm/lib")
        set(LLVM_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/llvm-project/llvm/include"
          "${CMAKE_CURRENT_BINARY_DIR}/llvm-project/llvm/include")
        set(CLANG_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/llvm-project/clang/include"
            "${CMAKE_CURRENT_BINARY_DIR}/llvm-project/llvm/tools/clang/include")
        set(LLVM_DEFINITIONS "")
    endif()

    message(STATUS "Using LLVM from submodule ${LLVM_PACKAGE_VERSION}")
endif()

if(LLVM_PACKAGE_VERSION VERSION_LESS "7")
    message(WARNING "A version of clang/llvm that is too old is detected. Build failure is possible.")
endif()
if(LLVM_PACKAGE_VERSION VERSION_GREATER_EQUAL "20")
    message(WARNING "A version of clang/llvm that is too recent is detected. Build failure is possible.")
endif()

option(CAIDE_LINK_LLVM_DYLIB
       "Link against the LLVM dynamic library"
       ${LLVM_LINK_LLVM_DYLIB})

option(CAIDE_LINK_CLANG_DYLIB
       "Link against the clang dynamic library"
       ${CLANG_LINK_CLANG_DYLIB})

message(STATUS "Link against the LLVM dynamic library: ${CAIDE_LINK_LLVM_DYLIB}")
message(STATUS "Link against the Clang dynamic library: ${CAIDE_LINK_CLANG_DYLIB}")

message(STATUS "LLVM_LIBRARY_DIRS=${LLVM_LIBRARY_DIRS}")
message(STATUS "CLANG_LIBRARY_DIRS=${CLANG_LIBRARY_DIRS}")
message(STATUS "LLVM_INCLUDE_DIRS=${LLVM_INCLUDE_DIRS}")
message(STATUS "CLANG_INCLUDE_DIRS=${CLANG_INCLUDE_DIRS}")
message(STATUS "LLVM_DEFINITIONS=${LLVM_DEFINITIONS}")
message(STATUS "CLANG_DEFINITIONS=${CLANG_DEFINITIONS}")


include(CheckCXXCompilerFlag)

function(add_compiler_option_if_supported opt)
    CHECK_CXX_COMPILER_FLAG("${opt}" compiler_supports_option)
    if(compiler_supports_option)
        add_compile_options("${opt}")
    endif()
endfunction(add_compiler_option_if_supported)

# CXX_STANDARD() is available only in cmake >= 3.
# Set the compiler flag explicitly.
if(LLVM_PACKAGE_VERSION VERSION_LESS "10")
    add_compiler_option_if_supported("-std=c++11")
elseif(LLVM_PACKAGE_VERSION VERSION_LESS "16")
    add_compiler_option_if_supported("-std=c++14")
else()
    add_compiler_option_if_supported("-std=c++17")
endif()

if(MSVC)
    add_compile_options(/W3)
    foreach(flag /permissive-)
        add_compiler_option_if_supported("${flag}")
    endforeach()
    # Disable warnings in clang headers. See https://devblogs.microsoft.com/cppblog/broken-warnings-theory.
    CHECK_CXX_COMPILER_FLAG(/experimental:external compiler_supports_external_options)
    if(compiler_supports_external_options)
        add_compile_options(/experimental:external /external:anglebrackets /external:W0)
    endif()
else()
    foreach(flag -Wall -Wextra -Wshadow -Wlogical-op -Werror=return-type)
        add_compiler_option_if_supported("${flag}")
    endforeach()
endif()


add_library(caideInliner STATIC
    caideInliner.cpp clang_compat.cpp detect_options.cpp DependenciesCollector.cpp inliner.cpp
    MergeNamespacesVisitor.cpp optimizer.cpp OptimizerVisitor.cpp RemoveInactivePreprocessorBlocks.cpp
    sema_utils.cpp SmartRewriter.cpp SourceInfo.cpp SourceLocationComparers.cpp util.cpp)

target_include_directories(caideInliner SYSTEM PRIVATE ${CLANG_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS})
target_compile_definitions(caideInliner PRIVATE ${CLANG_DEFINITIONS} ${LLVM_DEFINITIONS})

if(CAIDE_LINK_CLANG_DYLIB)
    set(CAIDE_INLINER_CLANG_LIBS clang-cpp)
else(CAIDE_LINK_CLANG_DYLIB)
    set(CAIDE_INLINER_CLANG_LIBS clangAST
                                 clangBasic
                                 clangFrontend
                                 clangLex
                                 clangRewrite
                                 clangSema
                                 clangTooling)
endif(CAIDE_LINK_CLANG_DYLIB)

if(CAIDE_LINK_LLVM_DYLIB)
    set(CAIDE_INLINER_LLVM_LIBS  LLVM)
else(CAIDE_LINK_LLVM_DYLIB)
    set(CAIDE_INLINER_LLVM_LIBS  )
endif(CAIDE_LINK_LLVM_DYLIB)

target_link_libraries(caideInliner PRIVATE ${CAIDE_INLINER_CLANG_LIBS} ${CAIDE_INLINER_LLVM_LIBS})

add_subdirectory(cmd)

enable_testing()
add_subdirectory(test-tool)


# Find all libraries that we depend on, including recursive dependencies.
# The result is in topological order.
# This is only needed for linking in external build tools.
function(get_dependencies_recursively_impl target output_var entered_var)
    if(NOT TARGET ${target} OR NOT ${target} MATCHES "^(caide|clang|LLVM).*")
        return()
    endif()

    list(FIND ${entered_var} ${target} idx)
    if(${idx} GREATER "-1")
        return()
    endif()

    list(APPEND ${entered_var} ${target})

    get_target_property(imported ${target} IMPORTED)
    if(imported)
        get_target_property(dependencies ${target} INTERFACE_LINK_LIBRARIES)
    else()
        get_target_property(dependencies ${target} LINK_LIBRARIES)
    endif()

    if(dependencies)
        # message(STATUS "${target} -> ${dependencies}")
        foreach(dep ${dependencies})
            get_dependencies_recursively_impl(${dep} ${output_var} ${entered_var})
        endforeach()
    endif()

    list(APPEND ${output_var} ${target})

    set(${entered_var} ${${entered_var}} PARENT_SCOPE)
    set(${output_var} ${${output_var}} PARENT_SCOPE)
endfunction()

function(get_dependencies_recursively target output_var)
    set(${output_var} "" PARENT_SCOPE)
    set(entered "")
    get_dependencies_recursively_impl(${target} ${output_var} entered)
    list(REVERSE ${output_var})
    set(${output_var} ${${output_var}} PARENT_SCOPE)
endfunction()

get_dependencies_recursively(caideInliner all_libs)
message(STATUS "All libs: ${all_libs}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/caide-libs.txt" "${all_libs}")
unset(all_libs)

