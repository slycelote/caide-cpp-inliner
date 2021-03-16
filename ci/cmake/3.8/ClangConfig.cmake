# This file allows users to call find_package(Clang) and pick up our targets.

get_filename_component(CLANG_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)

find_package(LLVM REQUIRED CONFIG HINTS $CLANG_CMAKE_DIR)

# Provide all our library targets to users.
include("${CMAKE_CURRENT_LIST_DIR}/ClangTargets.cmake")
