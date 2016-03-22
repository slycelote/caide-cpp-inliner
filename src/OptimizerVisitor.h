//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceLocation.h>

#include <map>
#include <string>
#include <unordered_set>
#include <vector>


namespace clang {
    class SourceManager;
    class ASTContext;
}

namespace caide {
namespace internal {


class SmartRewriter;
class UsedDeclarations;


class OptimizerVisitor: public clang::RecursiveASTVisitor<OptimizerVisitor> {
public:
    OptimizerVisitor(clang::SourceManager& srcManager, const UsedDeclarations& usedDecls,
            SmartRewriter& rewriter_);

    bool shouldVisitImplicitCode() const;
    bool shouldVisitTemplateInstantiations() const;

    bool TraverseDecl(clang::Decl* decl);

    bool VisitEmptyDecl(clang::EmptyDecl* decl);
    bool VisitNamespaceDecl(clang::NamespaceDecl* namespaceDecl);
    bool VisitFunctionDecl(clang::FunctionDecl* functionDecl);
    bool VisitFunctionTemplateDecl(clang::FunctionTemplateDecl* templateDecl);
    bool VisitCXXRecordDecl(clang::CXXRecordDecl* recordDecl);
    bool VisitClassTemplateDecl(clang::ClassTemplateDecl* templateDecl);
    bool VisitTypedefDecl(clang::TypedefDecl* typedefDecl);
    bool VisitTypeAliasDecl(clang::TypeAliasDecl* aliasDecl);
    bool VisitTypeAliasTemplateDecl(clang::TypeAliasTemplateDecl* aliasDecl);
    bool VisitUsingDirectiveDecl(clang::UsingDirectiveDecl* usingDecl);
    bool VisitVarDecl(clang::VarDecl* varDecl);

    // Apply changes that require some 'global' knowledge.
    // Called after traversal of the whole AST.
    void Finalize(clang::ASTContext& ctx);

private:
    bool needToRemoveFunction(clang::FunctionDecl* functionDecl) const;
    void removeDecl(clang::Decl* decl);

    clang::SourceManager& sourceManager;
    const UsedDeclarations& usedDeclarations;
    SmartRewriter& rewriter;

    std::unordered_set<clang::Decl*> declared;
    std::unordered_set<clang::Decl*> removed;

    // Parent namespaces of non-removed Decls
    std::unordered_set<clang::NamespaceDecl*> nonEmptyLexicalNamespaces;

    // namespaces for which 'using namespace' declaration has been issued.
    // FIXME: this should depend on current scope
    std::unordered_set<clang::NamespaceDecl*> usedNamespaces;

    // Declarations of static variables, grouped by their start location
    // (so comma separated declarations go into the same group).
    std::map<clang::SourceLocation, std::vector<clang::VarDecl*>> staticVariables;
};


}
}

