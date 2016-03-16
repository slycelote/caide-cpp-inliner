//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "OptimizerVisitor.h"

#include "SmartRewriter.h"
#include "UsedDeclarations.h"
#include "util.h"

//#define CAIDE_DEBUG_MODE
#include "caide_debug.h"


#include <clang/AST/ASTContext.h>
#include <clang/AST/RawCommentList.h>
#include <clang/Basic/SourceManager.h>


using namespace clang;

using std::string;

namespace caide {
namespace internal {


OptimizerVisitor::OptimizerVisitor(SourceManager& srcManager, const UsedDeclarations& usageInfo_,
                                   SmartRewriter& rewriter_)
    : sourceManager(srcManager)
    , usageInfo(usageInfo_)
    , rewriter(rewriter_)
{}

string OptimizerVisitor::toString(const Decl* decl) const {
    return internal::toString(sourceManager, decl);
}

// When we remove code, we're only interested in the real code,
// so no implicit instantiantions.
bool OptimizerVisitor::shouldVisitImplicitCode() const { return false; }
bool OptimizerVisitor::shouldVisitTemplateInstantiations() const { return false; }

bool OptimizerVisitor::VisitEmptyDecl(EmptyDecl* decl) {
    if (sourceManager.isInMainFile(decl->getLocStart()))
        removeDecl(decl);
    return true;
}

bool OptimizerVisitor::VisitNamespaceDecl(NamespaceDecl* namespaceDecl) {
    if (sourceManager.isInMainFile(namespaceDecl->getLocStart()) && !usageInfo.isUsed(namespaceDecl))
        removeDecl(namespaceDecl);
    return true;
}

/*
bool OptimizerVisitor::VisitStmt(Stmt* stmt) {
    std::cerr << stmt->getStmtClassName() << endl;
    return true;
}
*/

/*
 Here's how template functions and classes are represented in the AST.
-FunctionTemplateDecl <-- the template
|-TemplateTypeParmDecl
|-FunctionDecl  <-- general (non-specialized) case
|-FunctionDecl  <-- for each implicit instantiation of the template
| `-CompoundStmt
|   `-...
-FunctionDecl   <-- non-template or full explicit specialization of a template



|-ClassTemplateDecl <-- root template
| |-TemplateTypeParmDecl
| |-CXXRecordDecl  <-- non-specialized root template class
| | |-CXXRecordDecl
| | `-CXXMethodDecl...
| |-ClassTemplateSpecialization <-- non-instantiated explicit specialization (?)
| `-ClassTemplateSpecializationDecl <-- implicit instantiation of root template
|   |-TemplateArgument type 'double'
|   |-CXXRecordDecl
|   |-CXXMethodDecl...
|-ClassTemplatePartialSpecializationDecl <-- partial specialization
| |-TemplateArgument
| |-TemplateTypeParmDecl
| |-CXXRecordDecl
| `-CXXMethodDecl...
|-ClassTemplateSpecializationDecl <-- instantiation of explicit specialization
| |-TemplateArgument type 'int'
| |-CXXRecordDecl
| `-CXXMethodDecl...

 */

bool OptimizerVisitor::needToRemoveFunction(FunctionDecl* functionDecl) const {
    if (functionDecl->isExplicitlyDefaulted() || functionDecl->isDeleted())
        return false;
    FunctionDecl* canonicalDecl = functionDecl->getCanonicalDecl();
    const bool funcIsUnused = !usageInfo.isUsed(canonicalDecl);
    const bool thisIsRedeclaration = !functionDecl->doesThisDeclarationHaveABody()
            && declared.find(canonicalDecl) != declared.end();
    return funcIsUnused || thisIsRedeclaration;
}

bool OptimizerVisitor::VisitFunctionDecl(FunctionDecl* functionDecl) {
    if (!sourceManager.isInMainFile(functionDecl->getLocStart()))
        return true;
    dbg(CAIDE_FUNC);

    // It may have been processed as FunctionTemplateDecl already
    // but we try it anyway.
    if (needToRemoveFunction(functionDecl))
        removeDecl(functionDecl);

    declared.insert(functionDecl->getCanonicalDecl());
    return true;
}

// TODO: dependencies on types of template parameters
bool OptimizerVisitor::VisitFunctionTemplateDecl(FunctionTemplateDecl* templateDecl) {
    if (!sourceManager.isInMainFile(templateDecl->getLocStart()))
        return true;
    dbg(CAIDE_FUNC);
    FunctionDecl* functionDecl = templateDecl->getTemplatedDecl();

    // Correct source range may be given by either this template decl
    // or corresponding CXXMethodDecl, in case of template method of
    // template class. Choose the one that starts earlier.
    const bool processAsCXXMethod = sourceManager.isBeforeInTranslationUnit(
            getExpansionStart(sourceManager, functionDecl),
            getExpansionStart(sourceManager, templateDecl)
    );

    if (processAsCXXMethod) {
        // Will be processed as FunctionDecl later.
        return true;
    }

    if (needToRemoveFunction(functionDecl))
        removeDecl(templateDecl);
    return true;
}

bool OptimizerVisitor::VisitCXXRecordDecl(CXXRecordDecl* recordDecl) {
    if (!sourceManager.isInMainFile(recordDecl->getLocStart()))
        return true;
    bool isTemplated = recordDecl->getDescribedClassTemplate() != 0;
    TemplateSpecializationKind specKind = recordDecl->getTemplateSpecializationKind();
    if (isTemplated && (specKind == TSK_ImplicitInstantiation || specKind == TSK_Undeclared))
        return true;
    CXXRecordDecl* canonicalDecl = recordDecl->getCanonicalDecl();
    const bool classIsUnused = !usageInfo.isUsed(canonicalDecl);
    const bool thisIsRedeclaration = !recordDecl->isCompleteDefinition() && declared.find(canonicalDecl) != declared.end();

    if (classIsUnused || thisIsRedeclaration)
        removeDecl(recordDecl);

    declared.insert(canonicalDecl);
    return true;
}

bool OptimizerVisitor::VisitClassTemplateDecl(ClassTemplateDecl* templateDecl) {
    if (!sourceManager.isInMainFile(templateDecl->getLocStart()))
        return true;
    dbg(CAIDE_FUNC);
    ClassTemplateDecl* canonicalDecl = templateDecl->getCanonicalDecl();
    const bool classIsUnused = !usageInfo.isUsed(canonicalDecl);
    const bool thisIsRedeclaration = !templateDecl->isThisDeclarationADefinition() && declared.find(canonicalDecl) != declared.end();

    if (classIsUnused || thisIsRedeclaration)
        removeDecl(templateDecl);

    declared.insert(canonicalDecl);
    return true;
}

bool OptimizerVisitor::VisitTypedefDecl(TypedefDecl* typedefDecl) {
    if (!sourceManager.isInMainFile(typedefDecl->getLocStart()))
        return true;

    Decl* canonicalDecl = typedefDecl->getCanonicalDecl();
    if (!usageInfo.isUsed(canonicalDecl))
        removeDecl(typedefDecl);

    return true;
}

bool OptimizerVisitor::VisitTypeAliasDecl(TypeAliasDecl* aliasDecl) {
    if (!sourceManager.isInMainFile(aliasDecl->getLocStart()))
        return true;
    if (aliasDecl->getDescribedAliasTemplate()) {
        // This is a template alias; will be processed as TypeAliasTemplateDecl
        return true;
    }

    Decl* canonicalDecl = aliasDecl->getCanonicalDecl();
    if (!usageInfo.isUsed(canonicalDecl))
        removeDecl(aliasDecl);

    return true;
}

bool OptimizerVisitor::VisitTypeAliasTemplateDecl(TypeAliasTemplateDecl* aliasDecl) {
    if (!sourceManager.isInMainFile(aliasDecl->getLocStart()))
        return true;
    if (!usageInfo.isUsed(aliasDecl))
        removeDecl(aliasDecl);
    return true;
}

bool OptimizerVisitor::VisitUsingDirectiveDecl(UsingDirectiveDecl* usingDecl) {
    if (!sourceManager.isInMainFile(usingDecl->getLocStart()))
        return true;
    NamespaceDecl* ns = usingDecl->getNominatedNamespace();
    if (ns && !usedNamespaces.insert(ns).second)
        removeDecl(usingDecl);
    return true;
}

void OptimizerVisitor::removeDecl(Decl* decl) {
    if (!decl)
        return;
    SourceLocation start = getExpansionStart(sourceManager, decl);
    SourceLocation end = getExpansionEnd(sourceManager, decl);

    SourceLocation semicolonAfterDefinition = findSemiAfterLocation(end, decl->getASTContext());
    dbg("REMOVE " << decl->getDeclKindName() << " "
        << decl << ": " << toString(start) << " " << toString(end)
        << " ; " << toString(semicolonAfterDefinition) << std::endl);
    if (semicolonAfterDefinition.isValid())
        end = semicolonAfterDefinition;
    Rewriter::RewriteOptions opts;
    opts.RemoveLineIfEmpty = true;
    rewriter.removeRange(SourceRange(start, end), opts);

    RawComment* comment = decl->getASTContext().getRawCommentForDeclNoCache(decl);
    if (comment)
        rewriter.removeRange(comment->getSourceRange(), opts);
}

string OptimizerVisitor::toString(SourceLocation loc) const {
    return loc.printToString(sourceManager);
}

}
}

