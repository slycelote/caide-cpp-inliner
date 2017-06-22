//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include <clang/AST/DeclBase.h>
#include <clang/Basic/SourceLocation.h>

#include <map>
#include <set>
#include <utility>
#include <vector>


namespace clang {
    class Decl;
    class FunctionDecl;
    class VarDecl;
}

namespace caide {
namespace internal {

// Contains dependency graph and other information shared between optimizer stages.
struct SourceInfo {
    // key: Decl, value: what the key uses.
    std::map<clang::Decl*, std::set<clang::Decl*>> uses;

    // 'Roots of the dependency graph':
    // - int main()
    // - declarations marked with a comment '/// caide keep'
    std::set<clang::Decl*> declsToKeep;

    // Delayed parsed functions.
    std::vector<clang::FunctionDecl*> delayedParsedFunctions;

    // value: non-implicit Decl, key: location and kind of the decl.
    std::map<std::pair<clang::SourceLocation, clang::Decl::Kind>, clang::Decl*> nonImplicitDecls;

    static std::pair<clang::SourceLocation, clang::Decl::Kind> makeKey(clang::Decl* nonImplicitDecl);
};

}
}

