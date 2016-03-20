//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "SmartRewriter.h"
#include "SourceLocationComparers.h"

#include <clang/Basic/SourceManager.h>

#include <stdexcept>

using namespace clang;

namespace caide {
namespace internal {

SmartRewriter::SmartRewriter(clang::SourceManager& srcManager, const clang::LangOptions& langOptions)
    : rewriter(srcManager, langOptions)
    , comparer(srcManager)
    , removed(comparer)
    , changesApplied(false)
{
}

bool SmartRewriter::removeRange(const SourceRange& range, Rewriter::RewriteOptions /*opts*/) {
    removed.add(range.getBegin(), range.getEnd());
    return true;
}

bool SmartRewriter::canRemoveRange(const SourceRange& range) const {
    return !removed.intersects(range.getBegin(), range.getEnd());
}

const RewriteBuffer* SmartRewriter::getRewriteBufferFor(FileID fileID) const {
    return rewriter.getRewriteBufferFor(fileID);
}

void SmartRewriter::applyChanges() {
    if (changesApplied)
        throw std::logic_error("Rewriter changes have already been applied");
    changesApplied = true;
    Rewriter::RewriteOptions opts;
    for (const auto& range : removed)
        rewriter.RemoveText(SourceRange(range.first, range.second), opts);
}

}
}

