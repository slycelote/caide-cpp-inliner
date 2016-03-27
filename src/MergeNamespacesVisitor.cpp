//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "MergeNamespacesVisitor.h"
#include "SmartRewriter.h"
#include "util.h"

#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>

using namespace clang;


namespace caide {
namespace internal {


MergeNamespacesVisitor::MergeNamespacesVisitor(SourceManager& sourceManager_,
        const std::unordered_set<Decl*>& removed_, SmartRewriter& rewriter_)
    : sourceManager(sourceManager_)
    , removed(removed_)
    , rewriter(rewriter_)
{}

bool MergeNamespacesVisitor::shouldVisitImplicitCode() const { return false; }
bool MergeNamespacesVisitor::shouldVisitTemplateInstantiations() const { return false; }


bool MergeNamespacesVisitor::TraverseDecl(Decl* decl) {
    bool ret = RecursiveASTVisitor<MergeNamespacesVisitor>::TraverseDecl(decl);

    // Check that decl was not removed. We don't mark all declarations as removed, so check
    // the lexical context of the decl too.
    // Note: a type alias is not a declaration context, so the lexical context of its template
    // arguments is the enclosing namespace/class/function etc. It means that this check may give a
    // false positive. So we skip TemplateTypeParmDecl (it's always attached to another Decl anyway).
    if (decl && sourceManager.isInMainFile(decl->getLocStart()) && removed.count(decl) == 0
        && removed.count(dyn_cast_or_null<Decl>(decl->getLexicalDeclContext())) == 0)
    {
        if (auto* nsDecl = dyn_cast<NamespaceDecl>(decl))
            closedNamespaces.push(nsDecl);
        else if (!isa<TemplateTypeParmDecl>(decl))
            closedNamespaces = std::stack<NamespaceDecl*>{};
    }

    return ret;
}

bool MergeNamespacesVisitor::VisitNamespaceDecl(NamespaceDecl* nsDecl) {
    if (!sourceManager.isInMainFile(nsDecl->getLocStart()))
        return true;

    if (nsDecl && removed.count(nsDecl) == 0) {
        NamespaceDecl* canonicalDecl = nsDecl->getCanonicalDecl();
        if (!closedNamespaces.empty() && canonicalDecl == closedNamespaces.top()->getCanonicalDecl()) {
            // Merge with previous namespace.
            SourceLocation closingBraceLoc = closedNamespaces.top()->getRBraceLoc();
            rewriter.removeRange(closingBraceLoc, closingBraceLoc);
            closedNamespaces.pop();

            ASTContext& astContext = nsDecl->getASTContext();
            SourceLocation thisNamespaceNameStart =
                findTokenAfterLocation(nsDecl->getLocStart(), astContext, tok::raw_identifier);
            SourceLocation thisNamespaceOpeningBrace =
                findTokenAfterLocation(thisNamespaceNameStart, astContext, tok::l_brace);

            rewriter.removeRange(nsDecl->getLocStart(), thisNamespaceOpeningBrace);
        }
    }

    return true;
}


}
}

