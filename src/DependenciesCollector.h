//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include "clang_version.h"
#include "SourceLocationComparers.h"

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceLocation.h>

#include <iosfwd>
#include <set>
#include <stack>
#include <map>
#include <unordered_set>


namespace clang {
    class Sema;
    class SourceManager;
    class TemplateArgumentList;
}


namespace caide {
namespace internal {

struct SourceInfo;


class DependenciesCollector: public clang::RecursiveASTVisitor<DependenciesCollector> {
public:
    DependenciesCollector(clang::SourceManager& srcMgr,
        clang::Sema& sema,
        const std::unordered_set<std::string>& identifiersToKeep,
        SourceInfo& srcInfo_);

    bool shouldVisitImplicitCode() const;
    bool shouldVisitTemplateInstantiations() const;
    bool shouldWalkTypesOfTypeLocs() const;

    bool TraverseDecl(clang::Decl*);
    bool TraverseTemplateSpecializationType(clang::TemplateSpecializationType*);
    bool TraverseTemplateSpecializationTypeLoc(clang::TemplateSpecializationTypeLoc);

    bool TraverseCallExpr(clang::CallExpr*);

    bool VisitType(clang::Type*);
    bool VisitTypedefType(clang::TypedefType*);
    bool VisitTemplateSpecializationType(clang::TemplateSpecializationType*);
#if CAIDE_CLANG_VERSION_AT_LEAST(10,0)
    bool VisitAutoType(clang::AutoType*);
#endif

    bool VisitStmt(clang::Stmt* stmt);

    bool VisitDecl(clang::Decl* decl);
    bool VisitNamedDecl(clang::NamedDecl* namedDecl);
    bool VisitCallExpr(clang::CallExpr* callExpr);
    bool VisitCXXConstructExpr(clang::CXXConstructExpr* constructorExpr);
    bool VisitCXXConstructorDecl(clang::CXXConstructorDecl* ctorDecl);
    bool VisitTemplateTypeParmDecl(clang::TemplateTypeParmDecl* paramDecl);
    bool VisitDeclRefExpr(clang::DeclRefExpr* ref);
    bool VisitValueDecl(clang::ValueDecl* valueDecl);
    bool VisitVarDecl(clang::VarDecl*);
    bool VisitMemberExpr(clang::MemberExpr* memberExpr);
    bool VisitLambdaExpr(clang::LambdaExpr* lambdaExpr);
    bool VisitFieldDecl(clang::FieldDecl* field);
    bool VisitTypeAliasDecl(clang::TypeAliasDecl* aliasDecl);
    bool VisitTypeAliasTemplateDecl(clang::TypeAliasTemplateDecl* aliasTemplateDecl);
    bool VisitClassTemplateDecl(clang::ClassTemplateDecl* templateDecl);
    bool VisitClassTemplateSpecializationDecl(clang::ClassTemplateSpecializationDecl* specDecl);
    bool VisitVarTemplateSpecializationDecl(clang::VarTemplateSpecializationDecl* specDecl);
    bool TraverseClassTemplateSpecializationDecl(clang::ClassTemplateSpecializationDecl*);
    bool VisitFunctionDecl(clang::FunctionDecl* f);
    bool VisitFunctionTemplateDecl(clang::FunctionTemplateDecl* functionTemplate);
    bool VisitCXXMethodDecl(clang::CXXMethodDecl* method);
    bool VisitCXXRecordDecl(clang::CXXRecordDecl* recordDecl);
    bool VisitUsingShadowDecl(clang::UsingShadowDecl* usingDecl);
    bool VisitEnumDecl(clang::EnumDecl* enumDecl);
#if CAIDE_CLANG_VERSION_AT_LEAST(10,0)
    bool VisitConceptSpecializationExpr(clang::ConceptSpecializationExpr* conceptExpr);
#endif

    void printGraph(std::ostream& out) const;

private:
    clang::Decl* getCurrentDecl() const;
    clang::FunctionDecl* getCurrentFunction(clang::Decl* decl) const;
    clang::Decl* getParentDecl(clang::Decl* decl) const;

    clang::Decl* getCorrespondingDeclInNonInstantiatedContext(clang::Decl* semanticDecl) const;

    // Should be in RecursiveASTVisitor.
    void traverseTemplateArgumentsHelper(llvm::ArrayRef<clang::TemplateArgumentLoc> args);

    void insertReference(clang::Decl* from, clang::Decl* to);


    clang::SourceManager& sourceManager;
    clang::Sema& sema;
    const std::unordered_set<std::string>& identifiersToKeep;
    SourceInfo& srcInfo;

    // There is no getParentDecl(stmt) function, so we maintain the stack of Decls,
    // with inner-most active Decl at the top of the stack.
    // \sa TraverseDecl().
    std::stack<clang::Decl*> declStack;
};

}
}

