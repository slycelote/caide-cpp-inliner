//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "DependenciesCollector.h"
#include "clang_compat.h"
#include "clang_version.h"
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
using std::set;

namespace caide {
namespace internal {

bool DependenciesCollector::TraverseDecl(Decl* decl) {
    declStack.push(decl);
    bool ret = RecursiveASTVisitor<DependenciesCollector>::TraverseDecl(decl);
    declStack.pop();
    return ret;
}


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

void DependenciesCollector::insertReferenceToType(Decl* from, const Type* to,
        set<const Type*>& seen)
{
    if (!to)
        return;

    if (!seen.insert(to).second)
        return;

    dbg("Reference   FROM    " << from->getDeclKindName() << " " << from
        << "<" << toString(sourceManager, from).substr(0, 20) << ">"
        << toString(sourceManager, from->getSourceRange())
        << "     TO TYPE " << to->getTypeClassName() << " " << to
        << " " << QualType(to, 0).getAsString()
        << std::endl);

    if (const ElaboratedType* elaboratedType = dyn_cast<ElaboratedType>(to)) {
        insertReferenceToType(from, elaboratedType->getNamedType(), seen);
        return;
    }

    if (const ParenType* parenType = dyn_cast<ParenType>(to))
        insertReferenceToType(from, parenType->getInnerType(), seen);

    insertReference(from, to->getAsTagDecl());

    if (const ArrayType* arrayType = dyn_cast<ArrayType>(to))
        insertReferenceToType(from, arrayType->getElementType(), seen);

    if (const PointerType* pointerType = dyn_cast<PointerType>(to))
        insertReferenceToType(from, pointerType->getPointeeType(), seen);

    if (const ReferenceType* refType = dyn_cast<ReferenceType>(to)) {
        insertReferenceToType(from, refType->getPointeeTypeAsWritten(), seen);
        insertReferenceToType(from, refType->getPointeeType(), seen);
    }

    if (const TypedefType* typedefType = dyn_cast<TypedefType>(to))
        insertReference(from, typedefType->getDecl());

    if (const TemplateSpecializationType* tempSpecType =
            dyn_cast<TemplateSpecializationType>(to))
    {
        if (tempSpecType->isTypeAlias())
            insertReferenceToType(from, tempSpecType->getAliasedType());
        if (TemplateDecl* tempDecl = tempSpecType->getTemplateName().getAsTemplateDecl())
            insertReference(from, tempDecl);
        for (unsigned i = 0; i < tempSpecType->getNumArgs(); ++i) {
            const TemplateArgument& arg = tempSpecType->getArg(i);
            if (arg.getKind() == TemplateArgument::Type)
                insertReferenceToType(from, arg.getAsType(), seen);
        }
    }
}

void DependenciesCollector::insertReferenceToType(Decl* from, QualType to,
        set<const Type*>& seen)
{
    insertReferenceToType(from, to.getTypePtrOrNull(), seen);
}

void DependenciesCollector::insertReferenceToType(Decl* from, QualType to)
{
    set<const Type*> seen;
    insertReferenceToType(from, to, seen);
}

void DependenciesCollector::insertReferenceToType(Decl* from, const Type* to)
{
    set<const Type*> seen;
    insertReferenceToType(from, to, seen);
}

void DependenciesCollector::insertReferenceToType(Decl* from, const TypeSourceInfo* typeSourceInfo) {
    if (typeSourceInfo)
        insertReferenceToType(from, typeSourceInfo->getType());
}

DependenciesCollector::DependenciesCollector(SourceManager& srcMgr, SourceInfo& srcInfo_)
    : sourceManager(srcMgr)
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

    if (ctx)
    {
        // The following is useful for classes implementing a standard C++ concept.
        // For instance, a custom iterator passed to std::shuffle must be a RandomAccessIterator,
        // which means that it must provide a certain number of member functions/type aliases.
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

bool DependenciesCollector::VisitCallExpr(CallExpr* callExpr) {
    dbg(CAIDE_FUNC);
    Expr* callee = callExpr->getCallee();
    Decl* calleeDecl = callExpr->getCalleeDecl();

    if (!callee || !calleeDecl || isa<UnresolvedMemberExpr>(callee) || isa<CXXDependentScopeMemberExpr>(callee))
        return true;

    insertReference(getCurrentDecl(), calleeDecl);
    return true;
}

bool DependenciesCollector::VisitCXXConstructExpr(CXXConstructExpr* constructorExpr) {
    dbg(CAIDE_FUNC);
    insertReference(getCurrentDecl(), constructorExpr->getConstructor());
    return true;
}

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
            insertReference(getCurrentDecl(), ctorInit->getMember());
        insertReferenceToType(getCurrentDecl(), ctorInit->getBaseClass());
        insertReferenceToType(getCurrentDecl(), ctorInit->getTypeSourceInfo());
    }

    return true;
}

bool DependenciesCollector::VisitCXXTemporaryObjectExpr(CXXTemporaryObjectExpr* tempExpr) {
    insertReferenceToType(getCurrentDecl(), tempExpr->getTypeSourceInfo());
    return true;
}

bool DependenciesCollector::VisitTemplateTypeParmDecl(TemplateTypeParmDecl* paramDecl) {
    if (paramDecl->hasDefaultArgument())
        insertReferenceToType(getParentDecl(paramDecl), paramDecl->getDefaultArgument());
    return true;
}

bool DependenciesCollector::VisitCXXNewExpr(CXXNewExpr* newExpr) {
    insertReferenceToType(getCurrentDecl(), newExpr->getAllocatedType());
    return true;
}

void DependenciesCollector::insertReference(Decl* from, NestedNameSpecifier* specifier) {
    while (specifier) {
        insertReferenceToType(from, specifier->getAsType());
        specifier = specifier->getPrefix();
    }
}

bool DependenciesCollector::VisitDeclRefExpr(DeclRefExpr* ref) {
    dbg(CAIDE_FUNC);
    Decl* currentDecl = getCurrentDecl();
    insertReference(currentDecl, ref->getDecl());
    insertReference(currentDecl, ref->getFoundDecl());
    insertReference(currentDecl, ref->getQualifier());
    return true;
}

bool DependenciesCollector::VisitCXXScalarValueInitExpr(CXXScalarValueInitExpr* initExpr) {
    insertReferenceToType(getCurrentDecl(), initExpr->getTypeSourceInfo());
    return true;
}

bool DependenciesCollector::VisitExplicitCastExpr(ExplicitCastExpr* castExpr) {
    insertReferenceToType(getCurrentDecl(), castExpr->getTypeAsWritten());
    return true;
}

bool DependenciesCollector::VisitValueDecl(ValueDecl* valueDecl) {
    dbg(CAIDE_FUNC);
    // Mark any function as depending on its local variables.
    // TODO: detect unused local variables.
    insertReference(getCurrentFunction(valueDecl), valueDecl);

    insertReferenceToType(valueDecl, valueDecl->getType());
    return true;
}

// X->F and X.F
bool DependenciesCollector::VisitMemberExpr(MemberExpr* memberExpr) {
    dbg(CAIDE_FUNC);
    Decl* currentDecl = getCurrentDecl();
    // getFoundDecl() returns either MemberDecl itself or UsingShadowDecl corresponding to a UsingDecl
    insertReference(currentDecl, memberExpr->getFoundDecl().getDecl());
    insertReference(currentDecl, memberExpr->getQualifier());
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

// using ns::identifier, using BaseType::identifier
bool DependenciesCollector::VisitUsingDecl(UsingDecl* usingDecl) {
    insertReference(usingDecl, usingDecl->getQualifier());
    return true;
}

bool DependenciesCollector::VisitLambdaExpr(LambdaExpr* lambdaExpr) {
    dbg(CAIDE_FUNC);
    insertReference(getCurrentDecl(), lambdaExpr->getCallOperator());
    return true;
}

bool DependenciesCollector::VisitFieldDecl(FieldDecl* field) {
    dbg(CAIDE_FUNC);
    insertReference(field, field->getParent());
    return true;
}

// Includes both typedef and type alias
bool DependenciesCollector::VisitTypedefNameDecl(TypedefNameDecl* typedefDecl) {
    dbg(CAIDE_FUNC);
    insertReferenceToType(typedefDecl, typedefDecl->getUnderlyingType());
    return true;
}

bool DependenciesCollector::VisitTypeAliasDecl(TypeAliasDecl* aliasDecl) {
    dbg(CAIDE_FUNC);
    insertReference(aliasDecl, aliasDecl->getDescribedAliasTemplate());
    return true;
}

bool DependenciesCollector::VisitTypeAliasTemplateDecl(TypeAliasTemplateDecl* aliasTemplateDecl) {
    dbg(CAIDE_FUNC);
    insertReference(aliasTemplateDecl, aliasTemplateDecl->getInstantiatedFromMemberTemplate());
    return true;
}

bool DependenciesCollector::VisitClassTemplateDecl(ClassTemplateDecl* templateDecl) {
    dbg(CAIDE_FUNC);
    insertReference(templateDecl, templateDecl->getTemplatedDecl());
    return true;
}

bool DependenciesCollector::VisitClassTemplateSpecializationDecl(ClassTemplateSpecializationDecl* specDecl) {
    dbg(CAIDE_FUNC);
    // specDecl is canonical (e.g. all typedefs are removed).
    // Add reference to the type that is actually written in the code.
    insertReferenceToType(specDecl, specDecl->getTypeAsWritten());

    llvm::PointerUnion<ClassTemplateDecl*, ClassTemplatePartialSpecializationDecl*>
        instantiatedFrom = specDecl->getSpecializedTemplateOrPartial();

    if (instantiatedFrom.is<ClassTemplateDecl*>())
        insertReference(specDecl, instantiatedFrom.get<ClassTemplateDecl*>());
    else if (instantiatedFrom.is<ClassTemplatePartialSpecializationDecl*>())
        insertReference(specDecl, instantiatedFrom.get<ClassTemplatePartialSpecializationDecl*>());

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
    dbg(CAIDE_FUNC);
    if (f->isMain())
        srcInfo.declsToKeep.insert(f);

    if (sourceManager.isInMainFile(getBeginLoc(f)) && f->isLateTemplateParsed())
        srcInfo.delayedParsedFunctions.push_back(f);

    if (f->getTemplatedKind() == FunctionDecl::TK_FunctionTemplate) {
        // skip non-instantiated template function
        return true;
    }

    FunctionTemplateSpecializationInfo* specInfo = f->getTemplateSpecializationInfo();
    if (specInfo) {
        insertReference(f, specInfo->getTemplate()->getTemplatedDecl());
        // Add references to template argument types as they are written in code, not the canonical types.
        if (const ASTTemplateArgumentListInfo* templateArgs = specInfo->TemplateArgumentsAsWritten) {
            for (unsigned i = 0; i < templateArgs->NumTemplateArgs; ++i) {
                const TemplateArgumentLoc& argLoc = (*templateArgs)[i];
                if (argLoc.getArgument().getKind() == TemplateArgument::Type)
                    insertReferenceToType(f, argLoc.getTypeSourceInfo());
            }
        }
    }

    insertReferenceToType(f, f->getReturnType());

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
    dbg(CAIDE_FUNC);
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

    if (recordDecl->isThisDeclarationADefinition()) {
        for (const CXXBaseSpecifier* base = recordDecl->bases_begin();
             base != recordDecl->bases_end(); ++base)
        {
            insertReferenceToType(recordDecl, base->getType());
        }
    }
    return true;
}

// sizeof, alignof
bool DependenciesCollector::VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr* expr) {
    if (expr->isArgumentType())
        insertReferenceToType(getCurrentDecl(), expr->getArgumentType());
    // if the argument is a variable it will be processed as DeclRefExpr
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

    // reference to underlying type
    insertReferenceToType(enumDecl, enumDecl->getIntegerType());
    return true;
}

bool DependenciesCollector::VisitStmt(clang::Stmt* stmt) {
    (void)stmt;
    //dbg(stmt->getStmtClassName() << std::endl);
    //stmt->dump();
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

