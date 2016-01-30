#pragma once
#include <clang/Basic/Version.h>

#define CAIDE_CLANG_VERSION_AT_LEAST(major, minor) \
    ((major) < CLANG_VERSION_MAJOR || \
    (major) == CLANG_VERSION_MAJOR && (minor) <= CLANG_VERSION_MINOR)

#if !CAIDE_CLANG_VERSION_AT_LEAST(3,6)
# error "clang version >= 3.6 is required"
#endif

