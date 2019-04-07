//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "clang_compat.h"
#include "clang_version.h"

#include <clang/AST/DeclCXX.h>


using namespace clang;

namespace caide { namespace internal {

CXXConstructorDecl* getInheritedConstructor(CXXConstructorDecl* ctorDecl) {
#if CAIDE_CLANG_VERSION_AT_LEAST(3,9)
    return ctorDecl->getInheritedConstructor().getConstructor();
#else
    return const_cast<CXXConstructorDecl*>(ctorDecl->getInheritedConstructor());
#endif
}


SourceLocation getBeginLoc(const Decl* decl) {
#if CAIDE_CLANG_VERSION_AT_LEAST(8,0)
    return decl->getBeginLoc();
#else
    return decl->getLocStart();
#endif
}

SourceLocation getEndLoc(const Decl* decl) {
#if CAIDE_CLANG_VERSION_AT_LEAST(8,0)
    return decl->getEndLoc();
#else
    return decl->getLocEnd();
#endif
}

SourceLocation getBeginLoc(const RawComment* comment) {
#if CAIDE_CLANG_VERSION_AT_LEAST(8,0)
    return comment->getBeginLoc();
#else
    return comment->getLocStart();
#endif
}

SourceLocation getEndLoc(const RawComment* comment) {
#if CAIDE_CLANG_VERSION_AT_LEAST(8,0)
    return comment->getEndLoc();
#else
    return comment->getLocEnd();
#endif
}

}}

