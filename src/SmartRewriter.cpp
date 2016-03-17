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

RewriteItemComparer::RewriteItemComparer(const clang::SourceManager& sourceManager)
    : cmp(sourceManager)
{}

bool RewriteItemComparer::operator() (const RewriteItem& lhs, const RewriteItem& rhs) const {
    return cmp(lhs.range.getBegin(), rhs.range.getBegin());
}

SmartRewriter::SmartRewriter(clang::SourceManager& srcManager, const clang::LangOptions& langOptions)
    : rewriter(srcManager, langOptions)
    , comparer(srcManager)
    , removed(comparer)
    , changesApplied(false)
{
}

bool SmartRewriter::removeRange(const SourceRange& range, Rewriter::RewriteOptions opts) {
    if (!canRemoveRange(range))
        return false;
    removed.insert({range, opts});
    return true;
}

bool SmartRewriter::canRemoveRange(const SourceRange& range) const {
    if (removed.empty())
        return true;

    RewriteItem ri;
    ri.range = range;

    auto i = removed.lower_bound(ri);
    // i->range.getBegin() >= range.getBegin()

    const SourceManager& srcManager = rewriter.getSourceMgr();

    if (i != removed.end() && !srcManager.isBeforeInTranslationUnit(range.getEnd(), i->range.getBegin()))
        return false;

    if (i == removed.begin())
        return true;

    --i;
    // i->range.getBegin() < range.getBegin()

    return srcManager.isBeforeInTranslationUnit(i->range.getEnd(), range.getBegin());
}

const RewriteBuffer* SmartRewriter::getRewriteBufferFor(FileID fileID) const {
    return rewriter.getRewriteBufferFor(fileID);
}

void SmartRewriter::applyChanges() {
    if (changesApplied)
        throw std::logic_error("Rewriter changes have already been applied");
    changesApplied = true;
    for (const RewriteItem& ri : removed)
        rewriter.RemoveText(ri.range, ri.opts);
}

}
}

