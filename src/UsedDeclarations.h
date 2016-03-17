//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include "SourceLocationComparers.h"

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
    explicit UsedDeclarations(clang::SourceManager& sourceManager_);
    void addIfInMainFile(clang::Decl* decl);
    bool contains(clang::Decl* decl) const;

private:
    clang::SourceRange getSourceRange(clang::Decl* decl) const;

    clang::SourceManager& sourceManager;
    std::set<clang::Decl*> usedDecls;
    std::set<clang::SourceRange, ArbitraryRangeComparer> locationsOfUsedDecls;
};


}
}

