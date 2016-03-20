//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include <clang/Basic/SourceLocation.h>

namespace clang {
    class SourceManager;
    class SourceRange;
}

namespace caide {
namespace internal {

// Unlike operator<, compares SourceLocations by their relative order in the source file.
struct SourceLocationComparer {
    explicit SourceLocationComparer(const clang::SourceManager& sourceManager);
    SourceLocationComparer(const SourceLocationComparer&) = default;
    SourceLocationComparer(SourceLocationComparer&&) = default;

    bool operator() (clang::SourceLocation lhs, clang::SourceLocation rhs) const;
    const clang::SourceManager& sourceManager;
};

// Compares SourceRanges in some arbitrary order.
struct ArbitraryRangeComparer {
    bool operator() (const clang::SourceRange& lhs, const clang::SourceRange& rhs) const;
};

}
}

