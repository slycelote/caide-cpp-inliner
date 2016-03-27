//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceLocation.h>

#include <map>
#include <string>
#include <unordered_map>
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
            std::unordered_set<clang::Decl*>& removedDecls, SmartRewriter& rewriter_);

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

    // Process 'using namespace ns;' or 'using ns::identifier;' declaration
    // canonicalDecl is the namespace or the identifier that is the target of the using declaration
    // declContext is the declaration context where the using declaration is issued.
    // Return value is true when this using declaration is redundant.
    bool processUsingDirective(clang::Decl* canonicalDecl, clang::DeclContext* declContext);


    clang::SourceManager& sourceManager;
    const UsedDeclarations& usedDeclarations;
    SmartRewriter& rewriter;

    std::unordered_set<clang::Decl*> declared;
    std::unordered_set<clang::Decl*>& removed;

    // Parent namespaces of non-removed Decls
    std::unordered_set<clang::NamespaceDecl*> nonEmptyLexicalNamespaces;

    // For each semantic declaraction context, keep track of which namespaces have been seen in
    // 'using namespace ns;' directives in this declaraction context (TODO: and which identifiers
    // have been seen in 'using ns::identifier' declaractions).
    std::unordered_map<clang::DeclContext*, std::unordered_set<clang::Decl*>> seenInUsingDirectives;

    // Declarations of static variables, grouped by their start location
    // (so comma separated declarations go into the same group).
    std::map<clang::SourceLocation, std::vector<clang::VarDecl*>> staticVariables;
};


}
}

