//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include <llvm/ADT/ArrayRef.h>

#include <vector>

namespace clang {
    class Expr;
    class Sema;
    class ClassTemplatePartialSpecializationDecl;
    class TemplateArgument;
    class TemplateArgumentLoc;
    class TemplateDecl;
    class TypeSourceInfo;
}

namespace caide { namespace internal {

class SuppressErrorsInScope {
public:
    explicit SuppressErrorsInScope(clang::Sema& sema_);
    ~SuppressErrorsInScope();

private:
    clang::Sema& sema;
    bool origSuppressAllDiagnostics;
};

// Sugared form of some entities emerging during a specific template instantiation.
struct SugaredSignature {
    // Defaulted template arguments.
    std::vector<clang::TemplateArgumentLoc> templateArgLocs;

    // Sugared constraints, including concept constraints, requires clauses and
    // (for concept instantiation) constraint expression in the concept definition.
    std::vector<clang::Expr*> associatedConstraints;

    // Sugared types of non-type template parameters and (for function template
    // instantiation) sugared function argument types.
    std::vector<clang::TypeSourceInfo*> argTypes;
};

SugaredSignature substituteTemplateArguments(
        clang::Sema&, clang::TemplateDecl*,
        llvm::ArrayRef<clang::TemplateArgument> writtenArgs,
        llvm::ArrayRef<clang::TemplateArgument> args);

SugaredSignature substituteTemplateArguments(
        clang::Sema&, clang::ClassTemplatePartialSpecializationDecl*,
        llvm::ArrayRef<clang::TemplateArgument> args);

}}

