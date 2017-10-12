//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "OptimizerVisitor.h"

#include "SmartRewriter.h"
#include "util.h"

// #define CAIDE_DEBUG_MODE
#include "caide_debug.h"


#include <clang/AST/ASTContext.h>
#include <clang/AST/RawCommentList.h>
#include <clang/Basic/SourceManager.h>


using namespace clang;

using std::string;
using std::vector;

namespace caide {
namespace internal {


OptimizerVisitor::OptimizerVisitor(SourceManager& srcManager, const std::unordered_set<Decl*>& usedDecls,
            std::unordered_set<Decl*>& removedDecls, SmartRewriter& rewriter_)
    : sourceManager(srcManager)
    , usedDeclarations(usedDecls)
    , rewriter(rewriter_)
    , removed(removedDecls)
{}

// When we remove code, we're only interested in the real code,
// so no implicit instantiations.
bool OptimizerVisitor::shouldVisitImplicitCode() const { return false; }
bool OptimizerVisitor::shouldVisitTemplateInstantiations() const { return false; }

bool OptimizerVisitor::TraverseDecl(Decl* decl) {
#ifdef CAIDE_DEBUG_MODE
    if (decl && sourceManager.isInMainFile(decl->getLocStart())) {
        dbg("DECL " << decl->getDeclKindName() << " " << decl
            << "<" << toString(sourceManager, decl).substr(0, 30) << ">"
            << toString(sourceManager, getExpansionRange(sourceManager, decl))
            << std::endl);
    }
#endif

    bool ret = RecursiveASTVisitor<OptimizerVisitor>::TraverseDecl(decl);

    if (decl && sourceManager.isInMainFile(decl->getLocStart())) {
        // We need to visit NamespaceDecl *after* visiting it children. Tree traversal is in
        // pre-order, so processing NamespaceDecl is done here instead of in VisitNamespaceDecl.
        if (auto* nsDecl = dyn_cast<NamespaceDecl>(decl)) {
            if (nonEmptyLexicalNamespaces.find(nsDecl) == nonEmptyLexicalNamespaces.end())
                removeDecl(nsDecl);
        }

        if (auto* lexicalNamespace = dyn_cast_or_null<NamespaceDecl>(decl->getLexicalDeclContext())) {
            // Lexical context of template parameters of template type aliases is the namespace
            if (removed.find(decl) == removed.end() && !isa<TemplateTypeParmDecl>(decl))
            {
                dbg("Marking the parent namespace as non-empty" << std::endl);
                nonEmptyLexicalNamespaces.insert(lexicalNamespace);
            }
        }
    }

    return ret;
}

bool OptimizerVisitor::VisitEmptyDecl(EmptyDecl* decl) {
    if (sourceManager.isInMainFile(decl->getLocStart()))
        removeDecl(decl);
    return true;
}

bool OptimizerVisitor::VisitEnumDecl(clang::EnumDecl* enumDecl) {
    if (sourceManager.isInMainFile(enumDecl->getLocStart())
        && usedDeclarations.count(enumDecl->getCanonicalDecl()) == 0)
    {
        removeDecl(enumDecl);
    }
    return true;
}

bool OptimizerVisitor::VisitNamespaceDecl(NamespaceDecl* nsDecl) {
    if (sourceManager.isInMainFile(nsDecl->getLocStart())
        && usedDeclarations.count(nsDecl->getCanonicalDecl()) == 0)
    {
        removeDecl(nsDecl);
    }
    // The case when nsDecl is semantically used but this specific decl must be removed
    // is handled in TraverseDecl
    return true;
}

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
| | |-CXXRecordDecl (implicit)
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
    const bool funcIsUnused = usedDeclarations.count(canonicalDecl) == 0;
    const bool thisIsRedeclaration = !functionDecl->doesThisDeclarationHaveABody()
            && declared.find(canonicalDecl) != declared.end();
    const bool thisIsFriendDeclaration = functionDecl->getFriendObjectKind() != Decl::FOK_None;
    // TODO: Are we actually used by this friend?
    return funcIsUnused || (thisIsRedeclaration && !thisIsFriendDeclaration);
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

    if (needToRemoveFunction(functionDecl))
        removeDecl(templateDecl);
    return true;
}

bool OptimizerVisitor::VisitCXXRecordDecl(CXXRecordDecl* recordDecl) {
    if (!sourceManager.isInMainFile(recordDecl->getLocStart()))
        return true;
    dbg(CAIDE_FUNC);

    if (ClassTemplateDecl* classTemplate = recordDecl->getDescribedClassTemplate()) {
        if (removed.find(classTemplate) != removed.end())
            removeDecl(recordDecl);
        const TemplateSpecializationKind specKind = recordDecl->getTemplateSpecializationKind();
        if (specKind == TSK_Undeclared) {
            // This record corresponds to a non-specialized class template; it was processed as
            // ClassTemplateDecl.
            return true;
        }
    }

    CXXRecordDecl* canonicalDecl = recordDecl->getCanonicalDecl();
    const bool classIsUnused = usedDeclarations.count(canonicalDecl) == 0;
    const bool thisIsRedeclaration = !recordDecl->isCompleteDefinition()
        && declared.find(canonicalDecl) != declared.end();

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
    const bool classIsUnused = usedDeclarations.count(canonicalDecl) == 0;
    const bool thisIsRedeclaration = !templateDecl->isThisDeclarationADefinition()
        && declared.find(canonicalDecl) != declared.end();
    const bool thisIsFriendDeclaration = templateDecl->getFriendObjectKind() != Decl::FOK_None;

    // TODO: Are we actually used by this friend?
    if (classIsUnused || (thisIsRedeclaration && !thisIsFriendDeclaration))
        removeDecl(templateDecl);

    declared.insert(canonicalDecl);
    return true;
}

bool OptimizerVisitor::VisitTypedefDecl(TypedefDecl* typedefDecl) {
    if (!sourceManager.isInMainFile(typedefDecl->getLocStart()))
        return true;
    dbg(CAIDE_FUNC);

    Decl* canonicalDecl = typedefDecl->getCanonicalDecl();
    if (usedDeclarations.count(canonicalDecl) == 0)
        removeDecl(typedefDecl);

    return true;
}

bool OptimizerVisitor::VisitTypeAliasDecl(TypeAliasDecl* aliasDecl) {
    if (!sourceManager.isInMainFile(aliasDecl->getLocStart()))
        return true;
    dbg(CAIDE_FUNC);

    if (TypeAliasTemplateDecl* aliasTemplate = aliasDecl->getDescribedAliasTemplate()) {
        if (usedDeclarations.count(aliasTemplate) == 0)
            removeDecl(aliasDecl);
        // This is a template alias; will be processed as TypeAliasTemplateDecl
        return true;
    }

    Decl* canonicalDecl = aliasDecl->getCanonicalDecl();
    if (usedDeclarations.count(canonicalDecl) == 0)
        removeDecl(aliasDecl);

    return true;
}

bool OptimizerVisitor::VisitTypeAliasTemplateDecl(TypeAliasTemplateDecl* aliasTemplate) {
    if (!sourceManager.isInMainFile(aliasTemplate->getLocStart()))
        return true;
    dbg(CAIDE_FUNC);

    if (usedDeclarations.count(aliasTemplate) == 0)
        removeDecl(aliasTemplate);
    return true;
}

bool OptimizerVisitor::processUsingDirective(Decl* canonicalDecl, DeclContext* declContext) {
    bool usingIsRedundant = usedDeclarations.count(canonicalDecl) == 0;
    if (declContext) {
        DeclContext* canonicalContext = declContext->getPrimaryContext();
        bool seenInCurrentContext = !seenInUsingDirectives[canonicalContext].insert(canonicalDecl).second;
        usingIsRedundant |= seenInCurrentContext;

        while (!usingIsRedundant && (declContext = declContext->getParent())) { // semantic parent
            canonicalContext = declContext->getPrimaryContext();
            usingIsRedundant |= seenInUsingDirectives[canonicalContext].count(canonicalDecl) > 0;
        }
    }
    return usingIsRedundant;
}

// 'using namespace Ns;'
bool OptimizerVisitor::VisitUsingDirectiveDecl(UsingDirectiveDecl* usingDecl) {
    if (!sourceManager.isInMainFile(usingDecl->getLocStart()))
        return true;
    dbg(CAIDE_FUNC);

    if (NamespaceDecl* ns = usingDecl->getNominatedNamespace())
        if (processUsingDirective(ns->getCanonicalDecl(), usingDecl->getDeclContext()))
            removeDecl(usingDecl);

    return true;
}

// Variables are a special case because there may be many comma separated variables in one definition.
// We remove them separately in Finalize() method.
bool OptimizerVisitor::VisitVarDecl(VarDecl* varDecl) {
    SourceLocation start = getExpansionStart(sourceManager, varDecl);
    if (!varDecl->isLocalVarDeclOrParm() && sourceManager.isInMainFile(start)) {
        staticVariables[start].push_back(varDecl);
        /*
        Technically, we cannot remove global static variables because
        their initializers may have side effects.
        The following code marks too many expressions as having side effects
        (e.g. it will mark an std::vector constructor as such):

        VarDecl* definition = varDecl->getDefinition();
        Expr* initExpr = definition ? definition->getInit() : nullptr;
        if (initExpr && initExpr->HasSideEffects(varDecl->getASTContext()))
            srcInfo.declsToKeep.insert(varDecl);

        The analysis of which functions *really* have side effects seems too
        complicated. So currently we simply remove unreferenced global static
        variables unless they are marked with a '/// caide keep' comment.
        */
        if (usedDeclarations.count(varDecl) == 0) {
            // Mark this variable as removed, but the actual code deletion is done in
            // removeVariables() method.
            removed.insert(varDecl);
        }
    }
    return true;
}

void OptimizerVisitor::removeDecl(Decl* decl) {
    if (!decl)
        return;
    removed.insert(decl);

    SourceLocation start = getExpansionStart(sourceManager, decl);
    SourceLocation end = getExpansionEnd(sourceManager, decl);
    SourceLocation semicolonAfterDefinition = findSemiAfterLocation(end, decl->getASTContext());

    dbg("REMOVE " << decl->getDeclKindName() << " "
        << decl << ": " << toString(sourceManager, start) << " " << toString(sourceManager, end)
        << " ; " << toString(sourceManager, semicolonAfterDefinition) << std::endl);

    if (semicolonAfterDefinition.isValid())
        end = semicolonAfterDefinition;

    rewriter.removeRange(start, end);

    if (RawComment* comment = decl->getASTContext().getRawCommentForDeclNoCache(decl))
        rewriter.removeRange(comment->getSourceRange());
}

void OptimizerVisitor::Finalize(ASTContext& ctx) {
    for (const auto& kv : staticVariables) {
        SourceLocation startOfType = kv.first;
        const vector<VarDecl*>& vars = kv.second;

        const size_t n = vars.size();
        vector<bool> isUsed(n, true);
        size_t lastUsed = n;
        for (size_t i = 0; i < n; ++i) {
            isUsed[i] = usedDeclarations.count(vars[i]->getCanonicalDecl()) != 0;
            if (isUsed[i])
                lastUsed = i;
        }

        SourceLocation endOfLastVar = getExpansionEnd(sourceManager, vars.back());

        if (lastUsed == n) {
            // all variables are unused
            SourceLocation semiColon = findSemiAfterLocation(endOfLastVar, ctx);
            rewriter.removeRange(startOfType, semiColon);
        } else {
            for (size_t i = 0; i < lastUsed; ++i) if (!isUsed[i]) {
                // beginning of variable name
                SourceLocation beg = vars[i]->getLocation();

                // end of initializer
                SourceLocation end = getExpansionEnd(sourceManager, vars[i]);

                if (i+1 < n) {
                    // comma
                    end = findTokenAfterLocation(end, ctx, tok::comma);
                }

                if (beg.isValid() && end.isValid())
                    rewriter.removeRange(beg, end);
            }
            if (lastUsed + 1 != n) {
                // clear all remaining variables, starting with comma
                SourceLocation end = getExpansionEnd(sourceManager, vars[lastUsed]);
                SourceLocation comma = findTokenAfterLocation(end, ctx, tok::comma);
                rewriter.removeRange(comma, endOfLastVar);
            }
        }
    }
}


}
}

