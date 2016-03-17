//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include "SourceLocationComparers.h"

#include <clang/Rewrite/Core/Rewriter.h>

#include <set>
#include <vector>

namespace clang {
    class LangOptions;
    class SourceManager;
}

namespace caide {
namespace internal {

struct RewriteItem {
    clang::SourceRange range;
    clang::Rewriter::RewriteOptions opts;
};

struct RewriteItemComparer {
    explicit RewriteItemComparer(const clang::SourceManager& sourceManager);
    RewriteItemComparer(const RewriteItemComparer&) = default;
    RewriteItemComparer(RewriteItemComparer&&) = default;

    bool operator() (const RewriteItem& lhs, const RewriteItem& rhs) const;
    SourceLocationComparer cmp;
};


class SmartRewriter {
public:
    SmartRewriter(clang::SourceManager& sourceManager, const clang::LangOptions& langOptions);
    SmartRewriter(const SmartRewriter&) = delete;
    SmartRewriter& operator=(const SmartRewriter&) = delete;
    SmartRewriter(SmartRewriter&&) = delete;
    SmartRewriter& operator=(SmartRewriter&&) = delete;

    bool canRemoveRange(const clang::SourceRange& range) const;
    bool removeRange(const clang::SourceRange& range, clang::Rewriter::RewriteOptions opts);
    const clang::RewriteBuffer* getRewriteBufferFor(clang::FileID fileID) const;
    void applyChanges();

private:
    clang::Rewriter rewriter;
    RewriteItemComparer comparer;
    std::set<RewriteItem, RewriteItemComparer> removed;
    bool changesApplied;
};

}
}

