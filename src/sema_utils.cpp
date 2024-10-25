//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "sema_utils.h"

// #define CAIDE_DEBUG_MODE
#include "caide_debug.h"
#include "clang_version.h"

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

TypesInSignature getSugaredTypesInSignature(Sema& sema, CallExpr* callExpr) {
    TypesInSignature ret;

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
        if (auto* castExpr = dyn_cast<ImplicitCastExpr>(callee);
            castExpr && castExpr->getCastKind() == CK_FunctionToPointerDecay) {
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
    TemplateParameterList* templateParams = ftemplate->getTemplateParameters();
    llvm::ArrayRef<TemplateArgumentLoc> templateArgs = refExpr->template_arguments();
    const unsigned numExplicitArguments = templateArgs.size();

    SuppressErrorsInScope guard(sema);

    llvm::ArrayRef<TemplateArgument> sugaredTemplateArgs;

    llvm::SmallVector<TemplateArgument> fallbackBuffer;
    auto useFallback = [&] {
        fallbackBuffer.reserve(templateParams->size());
        for (unsigned i = 0; i < numExplicitArguments; ++i) {
            fallbackBuffer.push_back(templateArgs[i].getArgument());
        }
        for (unsigned i = numExplicitArguments; i < templateParams->size(); ++i) {
            // Unsugared.
            fallbackBuffer.push_back(specInfo->TemplateArguments->get(i));
        }

        sugaredTemplateArgs = fallbackBuffer;
    };

    if (numExplicitArguments == templateParams->size()) {
        // All template arguments are explicitly provided, no need for deduction.
        useFallback();
    } else {
        // Perform template argument deduction for the function call.
        TemplateArgumentListInfo explicitArgs;
        for (const TemplateArgumentLoc& argLoc : templateArgs) {
            explicitArgs.addArgument(argLoc);
        }
        clang::sema::TemplateDeductionInfo deductionInfo(calleeDecl->getBeginLoc());
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
                [](llvm::ArrayRef<QualType>){return false;}));
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
    // TODO: handle nested templates.
    sugaredMLTAL.addOuterTemplateArguments(ftemplate, sugaredTemplateArgs, true);

    // Now substitute sugared template arguments into parameter types.
    FunctionDecl* func = ftemplate->getTemplatedDecl(); // Non-specialized decl.
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

    return ret;
}

}}

