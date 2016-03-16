//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.


#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceLocation.h>

#include <set>
#include <string>

namespace clang {
    class SourceManager;
}

namespace caide {
namespace internal {

class SmartRewriter;
class UsedDeclarations;


class OptimizerVisitor: public clang::RecursiveASTVisitor<OptimizerVisitor> {
public:
    OptimizerVisitor(clang::SourceManager& srcManager, const UsedDeclarations& usageInfo_,
            SmartRewriter& rewriter_);

    bool shouldVisitImplicitCode() const;
    bool shouldVisitTemplateInstantiations() const;

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

private:
    bool needToRemoveFunction(clang::FunctionDecl* functionDecl) const;
    void removeDecl(clang::Decl* decl);

    std::string toString(const clang::Decl* decl) const;
    std::string toString(clang::SourceLocation loc) const;


    clang::SourceManager& sourceManager;
    const UsedDeclarations& usageInfo;
    std::set<clang::Decl*> declared;
    std::set<clang::NamespaceDecl*> usedNamespaces;
    SmartRewriter& rewriter;
};

}
}

