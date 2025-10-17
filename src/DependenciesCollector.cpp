//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "DependenciesCollector.h"
#include "clang_compat.h"
#include "clang_version.h"
#include "sema_utils.h"
#include "SourceInfo.h"
#include "util.h"

// #define CAIDE_DEBUG_MODE
#include "caide_debug.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/RawCommentList.h>
#include <clang/Basic/SourceManager.h>

#include <ostream>
#include <sstream>
#include <string>


using namespace clang;

namespace caide {
namespace internal {

bool DependenciesCollector::TraverseDecl(Decl* decl) {
    declStack.push(decl);
    bool ret = RecursiveASTVisitor<DependenciesCollector>::TraverseDecl(decl);
    declStack.pop();
    return ret;
}

void DependenciesCollector::traverseSugaredSignature(const SugaredSignature& sig, bool traverseTypeLocs) {
    for (const TemplateArgumentLoc& argLoc : sig.templateArgLocs)
        TraverseTemplateArgumentLoc(argLoc);

    for (TypeSourceInfo* t : sig.argTypes) {
        if (traverseTypeLocs) {
            TraverseTypeLoc(t->getTypeLoc());
        } else {
            TraverseType(t->getType());
        }
    }

    for (Expr* expr : sig.associatedConstraints)
        TraverseStmt(expr);
}

void DependenciesCollector::traverseTemplateSpecializationTypeImpl(
        const TemplateSpecializationType* tempSpecType, bool traverseTypeLocs)
{
    // Template specialized by this type.
    TemplateDecl* tempDecl = nullptr;

    llvm::ArrayRef<TemplateArgument> args;

    if (tempSpecType->isTypeAlias()) {
        // TODO: This type might have been traversed or not.
        // TODO: TraverseTypeLoc?
        TraverseType(tempSpecType->getAliasedType());

        // This will be TypeAliasTemplateDecl.
        tempDecl = tempSpecType->getTemplateName().getAsTemplateDecl();
    } else if (auto* specDecl = dyn_cast_or_null<ClassTemplateSpecializationDecl>(tempSpecType->getAsTagDecl())) {
        args = specDecl->getTemplateInstantiationArgs().asArray();
        llvm::PointerUnion<ClassTemplateDecl*, ClassTemplatePartialSpecializationDecl*>
            instantiatedFrom = specDecl->getSpecializedTemplateOrPartial();

        // This cannot be a partial specialization.
        tempDecl = dyn_cast_if_present<ClassTemplateDecl*>(instantiatedFrom);
    }

    if (tempDecl) {
        // These will be arguments as written at the point from where the
        // reference comes (e.g. a variable declaration).
        llvm::ArrayRef<TemplateArgument> writtenArgs{
            getArgs(*tempSpecType), getNumArgs(*tempSpecType)};
        SugaredSignature sig = substituteTemplateArguments(sema, tempDecl, writtenArgs, args);
        traverseSugaredSignature(sig, traverseTypeLocs);
    }
}

bool DependenciesCollector::TraverseTemplateSpecializationTypeLoc(TemplateSpecializationTypeLoc tempSpecTypeLoc) {
    dbg(CAIDE_FUNC);
    RecursiveASTVisitor::TraverseTemplateSpecializationTypeLoc(tempSpecTypeLoc);

    traverseTemplateSpecializationTypeImpl(tempSpecTypeLoc.getTypePtr(), true);
    return true;
}

// We duplicate this for Type and TypeLoc, as not everything in clang traversal uses TypeLocs yet.
bool DependenciesCollector::TraverseTemplateSpecializationType(TemplateSpecializationType* tempSpecType) {
    dbg(CAIDE_FUNC);
    RecursiveASTVisitor::TraverseTemplateSpecializationType(tempSpecType);

    traverseTemplateSpecializationTypeImpl(tempSpecType, false);
    return true;
}

#if CAIDE_CLANG_VERSION_AT_LEAST(10,0)
bool DependenciesCollector::TraverseAutoType(AutoType* autoType) {
    dbg(CAIDE_FUNC);
    RecursiveASTVisitor::TraverseAutoType(autoType);

    if (ConceptDecl* conceptDecl = autoType->getTypeConstraintConcept()) {
        // XXX: We might need this in stdlib for transitive dependencies, but
        // substituteTemplateArguments takes too long.
        if (sourceManager.isInMainFile(getBeginLoc(conceptDecl))) {
            llvm::SmallVector<TemplateArgument, 4> writtenArgs;
            writtenArgs.push_back(TemplateArgument(autoType->getDeducedType()));
            writtenArgs.append(autoType->getTypeConstraintArguments().begin(), autoType->getTypeConstraintArguments().end());
            SugaredSignature sig = substituteTemplateArguments(sema, conceptDecl, writtenArgs, {});
            traverseSugaredSignature(sig, /*traverseTypeLocs=*/false);
        }
    }
    return true;
}

bool DependenciesCollector::TraverseVarDecl(VarDecl* valueDecl) {
    dbg(CAIDE_FUNC);
    RecursiveASTVisitor::TraverseVarDecl(valueDecl);

    if (AutoType* autoType = valueDecl->getType().getTypePtr()->getContainedAutoType()) {
        // AutoType within AutoTypeLoc is missing deduced type: https://github.com/llvm/llvm-project/issues/42259
        // So we traverse the AutoType in addition to AutoTypeLoc.
        TraverseAutoType(autoType);
    }

    return true;
}
#endif

Decl* DependenciesCollector::getCurrentDecl() const {
    return declStack.empty() ? nullptr : declStack.top();
}

FunctionDecl* DependenciesCollector::getCurrentFunction(Decl* decl) const {
    DeclContext* declCtx = decl->getLexicalDeclContext();
    return dyn_cast_or_null<FunctionDecl>(declCtx);
}

Decl* DependenciesCollector::getParentDecl(Decl* decl) const {
    return dyn_cast_or_null<Decl>(decl->getLexicalDeclContext());
}

// A declaration in a templated context (such as a method or a field of a class template) is present
// as multiple instances in the AST: once for the template itself and once for each *implicit*
// instantiation. Clearly, it doesn't make sense to 'remove' a Decl coming from an implicit
// instantiation: what if another implicit instantiation of the same code is used? (And we don't:
// OptimizerVisitor doesn't visit implicit code.)
// We, therefore, need to add a dependency from each Decl that comes from an implicit instantiation to
// the one Decl that comes from the template itself; then if the one Decl is unreachable
// we remove it.
Decl* DependenciesCollector::getCorrespondingDeclInNonInstantiatedContext(clang::Decl* semanticDecl) const {
    // The implementation is HACKY. It relies on the assumption that a Decl inside an implicit
    // instantiation and the corresponding Decl in the non-instantiated context have the same
    // source range.
    auto key = SourceInfo::makeKey(semanticDecl);
    auto it = srcInfo.nonImplicitDecls.find(key);
    if (it != srcInfo.nonImplicitDecls.end())
        return it->second;
    return nullptr;
}

void DependenciesCollector::insertReference(Decl* from, Decl* to) {
    if (!from || !to)
        return;
    from = from->getCanonicalDecl();
    to = to->getCanonicalDecl();
    if (from == to)
        return;
    srcInfo.uses[from].insert(to);
    dbg("Reference   FROM    " << from->getDeclKindName() << " " << from
        << "<" << toString(sourceManager, from).substr(0, 20) << ">"
        << toString(sourceManager, from->getSourceRange())
        << "     TO     " << to->getDeclKindName() << " " << to
        << "<" << toString(sourceManager, to).substr(0, 20) << ">"
        << toString(sourceManager, to->getSourceRange())
        << std::endl);
}

bool DependenciesCollector::VisitType(Type* T) {
#ifdef CAIDE_DEBUG_MODE
    dbg("VisitType " << T->getTypeClassName()
        << " " << QualType(T, 0).getAsString() << std::endl);
    // T->dump();
#endif
    insertReference(getCurrentDecl(), T->getAsTagDecl());
    return true;
}

bool DependenciesCollector::VisitTypedefType(TypedefType* typedefType) {
    insertReference(getCurrentDecl(), typedefType->getDecl());
    return true;
}

bool DependenciesCollector::VisitTemplateSpecializationType(TemplateSpecializationType* tempSpecType) {
    TemplateName templateName = tempSpecType->getTemplateName();
    insertReference(getCurrentDecl(), templateName.getAsTemplateDecl());
    return true;
}

#if CAIDE_CLANG_VERSION_AT_LEAST(10,0)
bool DependenciesCollector::VisitAutoType(AutoType* autoType) {
    insertReference(getCurrentDecl(), autoType->getTypeConstraintConcept());
    return true;
}
#endif

DependenciesCollector::DependenciesCollector(SourceManager& srcMgr,
        Sema& sema_,
        const std::unordered_set<std::string>& identifiersToKeep_,
        SourceInfo& srcInfo_)
    : sourceManager(srcMgr)
    , sema(sema_)
    , identifiersToKeep(identifiersToKeep_)
    , srcInfo(srcInfo_)
{
}

bool DependenciesCollector::shouldVisitImplicitCode() const { return true; }
bool DependenciesCollector::shouldVisitTemplateInstantiations() const { return true; }
bool DependenciesCollector::shouldWalkTypesOfTypeLocs() const { return true; }

bool DependenciesCollector::VisitDecl(Decl* decl) {
    dbg("DECL " << decl->getDeclKindName() << " " << decl
        << "<" << toString(sourceManager, decl).substr(0, 30) << ">"
        << toString(sourceManager, getExpansionRange(sourceManager, decl))
        << std::endl);

    // Mark dependence on enclosing (semantic) class/namespace.
    Decl* ctx = dyn_cast_or_null<Decl>(decl->getDeclContext());
    if (ctx && !isa<FunctionDecl>(ctx))
        insertReference(decl, ctx);

    if (!sourceManager.isInMainFile(getBeginLoc(decl)))
        return true;

    // If this declaration is inside a template instantiation, mark dependence on the corresponding
    // declaration in a non-instantiated context.
    insertReference(decl, getCorrespondingDeclInNonInstantiatedContext(decl));

    // Remainder of the function processes special comments.
    RawComment* comment = decl->getASTContext().getRawCommentForDeclNoCache(decl);
    if (!comment)
        return true;

    bool invalid = false;
    const char* beg = sourceManager.getCharacterData(getBeginLoc(comment), &invalid);
    if (!beg || invalid)
        return true;

    const char* end =
        sourceManager.getCharacterData(getEndLoc(comment), &invalid);
    if (!end || invalid)
        return true;

    StringRef haystack(beg, end - beg + 1);

    {
        static const std::string caideKeepComment = "caide keep";
        StringRef needle(caideKeepComment);
        dbg(toString(sourceManager, decl) << ": " << haystack.str() << std::endl);
        if (haystack.find(needle) != StringRef::npos)
            srcInfo.declsToKeep.insert(decl);
    }

    if (ctx) {
        // The following is useful for classes implementing a standard C++ concept.
        // For instance, a custom iterator passed to std::shuffle must be a RandomAccessIterator,
        // which means that it must provide certain member functions/type aliases.
        // However, there is no way to detect this requirement, unless both the language and
        // the standard library have full concept support (which we don't want to rely on).
        // That's because some of the functions/type aliases required by the concept might
        // not actually be used by a particular implementation of std::shuffle. If we remove
        // them, the program becomes technically illegal and may not even compile in another
        // compiler.
        //
        // To work around that, it's possible to mark declarations required by some Concept
        // with a comment '/// caide concept'. This will ensure that these declarations don't
        // get removed as long as the class containing them is used.
        static const std::string caideConceptComment = "caide concept";
        StringRef needle(caideConceptComment);
        dbg(toString(sourceManager, decl) << ": " << haystack.str() << std::endl);
        if (haystack.find(needle) != StringRef::npos)
            insertReference(ctx, decl);
    }

    return true;
}

bool DependenciesCollector::VisitNamedDecl(clang::NamedDecl* decl) {
    if (!sourceManager.isInMainFile(getBeginLoc(decl)))
        return true;

    if (identifiersToKeep.count(decl->getQualifiedNameAsString()))
        srcInfo.declsToKeep.insert(decl);

    return true;
}

bool DependenciesCollector::TraverseCallExpr(CallExpr* callExpr) {
    dbg(CAIDE_FUNC);
    RecursiveASTVisitor::TraverseCallExpr(callExpr);

    Expr* callee = callExpr->getCallee();
    Decl* calleeDecl = callExpr->getCalleeDecl();

    if (!callee || !calleeDecl)
        return true;

    auto* refExpr = dyn_cast<DeclRefExpr>(callee);
    if (!refExpr) {
        auto* castExpr = dyn_cast<ImplicitCastExpr>(callee);
        if (castExpr && castExpr->getCastKind() == CK_FunctionToPointerDecay)
            refExpr = dyn_cast_if_present<DeclRefExpr>(castExpr->getSubExpr());
        if (!refExpr)
            return true;
    }

    // XXX: We might need the following in stdlib for transitive dependencies, but
    // substituteTemplateArguments takes too long.
    if (!sourceManager.isInMainFile(callExpr->getExprLoc()))
        return true;

    llvm::SmallVector<TemplateArgument, 4> writtenArgs;
    for (TemplateArgumentLoc argLoc : refExpr->template_arguments())
        writtenArgs.push_back(argLoc.getArgument());

    // Specialization called by this expr. Refers to unsugared types.
    auto* fspec = dyn_cast<FunctionDecl>(calleeDecl);
    if (!fspec)
        return true;
    FunctionTemplateSpecializationInfo* specInfo = fspec->getTemplateSpecializationInfo();
    if (!specInfo)
        return true;
    llvm::ArrayRef<TemplateArgument> args = specInfo->TemplateArguments->asArray();

    FunctionTemplateDecl* ftemplate = specInfo->getTemplate();
    SugaredSignature sig = substituteTemplateArguments(sema, ftemplate, writtenArgs, args);
    traverseSugaredSignature(sig);
    return true;
}

bool DependenciesCollector::VisitCallExpr(CallExpr* callExpr) {
    Expr* callee = callExpr->getCallee();
    Decl* calleeDecl = callExpr->getCalleeDecl();

    if (!callee || !calleeDecl || isa<UnresolvedMemberExpr>(callee) || isa<CXXDependentScopeMemberExpr>(callee))
        return true;

    insertReference(getCurrentDecl(), calleeDecl);

    return true;
}

bool DependenciesCollector::VisitCXXConstructExpr(CXXConstructExpr* constructorExpr) {
    insertReference(getCurrentDecl(), constructorExpr->getConstructor());
    return true;
}

#if CAIDE_CLANG_VERSION_AT_LEAST(10,0)
bool DependenciesCollector::TraverseConceptSpecializationExpr(ConceptSpecializationExpr* conceptExpr) {
    dbg(CAIDE_FUNC);
    RecursiveASTVisitor::TraverseConceptSpecializationExpr(conceptExpr);
    // XXX: We might need the following in stdlib for transitive dependencies, but
    // substituteTemplateArguments takes too long.
    if (!sourceManager.isInMainFile(conceptExpr->getExprLoc()))
        return true;

    ConceptDecl* conceptDecl = conceptExpr->getNamedConcept();
    const ASTTemplateArgumentListInfo* argsInfo = conceptExpr->getTemplateArgsAsWritten();
    llvm::SmallVector<TemplateArgument, 4> writtenArgs;
    for (auto argLoc : argsInfo->arguments())
        writtenArgs.push_back(argLoc.getArgument());

    SugaredSignature sig = substituteTemplateArguments(sema, conceptDecl, writtenArgs, {});
    traverseSugaredSignature(sig);
    return true;
}

bool DependenciesCollector::VisitConceptSpecializationExpr(ConceptSpecializationExpr* conceptExpr) {
    insertReference(getCurrentDecl(), conceptExpr->getNamedConcept());
    return true;
}
#endif

bool DependenciesCollector::VisitCXXConstructorDecl(CXXConstructorDecl* ctorDecl) {
#if CAIDE_CLANG_VERSION_AT_LEAST(3,9)
    CXXConstructorDecl* inheritedCtor = ctorDecl->getInheritedConstructor().getConstructor();
#else
    auto* inheritedCtor = const_cast<CXXConstructorDecl*>(ctorDecl->getInheritedConstructor());
#endif
    insertReference(ctorDecl, inheritedCtor);
    for (auto it = ctorDecl->init_begin(); it != ctorDecl->init_end(); ++it) {
        CXXCtorInitializer* ctorInit = *it;
        if (ctorInit->isWritten())
            insertReference(ctorDecl, ctorInit->getMember());
    }

    return true;
}

bool DependenciesCollector::VisitTemplateTypeParmDecl(TemplateTypeParmDecl* paramDecl) {
    // Reference from parent function/class template to its parameter, for transitivity.
    // TODO: This won't do the right thing for type aliases, as they're not a declaration context.
    insertReference(getParentDecl(paramDecl), paramDecl);

    return true;
}

bool DependenciesCollector::VisitDeclRefExpr(DeclRefExpr* ref) {
    Decl* currentDecl = getCurrentDecl();
    insertReference(currentDecl, ref->getDecl());
    insertReference(currentDecl, ref->getFoundDecl());
    return true;
}

bool DependenciesCollector::VisitValueDecl(ValueDecl* valueDecl) {
    // Mark any function as depending on its local variables.
    // TODO: detect unused local variables.
    insertReference(getCurrentFunction(valueDecl), valueDecl);
    return true;
}

bool DependenciesCollector::VisitVarDecl(VarDecl* varDecl) {
    insertReference(varDecl, varDecl->getDescribedVarTemplate());
    return true;
}

// X->F and X.F
bool DependenciesCollector::VisitMemberExpr(MemberExpr* memberExpr) {
    Decl* currentDecl = getCurrentDecl();
    // getFoundDecl() returns either MemberDecl itself or UsingShadowDecl corresponding to a UsingDecl
    insertReference(currentDecl, memberExpr->getFoundDecl().getDecl());
    return true;
}

// A using declaration has an additional shadow declaration for each declaration
// that it brings into scope.
bool DependenciesCollector::VisitUsingShadowDecl(UsingShadowDecl* usingShadowDecl) {
    // Add dependency on the actually written using declaration.
#if CAIDE_CLANG_VERSION_AT_LEAST(13,0)
    insertReference(usingShadowDecl, usingShadowDecl->getIntroducer());
#else
    insertReference(usingShadowDecl, usingShadowDecl->getUsingDecl());
#endif

    insertReference(usingShadowDecl, usingShadowDecl->getTargetDecl());
    return true;
}

bool DependenciesCollector::VisitLambdaExpr(LambdaExpr* lambdaExpr) {
    insertReference(getCurrentDecl(), lambdaExpr->getCallOperator());
    return true;
}

bool DependenciesCollector::VisitFieldDecl(FieldDecl* field) {
    insertReference(field, field->getParent());
    return true;
}

bool DependenciesCollector::VisitTypeAliasDecl(TypeAliasDecl* aliasDecl) {
    insertReference(aliasDecl, aliasDecl->getDescribedAliasTemplate());
    return true;
}

bool DependenciesCollector::VisitTypeAliasTemplateDecl(TypeAliasTemplateDecl* aliasTemplateDecl) {
    insertReference(aliasTemplateDecl, aliasTemplateDecl->getInstantiatedFromMemberTemplate());
    // Dependency on the single (pattern) TypeAlias associated with this template.
    insertReference(aliasTemplateDecl, aliasTemplateDecl->getTemplatedDecl());
    return true;
}

bool DependenciesCollector::VisitClassTemplateDecl(ClassTemplateDecl* templateDecl) {
    insertReference(templateDecl, templateDecl->getTemplatedDecl());
    return true;
}

bool DependenciesCollector::TraverseClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl* specDecl) {
    dbg(CAIDE_FUNC);
    RecursiveASTVisitor::TraverseClassTemplateSpecializationDecl(specDecl);

    llvm::PointerUnion<ClassTemplateDecl*, ClassTemplatePartialSpecializationDecl*>
        instantiatedFrom = specDecl->getSpecializedTemplateOrPartial();

    if (auto* partial = dyn_cast_if_present<ClassTemplatePartialSpecializationDecl*>(instantiatedFrom)) {
        // template<typename T>
        // class X<Foo<T>, Bar<T>> {...}
        // -> X<Foo<int>, Bar<int>>
        //
        // templateArgsAsWritten == [Foo<T>, Bar<T>]
        // instantiatedWithArgs = [int]
        //
        // To obtain correct dependencies, substitute instantiatedWithArgs into templateArgsAsWritten.
        const TemplateArgumentList& instantiatedWithArgs = specDecl->getTemplateInstantiationArgs();
        SugaredSignature sig = substituteTemplateArguments(sema, partial, instantiatedWithArgs.asArray());
        traverseSugaredSignature(sig);
    }

    return true;
}

bool DependenciesCollector::VisitClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl* specDecl) {
    llvm::PointerUnion<ClassTemplateDecl*, ClassTemplatePartialSpecializationDecl*>
        instantiatedFrom = specDecl->getSpecializedTemplateOrPartial();

    if (auto* tempDecl = dyn_cast_if_present<ClassTemplateDecl*>(instantiatedFrom)) {
        insertReference(specDecl, tempDecl);
    } else if (auto* partial = dyn_cast_if_present<ClassTemplatePartialSpecializationDecl*>(instantiatedFrom)) {
        insertReference(specDecl, partial);
    }

    return true;
}

bool DependenciesCollector::VisitVarTemplateSpecializationDecl(clang::VarTemplateSpecializationDecl* specDecl) {
    llvm::PointerUnion<VarTemplateDecl*, VarTemplatePartialSpecializationDecl*>
        instantiatedFrom = specDecl->getSpecializedTemplateOrPartial();

    if (auto* tempDecl = dyn_cast_if_present<VarTemplateDecl*>(instantiatedFrom)) {
        insertReference(specDecl, tempDecl);
    } else if (auto* partial = dyn_cast_if_present<VarTemplatePartialSpecializationDecl*>(instantiatedFrom)) {
        insertReference(specDecl, partial);
    }

    return true;
}

/*
Every function template is represented as a FunctionTemplateDecl and a FunctionDecl
(or something derived from FunctionDecl). The former contains template properties
(such as the template parameter lists) while the latter contains the actual description
of the template's contents. FunctionTemplateDecl::getTemplatedDecl() retrieves the
FunctionDecl that describes the function template,
FunctionDecl::getDescribedFunctionTemplate() retrieves the FunctionTemplateDecl
from a FunctionDecl.

We only use FunctionDecl's for dependency tracking.
 */
bool DependenciesCollector::VisitFunctionDecl(FunctionDecl* f) {
    if (f->isMain())
        srcInfo.declsToKeep.insert(f);

    if (sourceManager.isInMainFile(getBeginLoc(f)) && f->isLateTemplateParsed())
        srcInfo.delayedParsedFunctions.push_back(f);

    if (f->getTemplatedKind() == FunctionDecl::TK_FunctionTemplate) {
        // skip non-instantiated template function
        return true;
    }

    if (FunctionTemplateSpecializationInfo* specInfo = f->getTemplateSpecializationInfo()) {
        FunctionTemplateDecl* ftemplate = specInfo->getTemplate();
        insertReference(f, ftemplate->getTemplatedDecl());
    }

    insertReference(f, f->getInstantiatedFromMemberFunction());

    if (f->doesThisDeclarationHaveABody() &&
            sourceManager.isInMainFile(getBeginLoc(f)))
    {
        dbg("Moving to ";
            DeclarationName DeclName = f->getNameInfo().getName();
            std::string FuncName = DeclName.getAsString();
            std::cerr << FuncName << " at " <<
                toString(sourceManager, f->getLocation()) << std::endl;
        );
    }

    return true;
}

bool DependenciesCollector::VisitFunctionTemplateDecl(FunctionTemplateDecl* functionTemplate) {
    insertReference(functionTemplate,
            functionTemplate->getInstantiatedFromMemberTemplate());
    return true;
}

bool DependenciesCollector::VisitCXXMethodDecl(CXXMethodDecl* method) {
    insertReference(method, method->getParent());
    if (method->isVirtual()) {
        // Virtual methods may not be called directly. Assume that
        // if we need a class, we need all its virtual methods.
        // TODO: a more detailed analysis (walk the inheritance tree?)
        insertReference(method->getParent(), method);
    }
    return true;
}

bool DependenciesCollector::VisitCXXRecordDecl(CXXRecordDecl* recordDecl) {
    insertReference(recordDecl, recordDecl->getDescribedClassTemplate());
    // No implicit calls to destructors in AST; assume that
    // if a class is used, its destructor is used too.
    insertReference(recordDecl, recordDecl->getDestructor());
    return true;
}

bool DependenciesCollector::VisitEnumDecl(EnumDecl* enumDecl) {
    // Removing an unused enum constant can change values of used constants of the same enum.
    // So we assume that either the whole enum is used or it is unused. For this purpose, insert
    // bidirectional dependency links connecting the enum and each enum constant.
    for (auto it = enumDecl->enumerator_begin(); it != enumDecl->enumerator_end(); ++it) {
        insertReference(enumDecl, *it);
        insertReference(*it, enumDecl);
    }

    return true;
}

bool DependenciesCollector::VisitStmt(clang::Stmt* stmt) {
    (void)stmt;
    dbg(stmt->getStmtClassName() << std::endl);
    // stmt->dump();
    return true;
}

void DependenciesCollector::printGraph(std::ostream& out) const {
    auto locToStr = [&](const SourceLocation loc) {
        std::ostringstream str;
        str << sourceManager.getExpansionLineNumber(loc) << ":"
            << sourceManager.getExpansionColumnNumber(loc);
        return str.str();
    };
    auto getNodeId = [&](const Decl* decl) {
        SourceRange range = decl->getSourceRange();
        std::ostringstream str;
        str << '"' << decl->getDeclKindName() << "\\n";

        if (const auto* namedDecl = dyn_cast<NamedDecl>(decl))
            str << namedDecl->getNameAsString() << "\\n";

        str << decl << "\\n"
            << "<" << locToStr(range.getBegin()) << "-" << locToStr(range.getEnd()) << ">"
            << '"';
        return str.str();
    };

    const auto& graph = srcInfo.uses;
    out << "digraph {\n";
    for (const auto& it : graph) if (it.first) {
        std::string fromStr = getNodeId(it.first);
        out << fromStr << "\n";
        for (const Decl* to : it.second) if (to)
            out << fromStr << " -> " << getNodeId(to) << "\n";
    }

    out << "}\n";
}


}
}

