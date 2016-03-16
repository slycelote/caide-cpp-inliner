//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "optimizer.h"
#include "DependenciesCollector.h"
#include "OptimizerVisitor.h"
#include "RemoveInactivePreprocessorBlocks.h"
#include "SmartRewriter.h"
#include "SourceInfo.h"
#include "UsedDeclarations.h"
#include "util.h"

//#define CAIDE_DEBUG_MODE
#include "caide_debug.h"


#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Sema/Sema.h>
#include <clang/Tooling/Tooling.h>


#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>


using namespace clang;
using std::set;
using std::string;
using std::vector;


namespace caide {
namespace internal {


class OptimizerConsumer: public ASTConsumer {
public:
    explicit OptimizerConsumer(CompilerInstance& compiler_, SmartRewriter& smartRewriter_,
                Rewriter& rewriter_, RemoveInactivePreprocessorBlocks& ppCallbacks_,
                string& result_)
        : compiler(compiler_)
        , sourceManager(compiler.getSourceManager())
        , smartRewriter(smartRewriter_)
        , rewriter(rewriter_)
        , ppCallbacks(ppCallbacks_)
        , result(result_)
    {}

    virtual void HandleTranslationUnit(ASTContext& Ctx) override {
        //std::cerr << "Build dependency graph" << std::endl;
        DependenciesCollector depsVisitor(sourceManager, srcInfo);
        depsVisitor.TraverseDecl(Ctx.getTranslationUnitDecl());

        // Source range of delayed-parsed template functions includes only declaration part.
        //     Force their parsing to get correct source ranges.
        //     Suppress error messages temporarily (it's OK for these functions
        //     to be malformed).
        clang::Sema& sema = compiler.getSema();
        sema.getDiagnostics().setSuppressAllDiagnostics(true);
        for (FunctionDecl* f : srcInfo.delayedParsedFunctions) {
            clang::LateParsedTemplate* lpt = sema.LateParsedTemplateMap[f];
            sema.LateTemplateParser(sema.OpaqueParser, *lpt);
        }
        sema.getDiagnostics().setSuppressAllDiagnostics(false);

        //std::cerr << "Search for used decls" << std::endl;
        UsedDeclarations usageInfo(sourceManager, rewriter);
        set<Decl*> used;
        set<Decl*> queue;
        for (Decl* decl : srcInfo.declsToKeep)
            queue.insert(isa<NamespaceDecl>(decl) ? decl : decl->getCanonicalDecl());

        while (!queue.empty()) {
            Decl* decl = *queue.begin();
            queue.erase(queue.begin());
            if (used.insert(decl).second) {
                queue.insert(srcInfo.uses[decl].begin(), srcInfo.uses[decl].end());
                usageInfo.addIfInMainFile(decl);

                if (CXXRecordDecl* record = dyn_cast<CXXRecordDecl>(decl)) {
                    // No implicit calls to destructors in AST; assume that
                    // if a class is used, its destructor is used too.
                    if (CXXDestructorDecl* destructor = record->getDestructor())
                        queue.insert(destructor);
                }
            }
        }

        used.clear();

        //std::cerr << "Remove unused decls" << std::endl;
        OptimizerVisitor visitor(sourceManager, usageInfo, smartRewriter);
        visitor.TraverseDecl(Ctx.getTranslationUnitDecl());

        removeUnusedVariables(usageInfo, Ctx);

        ppCallbacks.Finalize();

        smartRewriter.applyChanges();

        //std::cerr << "Done!" << std::endl;
        result = getResult();
    }

private:
    // Variables are a special case because there may be many
    // comma separated variables in one definition.
    void removeUnusedVariables(const UsedDeclarations& usageInfo, ASTContext& ctx) {
        Rewriter::RewriteOptions opts;
        opts.RemoveLineIfEmpty = true;

        for (const auto& kv : srcInfo.staticVariables) {
            const vector<VarDecl*>& vars = kv.second;
            const size_t n = vars.size();
            vector<bool> isUsed(n, true);
            size_t lastUsed = n;
            for (size_t i = 0; i < n; ++i) {
                VarDecl* var = vars[i];
                isUsed[i] = usageInfo.isUsed(var->getCanonicalDecl());
                if (isUsed[i])
                    lastUsed = i;
            }

            SourceLocation startOfType = kv.first;
            SourceLocation endOfLastVar = getExpansionEnd(sourceManager, vars.back());

            if (lastUsed == n) {
                // all variables are unused
                SourceLocation semiColon = findSemiAfterLocation(endOfLastVar, ctx);
                SourceRange range(startOfType, semiColon);
                smartRewriter.removeRange(range, opts);
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

                    if (beg.isValid() && end.isValid()) {
                        SourceRange range(beg, end);
                        smartRewriter.removeRange(range, opts);
                    }
                }
                if (lastUsed + 1 != n) {
                    // clear all remaining variables, starting with comma
                    SourceLocation end = getExpansionEnd(sourceManager, vars[lastUsed]);
                    SourceLocation comma = findTokenAfterLocation(end, ctx, tok::comma);
                    SourceRange range(comma, endOfLastVar);
                    smartRewriter.removeRange(range, opts);
                }
            }
        }
    }

    string getResult() const {
        // At this point the rewriter's buffer should be full with the rewritten
        // file contents.
        if (const RewriteBuffer* rewriteBuf =
                smartRewriter.getRewriteBufferFor(sourceManager.getMainFileID()))
            return string(rewriteBuf->begin(), rewriteBuf->end());

        // No changes
        bool invalid;
        const llvm::MemoryBuffer* buf = sourceManager.getBuffer(sourceManager.getMainFileID(), &invalid);
        if (buf && !invalid)
            return string(buf->getBufferStart(), buf->getBufferEnd());
        else
            return "Inliner error"; // something's wrong
    }

private:
    CompilerInstance& compiler;
    SourceManager& sourceManager;
    SmartRewriter& smartRewriter;
    Rewriter& rewriter;
    RemoveInactivePreprocessorBlocks& ppCallbacks;
    string& result;
    SourceInfo srcInfo;
};


class OptimizerFrontendAction : public ASTFrontendAction {
private:
    Rewriter& rewriter;
    SmartRewriter& smartRewriter;
    string& result;
    const set<string>& macrosToKeep;
public:
    OptimizerFrontendAction(Rewriter& rewriter_, SmartRewriter& smartRewriter_,
           string& result_, const set<string>& macrosToKeep_)
        : rewriter(rewriter_)
        , smartRewriter(smartRewriter_)
        , result(result_)
        , macrosToKeep(macrosToKeep_)
    {}

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& compiler, StringRef /*file*/) override
    {
        if (!compiler.hasSourceManager())
            throw "No source manager";
        rewriter.setSourceMgr(compiler.getSourceManager(), compiler.getLangOpts());
        auto ppCallbacks = std::unique_ptr<RemoveInactivePreprocessorBlocks>(
            new RemoveInactivePreprocessorBlocks(compiler.getSourceManager(), smartRewriter, macrosToKeep));
        auto consumer = std::unique_ptr<OptimizerConsumer>(new OptimizerConsumer(compiler, smartRewriter, rewriter, *ppCallbacks, result));
        compiler.getPreprocessor().addPPCallbacks(std::move(ppCallbacks));
        return std::move(consumer);
    }
};

class OptimizerFrontendActionFactory: public tooling::FrontendActionFactory {
private:
    Rewriter& rewriter;
    SmartRewriter smartRewriter;
    string& result;
    const set<string>& macrosToKeep;
public:
    OptimizerFrontendActionFactory(Rewriter& rewriter_, string& result_,
                const set<string>& macrosToKeep_)
        : rewriter(rewriter_)
        , smartRewriter(rewriter_)
        , result(result_)
        , macrosToKeep(macrosToKeep_)
    {}
    FrontendAction* create() {
        return new OptimizerFrontendAction(rewriter, smartRewriter, result, macrosToKeep);
    }
};


Optimizer::Optimizer(const vector<string>& cmdLineOptions_,
                     const vector<string>& macrosToKeep_)
    : cmdLineOptions(cmdLineOptions_)
    , macrosToKeep(macrosToKeep_.begin(), macrosToKeep_.end())
{}

string Optimizer::doOptimize(const string& cppFile) {
    std::unique_ptr<tooling::FixedCompilationDatabase> compilationDatabase(
        createCompilationDatabaseFromCommandLine(cmdLineOptions));

    vector<string> sources;
    sources.push_back(cppFile);

    clang::tooling::ClangTool tool(*compilationDatabase, sources);

    Rewriter rewriter;
    string result;
    OptimizerFrontendActionFactory factory(rewriter, result, macrosToKeep);

    int ret = tool.run(&factory);
    if (ret != 0)
        throw std::runtime_error("Compilation error");

    return result;
}

}
}

