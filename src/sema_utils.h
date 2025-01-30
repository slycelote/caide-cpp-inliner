//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include <vector>

namespace clang {
    class CallExpr;
    class Decl;
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

struct SugaredSignature {
    std::vector<clang::TemplateArgument> templateArgs;
    std::vector<clang::TypeSourceInfo*> argTypes;
    std::vector<clang::Expr*> associatedConstraints;
};

// For a function template call, return sugared types that are
// instantiated as part of this call.
SugaredSignature getSugaredSignature(clang::Sema&, clang::CallExpr*);

std::vector<clang::TemplateArgumentLoc> substituteDefaultTemplateArguments(
        clang::Sema&, clang::TemplateDecl*,
        const clang::TemplateArgument* args, unsigned numArgs);

std::vector<clang::TemplateArgumentLoc> substituteTemplateArguments(
        clang::Sema&, clang::ClassTemplatePartialSpecializationDecl*,
        const clang::TemplateArgument* args, unsigned numArgs);

clang::Expr* substituteTemplateArguments(
        clang::Sema&, clang::Expr*, clang::Decl* exprParent,
        const clang::TemplateArgument* args, unsigned numArgs);

}}

