//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "SourceLocationComparers.h"

#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>


using clang::SourceLocation;
using clang::SourceRange;


namespace caide {
namespace internal {

SourceLocationComparer::SourceLocationComparer(const clang::SourceManager& sourceManager_)
    : sourceManager(sourceManager_)
{}

bool SourceLocationComparer::operator() (const SourceLocation& lhs, const SourceLocation& rhs) const {
    return sourceManager.isBeforeInTranslationUnit(lhs, rhs);
}

bool ArbitraryRangeComparer::operator() (const clang::SourceRange& lhs, const clang::SourceRange& rhs) const {
    clang::SourceLocation lbegin = lhs.getBegin(), rbegin = rhs.getBegin();
    if (lbegin == rbegin)
        return lhs.getEnd() < rhs.getEnd();
    else
        return lbegin < rbegin;
}

}
}

