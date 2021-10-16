//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once
#include <clang/Basic/Version.h>

#define CAIDE_CLANG_VERSION_AT_LEAST(major, minor) \
    ((major) < CLANG_VERSION_MAJOR || \
    (major) == CLANG_VERSION_MAJOR && (minor) <= CLANG_VERSION_MINOR)

