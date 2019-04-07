//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include <clang/Basic/SourceLocation.h>

namespace clang {
    class CXXConstructorDecl;
    class Decl;
    class RawComment;
}

namespace caide { namespace internal {

clang::CXXConstructorDecl* getInheritedConstructor(clang::CXXConstructorDecl* ctorDecl);
clang::SourceLocation getBeginLoc(const clang::Decl* decl);
clang::SourceLocation getEndLoc(const clang::Decl* decl);
clang::SourceLocation getBeginLoc(const clang::RawComment* comment);
clang::SourceLocation getEndLoc(const clang::RawComment* comment);

}}

