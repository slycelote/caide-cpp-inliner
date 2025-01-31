//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "sema_utils.h"

// #define CAIDE_DEBUG_MODE
#include "caide_debug.h"
#include "clang_version.h"
#include "util.h"

#include <clang/Sema/Sema.h>
#include <clang/Sema/Template.h>
#include <clang/Sema/TemplateDeduction.h>

using namespace clang;

namespace caide { namespace internal {

SuppressErrorsInScope::SuppressErrorsInScope(Sema& sema_):
    sema(sema_)
{
    DiagnosticsEngine& diag = sema.getDiagnostics();
    origSuppressAllDiagnostics = diag.getSuppressAllDiagnostics();
    diag.setSuppressAllDiagnostics(true);
}

SuppressErrorsInScope::~SuppressErrorsInScope() {
    sema.getDiagnostics().setSuppressAllDiagnostics(origSuppressAllDiagnostics);
}

namespace {

// Calculates number of template levels inside which decl is wrapped.
// If decl itself is a template, it is not counted.
unsigned getNumTemplateLevels(const Decl* decl) {
    unsigned res = 0;
    for (const DeclContext* ctx = decl->getDeclContext(); ctx; ctx = ctx->getParent()) {
        if (isa<ClassTemplatePartialSpecializationDecl>(ctx)) {
            ++res;
        } else if (const auto* record = dyn_cast<CXXRecordDecl>(ctx)) {
            if (record->getDescribedClassTemplate()) {
                ++res;
            }
        } else if (const auto* func = dyn_cast<FunctionDecl>(ctx)) {
            if (func->getDescribedFunctionTemplate()) {
                ++res;
            }
        }
    }

    return res;
}

}

SugaredSignature getSugaredSignature(Sema& sema, CallExpr* callExpr) {
    SugaredSignature ret;

#if CAIDE_CLANG_VERSION_AT_LEAST(16, 0)
    Expr* callee = callExpr->getCallee();
    Decl* calleeDecl = callExpr->getCalleeDecl();

    if (!callee || !calleeDecl)
        return ret;

    // Specialization called by this expr. Refers to unsugared types.
    auto* fspec = dyn_cast<FunctionDecl>(calleeDecl);
    if (!fspec) {
        return ret;
    }

    auto* refExpr = dyn_cast<DeclRefExpr>(callee);
    if (!refExpr) {
        auto* castExpr = dyn_cast<ImplicitCastExpr>(callee);
        if (castExpr && castExpr->getCastKind() == CK_FunctionToPointerDecay) {
            refExpr = dyn_cast<DeclRefExpr>(castExpr->getSubExpr());
        }
        if (!refExpr) {
            return ret;
        }
    }

    FunctionTemplateSpecializationInfo* specInfo = fspec->getTemplateSpecializationInfo();
    if (!specInfo) {
        return ret;
    }

    FunctionTemplateDecl* ftemplate = specInfo->getTemplate();
    FunctionDecl* func = ftemplate->getTemplatedDecl(); // Non-specialized decl.
    TemplateParameterList* templateParams = ftemplate->getTemplateParameters();
    llvm::ArrayRef<TemplateArgumentLoc> templateArgs = refExpr->template_arguments();
    const unsigned numExplicitArguments = templateArgs.size();

    SuppressErrorsInScope guard(sema);

    llvm::ArrayRef<TemplateArgument> sugaredTemplateArgs;

    llvm::SmallVector<TemplateArgument, 8> fallbackBuffer;
    auto useFallback = [&] {
        fallbackBuffer.reserve(templateParams->size());
        for (unsigned i = 0; i < numExplicitArguments; ++i) {
            fallbackBuffer.push_back(templateArgs[i].getArgument());
        }
        for (unsigned i = numExplicitArguments; i < templateParams->size(); ++i) {
            // Falling back to unsugared arguments.
            fallbackBuffer.push_back(specInfo->TemplateArguments->get(i));
        }

        sugaredTemplateArgs = fallbackBuffer;
    };

    clang::sema::TemplateDeductionInfo deductionInfo(calleeDecl->getBeginLoc());
    if (numExplicitArguments == templateParams->size()) {
        // All template arguments are explicitly provided, no need for deduction.
        useFallback();
    } else {
        // Perform template argument deduction for the function call.
        TemplateArgumentListInfo explicitArgs;
        for (const TemplateArgumentLoc& argLoc : templateArgs) {
            explicitArgs.addArgument(argLoc);
        }
        FunctionDecl* specialization;
        // Actual result type has different spelling in different clang versions.
        auto res = static_cast<int>(sema.DeduceTemplateArguments(
                ftemplate,
                &explicitArgs,
                llvm::ArrayRef<Expr*>{callExpr->getArgs(), callExpr->getNumArgs()},
                specialization,
                deductionInfo,
                /*PartialOverloading=*/false,
#if CAIDE_CLANG_VERSION_AT_LEAST(17, 0)
                /*AggregateDeductionCandidate=*/false,
#endif
#if CAIDE_CLANG_VERSION_AT_LEAST(18, 0)
                // TODO: What are these?
                /*ObjectType=*/QualType(),
                /*ObjectClassification=*/Expr::Classification(),
#endif
                /*CheckNonDependent=*/[](llvm::ArrayRef<QualType>){return false;}));
        if (res == 0) {
            TemplateArgumentList* argList = deductionInfo.takeSugared();
            sugaredTemplateArgs = argList->asArray();
        } else {
            dbg("sema.DeduceTemplateArguments failed: " << res << std::endl);
            useFallback();
        }
    }

    ret.templateArgs.assign(sugaredTemplateArgs.begin(), sugaredTemplateArgs.end());

    MultiLevelTemplateArgumentList sugaredMLTAL;
    sugaredMLTAL.addOuterTemplateArguments(ftemplate, sugaredTemplateArgs, true);
    // TODO: are we handling nested templates properly?
    sugaredMLTAL.addOuterRetainedLevels(getNumTemplateLevels(ftemplate));

    llvm::SmallVector<TemplateArgument, 4> argsToDeduce;
    argsToDeduce.resize(func->parameters().size());
    // Push the instantiation on context stack till the end of scope.
    // This is only to avoid clang assertions in debug mode.
    // (sema.DeduceTemplateArguments above pushes and pops this internally.)
    Sema::InstantiatingTemplate substitutionContext(
            sema, deductionInfo.getLocation(), ftemplate, argsToDeduce,
            Sema::CodeSynthesisContext::DeducedTemplateArgumentSubstitution,
            deductionInfo);

    // Substitute sugared template arguments into parameter types.
    for (ParmVarDecl* param : func->parameters()) {
        TypeSourceInfo* paramTSI = param->getTypeSourceInfo();
        TypeSourceInfo* substType = sema.SubstType(
            paramTSI,
            sugaredMLTAL,
            param->getLocation(),
            {});
        if (substType) {
            // substType->getType().dump();
            ret.argTypes.push_back(substType);
        } else {
            dbg("sema.SubstType failed" << std::endl);
        }
    }

    // Substitute sugared template arguments into constraints.
    llvm::SmallVector<const Expr*, 4> associatedConstraints;
    ftemplate->getAssociatedConstraints(associatedConstraints);
    for (const Expr* expr : associatedConstraints) {
        clang::ExprResult result = sema.SubstExpr(const_cast<Expr*>(expr), sugaredMLTAL);
        if (result.get()) {
            dbg("Specialized constraint expr: " << toString(ftemplate->getASTContext(), *expr) << std::endl);
            ret.associatedConstraints.push_back(result.get());
        } else if (result.isInvalid()) {
            dbg("sema.SubstExpr failed" << std::endl);
        }
    }
#endif

    return ret;
}

std::vector<TemplateArgumentLoc> substituteDefaultTemplateArguments(
        Sema& sema, TemplateDecl* templateDecl,
        const TemplateArgument* args, unsigned numArgs) {
    std::vector<TemplateArgumentLoc> res;

#if CAIDE_CLANG_VERSION_AT_LEAST(16, 0)
    MultiLevelTemplateArgumentList sugaredMLTAL;
    sugaredMLTAL.addOuterTemplateArguments(templateDecl,
            llvm::ArrayRef<TemplateArgument>(args, numArgs),
            true);
    // TODO: are we handling nested templates properly?
    sugaredMLTAL.addOuterRetainedLevels(getNumTemplateLevels(templateDecl));
    TemplateParameterList* templateParams = templateDecl->getTemplateParameters();
    if (!templateParams) {
        return res;
    }

    llvm::SmallVector<TemplateArgumentLoc, 8> defaultTemplateArgs;

    // Skip explicitly provided arguments.
    for (unsigned i = numArgs; i < templateParams->size(); ++i) {
        NamedDecl* param = templateParams->getParam(i);
        TemplateArgumentLoc defaultArg;
        bool foundDefaultArgument = false;
        // See TemplateParameterList ctor in DeclTemplate.cpp for possible types.
        if (const auto* NTTP = dyn_cast<NonTypeTemplateParmDecl>(param)) {
            if (NTTP->hasDefaultArgument()) {
                foundDefaultArgument = true;
#if CAIDE_CLANG_VERSION_AT_LEAST(19,0)
                defaultArg = NTTP->getDefaultArgument();
#else
                Expr* expr = NTTP->getDefaultArgument();
                defaultArg = TemplateArgumentLoc(TemplateArgument(expr), expr);
#endif
            }
        } else if (const auto* typeParam = dyn_cast<TemplateTypeParmDecl>(param)) {
            if (typeParam->hasDefaultArgument()) {
                foundDefaultArgument = true;
#if CAIDE_CLANG_VERSION_AT_LEAST(19,0)
                defaultArg = typeParam->getDefaultArgument();
#else
                QualType type = typeParam->getDefaultArgument();
                TypeSourceInfo* TSI = typeParam->getDefaultArgumentInfo();
                defaultArg = TemplateArgumentLoc(TemplateArgument(type), TSI);
#endif
            }
        } else if (const auto* TTP = dyn_cast<TemplateTemplateParmDecl>(param)) {
            if (TTP->hasDefaultArgument()) {
                foundDefaultArgument = true;
                defaultArg = TTP->getDefaultArgument();
            }
        }
        if (foundDefaultArgument) {
            defaultTemplateArgs.push_back(defaultArg);
        }
    }

    if (!defaultTemplateArgs.empty()) {
        SuppressErrorsInScope guard(sema);
        TemplateArgumentListInfo substitutedDefaultArgs;
        bool error = sema.SubstTemplateArguments(defaultTemplateArgs,
                sugaredMLTAL, substitutedDefaultArgs);
        if (error) {
            dbg("sema.SubstTemplateArguments failed" << std::endl);
        } else {
            for (unsigned i = 0; i < substitutedDefaultArgs.size(); ++i)
                res.push_back(substitutedDefaultArgs[i]);
        }
    }
#endif

    return res;
}

std::vector<TemplateArgumentLoc> substituteTemplateArguments(
        Sema& sema, ClassTemplatePartialSpecializationDecl* templateDecl,
        const TemplateArgument* args, unsigned numArgs) {
    std::vector<TemplateArgumentLoc> res;

#if CAIDE_CLANG_VERSION_AT_LEAST(16, 0)
    MultiLevelTemplateArgumentList sugaredMLTAL;
    sugaredMLTAL.addOuterTemplateArguments(templateDecl,
            llvm::ArrayRef<TemplateArgument>(args, numArgs),
            true);
    // TODO: are we handling nested templates properly?
    sugaredMLTAL.addOuterRetainedLevels(getNumTemplateLevels(templateDecl));

    TemplateArgumentListInfo substitutedTemplateArgs;
    const ASTTemplateArgumentListInfo* templateArgsAsWritten = templateDecl->getTemplateArgsAsWritten();
    if (!templateArgsAsWritten) {
        return res;
    }

    SuppressErrorsInScope guard(sema);
    bool error = sema.SubstTemplateArguments(templateArgsAsWritten->arguments(),
            sugaredMLTAL, substitutedTemplateArgs);
    if (error) {
        dbg("sema.SubstTemplateArguments failed" << std::endl);
    } else {
        for (unsigned i = 0; i < substitutedTemplateArgs.size(); ++i)
            res.push_back(substitutedTemplateArgs[i]);
    }
#endif

    return res;
}

Expr* substituteTemplateArguments(
        clang::Sema& sema, clang::Expr* expr, clang::Decl* exprParent,
        const clang::TemplateArgument* args, unsigned numArgs) {
#if CAIDE_CLANG_VERSION_AT_LEAST(16, 0)
    MultiLevelTemplateArgumentList templateArgs;
    templateArgs.addOuterTemplateArguments(exprParent,
            llvm::ArrayRef<TemplateArgument>(args, numArgs),
            true);
    // TODO: are we handling nested templates properly?
    templateArgs.addOuterRetainedLevels(getNumTemplateLevels(exprParent));

    SuppressErrorsInScope guard(sema);
    clang::ExprResult result = sema.SubstExpr(expr, templateArgs);
    if (result.isInvalid()) {
        dbg("sema.SubstExpr failed" << std::endl);
        return nullptr;
    }
    return result.get();
#else
    return nullptr;
#endif
}

}}

