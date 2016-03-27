//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include <clang/AST/RecursiveASTVisitor.h>

#include <stack>
#include <unordered_set>


namespace clang {
    class SourceManager;
}

namespace caide {
namespace internal {


class SmartRewriter;


class MergeNamespacesVisitor: public clang::RecursiveASTVisitor<MergeNamespacesVisitor> {
public:
    MergeNamespacesVisitor(clang::SourceManager& sourceManager,
            const std::unordered_set<clang::Decl*>& removed_, SmartRewriter& rewriter_);

    bool shouldVisitImplicitCode() const;
    bool shouldVisitTemplateInstantiations() const;

    bool TraverseDecl(clang::Decl* decl);
    bool VisitNamespaceDecl(clang::NamespaceDecl* namespaceDecl);

private:
    // The stack of non-empty lexical namespaces that were closed most recently 'in a row' (without
    // non-removed declarations between closing braces).
    std::stack<clang::NamespaceDecl*> closedNamespaces;

    clang::SourceManager& sourceManager;
    // Removed lexical declarations.
    const std::unordered_set<clang::Decl*>& removed;
    SmartRewriter& rewriter;
};

}
}

