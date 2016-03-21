//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "UsedDeclarations.h"
#include "util.h"

//#define CAIDE_DEBUG_MODE
#include "caide_debug.h"

#include <clang/Basic/SourceManager.h>

#include <set>


using namespace clang;


namespace caide {
namespace internal {


UsedDeclarations::UsedDeclarations(SourceManager& sourceManager_)
    : sourceManager(sourceManager_)
{
}

bool UsedDeclarations::contains(Decl* decl) const {
    if (usedDecls.find(decl) != usedDecls.end())
        return true;

    // This is HACKY. A declaration in a templated context (such as a method or a field of a class
    // template) is present in multiple instances in the AST: once for the template itself and
    // once for each *implicit* instantiation. Clearly, it doesn't make sense to 'remove' a
    // Decl coming from an implicit instantiation: what if another implicit instantiation
    // of the same code is used? (And we don't: the OptimizerVisitor doesn't visit implicit code.)
    // The correct way to know if *any* implicit instantiation of the same code is used
    // would be to add a dependency from each Decl coming from an implicit instantiation to
    // the one Decl coming from the template itself; then if the one Decl is unreachable
    // we remove it. Unfortunately, there seems to be no way to say 'find the Decl in
    // non-instantiated context corresponding to this Decl'.
    // Our solution is to use the SourceRange of a Decl as its fingerprint. All instances of
    // a single declaration have the same SourceRange, regardless of where they come from:
    // the template itself or one of implicit instantiations. We add source ranges of used
    // Decl's to a set; to check if a declaration is used, look up its SourceRange in the set.
    //
    // Note that tracking usedDecls above is not necessary, except (maybe) for optimization
    // (SourceRangeComparer is relatively slow).
    SourceRange range = getSourceRange(decl);
    return locationsOfUsedDecls.find(range) != locationsOfUsedDecls.end();
}

void UsedDeclarations::add(Decl* decl) {
    SourceRange range = getSourceRange(decl);

    // Typically we are only interested in Decls defined in user code. However, in case user code
    // contains using directives such as 'using namespace std' or 'using std::vector', we need to
    // know whether those are used. For now, keep track of all Decls; in the future we might store
    // only namespaces and types, in addition to Decls defined in user code.
    dbg("USAGEINFO " <<
        decl->getDeclKindName() << " " << decl
        << "<" << toString(sourceManager, decl).substr(0, 30) << ">"
        << toString(sourceManager, range)
        << std::endl);
    usedDecls.insert(decl);
    locationsOfUsedDecls.insert(range);
}

SourceRange UsedDeclarations::getSourceRange(Decl* decl) const {
    SourceRange range = getExpansionRange(sourceManager, decl);
    /*
    Decl* anotherDecl = nullptr;
    if (FunctionDecl* f = dyn_cast_or_null<FunctionDecl>(decl))
        anotherDecl = f->getDescribedFunctionTemplate();
    else if (FunctionTemplateDecl* ft = dyn_cast_or_null<FunctionTemplateDecl>(decl))
        anotherDecl = ft;

    if (anotherDecl) {
        SourceRange anotherRange = getExpansionRange(sourceManager, anotherDecl);
        if (sourceManager.isBeforeInTranslationUnit(anotherRange.getBegin(), range.getBegin()))
            range = anotherRange;
    }
    */
    return range;
}

}
}

