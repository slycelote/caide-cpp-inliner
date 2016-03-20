//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include "IntervalSet.h"
#include "SourceLocationComparers.h"

#include <clang/Rewrite/Core/Rewriter.h>

namespace clang {
    class LangOptions;
    class SourceManager;
}

namespace caide {
namespace internal {

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
    SourceLocationComparer comparer;
    IntervalSet<clang::SourceLocation, SourceLocationComparer> removed;
    bool changesApplied;
};

}
}

