//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include "clang_version.h"

#include <clang/Basic/SourceLocation.h>
#include <llvm/Support/Casting.h>

namespace clang {
    class CXXConstructorDecl;
    class Decl;
    class RawComment;
    class SourceManager;
    class TemplateArgument;
    class TemplateSpecializationType;
}

namespace caide { namespace internal {

clang::CXXConstructorDecl* getInheritedConstructor(clang::CXXConstructorDecl* ctorDecl);
clang::SourceLocation getBeginLoc(const clang::Decl* decl);
clang::SourceLocation getEndLoc(const clang::Decl* decl);
clang::SourceLocation getBeginLoc(const clang::RawComment* comment);
clang::SourceLocation getEndLoc(const clang::RawComment* comment);
bool isWrittenInBuiltinFile(const clang::SourceManager& srcManager,
    clang::SourceLocation location);

unsigned getNumArgs(const clang::TemplateSpecializationType& templateSpecType);
const clang::TemplateArgument* getArgs(
        const clang::TemplateSpecializationType& templateSpecType);

#if CAIDE_CLANG_VERSION_AT_LEAST(15, 0)
using llvm::dyn_cast_if_present;
#else
template <typename To, typename From>
auto* dyn_cast_if_present(From&& val) {
    if (val) {
        return llvm::dyn_cast<To>(val);
    } else {
        return nullptr;
    }
}
#endif

}}

