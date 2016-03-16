//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include "SmartRewriter.h"

#include <clang/Basic/SourceLocation.h>

#include <set>


namespace clang {
    class Decl;
    class SourceManager;
    class Rewriter;
}

namespace caide {
namespace internal {


// Tracks which declarations are 'used', i.e. are called directly or indirectly from int main().
class UsedDeclarations {
public:
    UsedDeclarations(clang::SourceManager& sourceManager_, clang::Rewriter& rewriter_);
    void addIfInMainFile(clang::Decl* decl);
    bool isUsed(clang::Decl* decl) const;

private:
    clang::SourceRange getSourceRange(clang::Decl* decl) const;

    clang::SourceManager& sourceManager;
    SourceRangeComparer cmp;
    std::set<clang::Decl*> usedDecls;
    std::set<clang::SourceRange, SourceRangeComparer> locationsOfUsedDecls;
};


}
}

