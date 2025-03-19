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

#include <llvm/ADT/ArrayRef.h>

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

bool anyArgDependent(llvm::ArrayRef<TemplateArgument> args) {
    for (const auto& arg : args) {
        if (arg.isDependent()) {
            return true;
        }
    }
    return false;
}

void substituteExprs(Sema& sema, llvm::ArrayRef<const Expr*> exprs,
        const MultiLevelTemplateArgumentList& MLTAL, SugaredSignature& sig)
{
    for (auto* expr : exprs) {
        clang::ExprResult result = sema.SubstExpr(const_cast<Expr*>(expr), MLTAL);
        if (result.isInvalid()) {
            dbg("sema.SubstExpr failed" << std::endl);
        } else if (Expr* res = result.get()) {
            dbg("Specialized constraint expr: " << toString(sema.getASTContext(), *res) << std::endl);
            sig.associatedConstraints.push_back(res);
        }
    }
}

// sema.DeduceTemplateArguments was exposed in commit 7415524b
#if CAIDE_CLANG_VERSION_AT_LEAST(19, 1)

llvm::SmallVector<TemplateArgument, 4> getInjectedTemplateArgs(
        ASTContext& astCtx, const TemplateParameterList& params)
{
    llvm::SmallVector<TemplateArgument, 4> Ps;
    for (NamedDecl* paramDecl : params)
        Ps.push_back(astCtx.getInjectedTemplateArg(paramDecl));
    return Ps;
}

template <typename TArgumentLocVector>
int deduceTemplateArguments(Sema& sema,
        TemplateDecl* templateDecl,
        llvm::ArrayRef<TemplateArgument> writtenArgs,
        llvm::SmallVectorImpl<TemplateArgument>& deduced,
        TArgumentLocVector& synthesizedArgLocs)
{
    const bool isFuncTemplate = isa<FunctionTemplateDecl>(templateDecl);
    TemplateParameterList* params = templateDecl->getTemplateParameters();

    // If deduced is not empty, we know unsugared form of template arguments
    // from the specialization decl. But we still want to synthesize sugared form
    // of default arguments.
    // If deduced is empty, specialization decl is not available (e.g for template type aliases).
    if (deduced.empty()) {
        llvm::SmallVector<TemplateArgument, 4> Ps = getInjectedTemplateArgs(sema.getASTContext(), *params);
        llvm::SmallVector<DeducedTemplateArgument, 4> deducedTemplateArgs(params->size());
        clang::sema::TemplateDeductionInfo deductionInfo(
                templateDecl->getBeginLoc(),
                getNumTemplateLevels(templateDecl));

        // Collects the last few arguments into a pack to match the parameter pack
        // (if any), but otherwise doesn't change arguments.
        int deductionResult = (int)sema.DeduceTemplateArguments(params, Ps, writtenArgs,
                deductionInfo, deducedTemplateArgs, false);
        if (deductionResult != 0) {
            return deductionResult;
        }

        deduced.assign(deducedTemplateArgs.begin(), deducedTemplateArgs.end());
    }

    // TODO: use correct source locations. Probably only matters in case of
    // compilation errors, which we don't expect here.
    SourceLocation templateLoc = templateDecl->getLocation();
    SourceLocation rAngleLoc = templateLoc;

    // Instantiate necessary default arguments. For function templates, some
    // template arguments could have been deduced from function arguments
    // and we don't know which template arguments were defaulted, apart from
    // explicitly provided arguments. Unnecessary instantiations can lead to
    // false positives or compilation errors (which we suppress in outer scope).
    for (unsigned i = writtenArgs.size(); i < params->size(); ++i) {
        bool hasDefaultArg = false;
        NamedDecl* param = params->getParam(i);
        // We pass 'deduced' as both SugaredConverted and CanonicalConverted arguments;
        // the latter doesn't seem to be used anyway...
        TemplateArgumentLoc defaultArgLoc = sema.SubstDefaultTemplateArgumentIfAvailable(
                templateDecl, templateLoc, rAngleLoc, param, deduced, deduced, hasDefaultArg);
        if (hasDefaultArg) {
            synthesizedArgLocs.push_back(defaultArgLoc);
            if (deduced[i].isNull()) {
                // Not filled by sema.DeduceTemplateArguments above.
                deduced[i] = defaultArgLoc.getArgument();
            }
        } else if (!isFuncTemplate) {
            // Something unexpected: no argument provided and no default argument.
            return -1;
        }
    }

    return 0;
}

void collectSugar(Sema& sema,
        TemplateDecl* templateDecl,
        const MultiLevelTemplateArgumentList& MLTAL,
        SugaredSignature& sig)
{
    TemplateParameterList* paramList = templateDecl->getTemplateParameters();

    // Substitute sugared template arguments into template parameter types.
    for (NamedDecl* paramDecl : paramList->asArray()) {
        if (const auto* NTTP = dyn_cast<NonTypeTemplateParmDecl>(paramDecl)) {
            TypeSourceInfo* substType = sema.SubstType(
                NTTP->getTypeSourceInfo(),
                MLTAL,
                NTTP->getBeginLoc(),
                {});
            if (substType) {
                // substType->getType().dump();
                sig.argTypes.push_back(substType);
            } else {
                dbg("sema.SubstType failed" << std::endl);
            }
        }
    }

    llvm::SmallVector<const Expr*, 4> constraints;
    templateDecl->getAssociatedConstraints(constraints);
    substituteExprs(sema, constraints, MLTAL, sig);
}
#endif

}

SugaredSignature substituteTemplateArguments(
        Sema& sema, TemplateDecl* templateDecl,
        llvm::ArrayRef<TemplateArgument> writtenArgs,
        llvm::ArrayRef<TemplateArgument> args)
{
    SugaredSignature ret;

#if CAIDE_CLANG_VERSION_AT_LEAST(19, 1)
    if (anyArgDependent(writtenArgs)) {
        return ret;
    }

    SuppressErrorsInScope guard(sema);

    llvm::SmallVector<TemplateArgument, 4> deducedArgs(args);

    int deductionResult = deduceTemplateArguments(sema, templateDecl, writtenArgs, deducedArgs, ret.templateArgLocs);
    if (deductionResult != 0) {
        dbg("sema.DeduceTemplateArguments failed: " << deductionResult << std::endl);
        return ret;
    }

    if (anyArgDependent(deducedArgs)) {
        return ret;
    }

    MultiLevelTemplateArgumentList MLTAL;
    MLTAL.addOuterTemplateArguments(templateDecl, deducedArgs, true);
    // TODO: are we handling nested templates properly?
    MLTAL.addOuterRetainedLevels(getNumTemplateLevels(templateDecl));

    // Push the instantiation on context stack till the end of scope.
    // This is only to avoid clang assertions in debug mode.
    Sema::InstantiatingTemplate substitutionContext(
            sema, templateDecl->getBeginLoc(), templateDecl);
    if (substitutionContext.isInvalid()) {
        dbg("Failed to create instantiation context" << std::endl);
        return ret;
    }

    collectSugar(sema, templateDecl, MLTAL, ret);
    if (auto* conceptDecl = dyn_cast<ConceptDecl>(templateDecl)) {
        Expr* constraintExpr = conceptDecl->getConstraintExpr();
        substituteExprs(sema, {constraintExpr}, MLTAL, ret);
    }
    if (auto* ftemplate = dyn_cast<FunctionTemplateDecl>(templateDecl)) {
        // Substitute sugared template arguments into function parameter types.
        FunctionDecl* func = ftemplate->getTemplatedDecl(); // Non-specialized decl.
        for (ParmVarDecl* param : func->parameters()) {
            TypeSourceInfo* paramTSI = param->getTypeSourceInfo();
            TypeSourceInfo* substType = sema.SubstType(
                paramTSI,
                MLTAL,
                param->getLocation(),
                {});
            if (substType) {
                // substType->getType().dump();
                ret.argTypes.push_back(substType);
            } else {
                dbg("sema.SubstType failed" << std::endl);
            }
        }
    }

#else
    (void)sema;
    (void)templateDecl;
    (void)writtenArgs;
    (void)args;
#endif

    return ret;
}

SugaredSignature substituteTemplateArguments(
        Sema& sema, ClassTemplatePartialSpecializationDecl* templateDecl,
        llvm::ArrayRef<TemplateArgument> args) {
    SugaredSignature ret;

#if CAIDE_CLANG_VERSION_AT_LEAST(16, 0)
    if (anyArgDependent(args)) {
        return ret;
    }

    MultiLevelTemplateArgumentList sugaredMLTAL;
    sugaredMLTAL.addOuterTemplateArguments(templateDecl, args, true);
    // TODO: are we handling nested templates properly?
    sugaredMLTAL.addOuterRetainedLevels(getNumTemplateLevels(templateDecl));

    TemplateArgumentListInfo substitutedTemplateArgs;
    const ASTTemplateArgumentListInfo* templateArgsAsWritten = templateDecl->getTemplateArgsAsWritten();
    if (!templateArgsAsWritten) {
        return ret;
    }

    clang::sema::TemplateDeductionInfo deductionInfo(
            templateDecl->getBeginLoc(),
            getNumTemplateLevels(templateDecl));
    llvm::SmallVector<TemplateArgument, 4> argsToDeduce;
    argsToDeduce.resize(templateDecl->getTemplateParameters()->size());
    // Push the instantiation on context stack till the end of scope.
    // This is only to avoid clang assertions in debug mode.
    //
    // We could use sema.DeduceTemplateArguments which pushes and pops this internally.
    // But it would probably be more expensive to perform the entire deduction, plus we want to
    // substitute actually written (sugared) arguments of the specialization.
    Sema::InstantiatingTemplate substitutionContext(
            sema, deductionInfo.getLocation(), templateDecl, argsToDeduce,
            deductionInfo);
    if (substitutionContext.isInvalid()) {
        dbg("Failed to create instantiation context for SubstTemplateArguments");
    } else {
        SuppressErrorsInScope guard(sema);
        bool error = sema.SubstTemplateArguments(templateArgsAsWritten->arguments(),
                sugaredMLTAL, substitutedTemplateArgs);
        if (error) {
            dbg("sema.SubstTemplateArguments failed" << std::endl);
        } else {
            for (unsigned i = 0; i < substitutedTemplateArgs.size(); ++i)
                ret.templateArgLocs.push_back(substitutedTemplateArgs[i]);
        }
    }

    llvm::SmallVector<const Expr*, 4> constraints;
    templateDecl->getAssociatedConstraints(constraints);
    substituteExprs(sema, constraints, sugaredMLTAL, ret);

#else
    (void)sema;
    (void)templateDecl;
    (void)args;
#endif

    return ret;
}

}}

