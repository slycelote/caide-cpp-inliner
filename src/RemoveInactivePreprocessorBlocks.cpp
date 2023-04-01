//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "RemoveInactivePreprocessorBlocks.h"
#include "SmartRewriter.h"
#include "util.h"

#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Preprocessor.h>

#include <map>
#include <set>
#include <string>
#include <vector>


using namespace clang;
using std::map;
using std::set;
using std::string;
using std::vector;


namespace caide {
namespace internal {

struct IfDefClause {
    // Locations of #if, #ifdef, #ifndef, #else, #elif tokens of this clause
    vector<SourceLocation> locations;

    // Index of selected branch in locations list; -1 if no branch was selected
    int selectedBranch;

    bool keepAllBranches;

    explicit IfDefClause(const SourceLocation& ifLocation)
        : selectedBranch(-1)
        , keepAllBranches(false)
    {
        locations.push_back(ifLocation);
    }
};

struct Macro {
    SourceRange definition;
    vector<SourceRange> usages;
    SourceLocation undefinition;
    bool isWhitelisted = false;
};

struct RemoveInactivePreprocessorBlocks::RemoveInactivePreprocessorBlocksImpl {
private:
    SourceManager& sourceManager;
    const LangOptions& langOptions;
    SmartRewriter& rewriter;
    const set<string>& macrosToKeep;

    vector<IfDefClause> activeClauses;

    // Currently defined macros
    map<const MacroDirective*, Macro> definedMacros;

    // Macros that were #defined, and then #undefined.
    vector<Macro> undefinedMacros;
private:
    bool isWhitelistedMacro(const string& macroName) const {
        return macrosToKeep.find(macroName) != macrosToKeep.end();
    }

    bool isInMainFile(SourceLocation loc) const {
        // sourceManager.isInMainFile returns true for builtin defines
        return sourceManager.getFileID(loc) == sourceManager.getMainFileID();
    }

public:
    RemoveInactivePreprocessorBlocksImpl(
            SourceManager& sourceManager_, const LangOptions& langOptions_,
            SmartRewriter& rewriter_, const set<string>& macrosToKeep_)
        : sourceManager(sourceManager_)
        , langOptions(langOptions_)
        , rewriter(rewriter_)
        , macrosToKeep(macrosToKeep_)
    {
    }

    void MacroDefined(const Token& MacroNameTok, const MacroDirective* MD) {
        string macroName = getTokenName(MacroNameTok);
        bool isWhitelisted = isWhitelistedMacro(macroName);

        if (MD && isInMainFile(MD->getLocation())) {
            SourceLocation b = MD->getLocation(), e;
            if (const MacroInfo* info = MD->getMacroInfo())
                e = info->getDefinitionEndLoc();
            else
                e = changeColumn(b, 10000);

            b = changeColumn(b, 1);

            Macro macro;
            macro.definition = SourceRange(b, e);
            macro.isWhitelisted = isWhitelisted;
            definedMacros[MD] = std::move(macro);
        }
    }

    void MacroUndefined(const Token& MacroNameTok, const MacroDirective* MD) {
        if (!MD || !isInMainFile(MD->getLocation()))
            return;

        auto it = definedMacros.find(MD);
        if (it != definedMacros.end()) {
            it->second.undefinition = MacroNameTok.getLocation();
            undefinedMacros.push_back(it->second);
            definedMacros.erase(it);
        }
    }

    void MacroExpands(const MacroDirective* MD, SourceRange Range) {
        if (!MD || !isInMainFile(MD->getLocation()))
            return;

        definedMacros[MD].usages.push_back(Range);
    }

    // This is where we remove unused macros.
    // We can be sure that a macro is unused only after we analyzed the AST
    // (e.g. it could be referenced in an unused function.)
    // There is PPCallbacks::EndOfMainFile(), however it seems to be called only after
    // some required resources have been deallocated. So we call this method manually instead.
    void Finalize() {
        auto removeMacroIfUnused = [this](const Macro& macro) {
            if (macro.isWhitelisted)
                return;

            for (const SourceRange& usageRange : macro.usages) {
                if (!rewriter.isPartOfRangeRemoved(usageRange)) {
                    // The usage of the macro has not been removed, so
                    // we can't remove the definition.
                    return;
                }
            }

            rewriter.removeRange(macro.definition);

            if (macro.undefinition.isValid()) {
                SourceLocation b = changeColumn(macro.undefinition, 1);
                SourceLocation e = changeColumn(macro.undefinition, 10000);
                rewriter.removeRange(b, e);
            }
        };

        // Remove unused #defines that don't have a corresponding #undef
        for (const auto& kv : definedMacros)
            removeMacroIfUnused(kv.second);
        // Remove previously #defined macros.
        for (const Macro& macro : undefinedMacros)
            removeMacroIfUnused(macro);
    }

    void If(SourceLocation Loc, SourceRange ConditionRange, ConditionValueKind ConditionValue) {
        if (!isInMainFile(Loc))
            return;
        activeClauses.push_back(IfDefClause(Loc));
        if (ConditionValue == CVK_True)
            activeClauses.back().selectedBranch = 0;
        if (containsWhitelistedString(ConditionRange))
            activeClauses.back().keepAllBranches = true;
    }

    void Ifdef(SourceLocation Loc, const Token& MacroNameTok, bool isMacroDefined) {
        if (!isInMainFile(Loc))
            return;
        activeClauses.push_back(IfDefClause(Loc));
        string macroName = getTokenName(MacroNameTok);
        if (isMacroDefined)
            activeClauses.back().selectedBranch = 0;
        if (isWhitelistedMacro(macroName))
            activeClauses.back().keepAllBranches = true;
    }

    void Ifndef(SourceLocation Loc, const Token& MacroNameTok, bool isMacroDefined) {
        if (!isInMainFile(Loc))
            return;
        activeClauses.push_back(IfDefClause(Loc));
        string macroName = getTokenName(MacroNameTok);
        if (!isMacroDefined)
            activeClauses.back().selectedBranch = 0;
        if (isWhitelistedMacro(macroName))
            activeClauses.back().keepAllBranches = true;
    }

    void Elif(SourceLocation Loc, SourceRange ConditionRange, ConditionValueKind ConditionValue, SourceLocation /*IfLoc*/ ) {
        if (!isInMainFile(Loc))
            return;
        if (ConditionValue == CVK_True)
            activeClauses.back().selectedBranch = (int)activeClauses.back().locations.size();
        activeClauses.back().locations.push_back(Loc);
        if (containsWhitelistedString(ConditionRange))
            activeClauses.back().keepAllBranches = true;
    }

    void Else(SourceLocation Loc, SourceLocation /*IfLoc*/) {
        if (!isInMainFile(Loc))
            return;
        if (activeClauses.back().selectedBranch < 0)
            activeClauses.back().selectedBranch = (int)activeClauses.back().locations.size();
        activeClauses.back().locations.push_back(Loc);
    }

    void Endif(SourceLocation Loc, SourceLocation /*IfLoc*/) {
        if (!isInMainFile(Loc))
            return;
        IfDefClause& clause = activeClauses.back();
        clause.locations.push_back(Loc);

        if (clause.keepAllBranches) {
            // do nothing
        } else if (clause.selectedBranch < 0) {
            // remove all branches
            SourceLocation b = changeColumn(clause.locations.front(), 1);
            SourceLocation e = changeColumn(clause.locations.back(), 10000);
            rewriter.removeRange(b, e);
        } else {
            // remove all branches except selected
            SourceLocation b = changeColumn(clause.locations.front(), 1);
            SourceLocation e = changeColumn(clause.locations[clause.selectedBranch], 10000);
            rewriter.removeRange(b, e);

            b = changeColumn(clause.locations[clause.selectedBranch + 1], 1);
            e = changeColumn(clause.locations.back(), 10000);
            rewriter.removeRange(b, e);
        }

        activeClauses.pop_back();
    }

    void InclusionDirective(SourceLocation HashLoc, CharSourceRange FilenameRange)
    {
        if (!isInMainFile(HashLoc))
            return;
        if (!activeClauses.empty())
            return;

        SourceLocation end = FilenameRange.getEnd();
        const char* s = sourceManager.getCharacterData(HashLoc);
        const char* e = sourceManager.getCharacterData(end);
        if (!s || !e)
            return;

        rewriter.appendToPreamble(string(s, e));
        rewriter.appendToPreamble("\n");
        rewriter.removeRange(HashLoc, end);
    }


private:
    string getTokenName(const Token& token) const {
        const char* b = sourceManager.getCharacterData(token.getLocation(), 0);
        const char* e = b + token.getLength();
        return string(b, e);
    }

    SourceLocation changeColumn(SourceLocation loc, unsigned col) const {
        std::pair<FileID, unsigned> decomposedLoc = sourceManager.getDecomposedLoc(loc);
        FileID fileId = decomposedLoc.first;
        unsigned filePos = decomposedLoc.second;
        unsigned line = sourceManager.getLineNumber(fileId, filePos);
        return sourceManager.translateLineCol(fileId, line, col);
    }

    bool containsWhitelistedString(SourceRange range) const {
        const char* b, *e;
        std::tie(b, e) = getCharRange(range, sourceManager, langOptions);
        if (!b || !e)
            return true;
        string rangeContent(b, e);
        for (const auto& s: macrosToKeep)
            if (rangeContent.find(s) != string::npos)
                return true;
        return false;
    }
};


RemoveInactivePreprocessorBlocks::RemoveInactivePreprocessorBlocks(
        SourceManager& sourceManager, const LangOptions& langOptions,
        SmartRewriter& rewriter, const set<string>& macrosToKeep)
    : impl(new RemoveInactivePreprocessorBlocksImpl(sourceManager, langOptions, rewriter, macrosToKeep))
{
}

RemoveInactivePreprocessorBlocks::~RemoveInactivePreprocessorBlocks() = default;

void RemoveInactivePreprocessorBlocks::MacroDefined(
        const Token& MacroNameTok, const MacroDirective* MD)
{
    impl->MacroDefined(MacroNameTok, MD);
}

#if CAIDE_CLANG_VERSION_AT_LEAST(5,0)
void RemoveInactivePreprocessorBlocks::MacroUndefined(
    const Token& MacroNameTok, const MacroDefinition& MacroDef, const MacroDirective* /*Undef*/)
{
    const DefMacroDirective* MD = MacroDef.getLocalDirective();
    impl->MacroUndefined(MacroNameTok, MD);
}
#elif CAIDE_CLANG_VERSION_AT_LEAST(3,7)
void RemoveInactivePreprocessorBlocks::MacroUndefined(
    const Token& MacroNameTok, const MacroDefinition& MacroDef)
{
    const DefMacroDirective* MD = MacroDef.getLocalDirective();
    impl->MacroUndefined(MacroNameTok, MD);
}
#else
void RemoveInactivePreprocessorBlocks::MacroUndefined(
    const Token& MacroNameTok, const MacroDirective* MD)
{
    impl->MacroUndefined(MacroNameTok, MD);
}
#endif

#if CAIDE_CLANG_VERSION_AT_LEAST(3,7)
void RemoveInactivePreprocessorBlocks::MacroExpands(
    const Token& /*MacroNameTok*/, const MacroDefinition& MacroDef,
    SourceRange Range, const MacroArgs* /*Args*/)
{
    const DefMacroDirective* MD = MacroDef.getLocalDirective();
    impl->MacroExpands(MD, Range);
}
#else
void RemoveInactivePreprocessorBlocks::MacroExpands(
    const Token& /*MacroNameTok*/, const MacroDirective* MD,
    SourceRange Range, const MacroArgs* /*Args*/)
{
    impl->MacroExpands(MD, Range);
}
#endif

void RemoveInactivePreprocessorBlocks::Finalize() {
    impl->Finalize();
}

void RemoveInactivePreprocessorBlocks::If(
    SourceLocation Loc, SourceRange ConditionRange,
    ConditionValueKind ConditionValue)
{
    impl->If(Loc, ConditionRange, ConditionValue);
}

#if CAIDE_CLANG_VERSION_AT_LEAST(3,7)
void RemoveInactivePreprocessorBlocks::Ifdef(
    SourceLocation Loc, const Token& MacroNameTok, const MacroDefinition& MD)
#else
void RemoveInactivePreprocessorBlocks::Ifdef(
    SourceLocation Loc, const Token& MacroNameTok, const MacroDirective* MD)
#endif
{
    impl->Ifdef(Loc, MacroNameTok, (bool)MD);
}

#if CAIDE_CLANG_VERSION_AT_LEAST(3,7)
void RemoveInactivePreprocessorBlocks::Ifndef(
    SourceLocation Loc, const Token& MacroNameTok, const MacroDefinition& MD)
#else
void RemoveInactivePreprocessorBlocks::Ifndef(
    SourceLocation Loc, const Token& MacroNameTok, const MacroDirective* MD)
#endif
{
    impl->Ifndef(Loc, MacroNameTok, (bool)MD);
}

void RemoveInactivePreprocessorBlocks::Elif(
    SourceLocation Loc, SourceRange ConditionRange,
    ConditionValueKind ConditionValue, SourceLocation IfLoc)
{
    impl->Elif(Loc, ConditionRange, ConditionValue, IfLoc);
}

void RemoveInactivePreprocessorBlocks::Else(SourceLocation Loc, SourceLocation IfLoc) {
    impl->Else(Loc, IfLoc);
}

void RemoveInactivePreprocessorBlocks::Endif(SourceLocation Loc, SourceLocation IfLoc) {
    impl->Endif(Loc, IfLoc);
}

void RemoveInactivePreprocessorBlocks::InclusionDirective(
    SourceLocation HashLoc,
    const Token& /*IncludeTok*/,
    StringRef /*FileName*/,
    bool /*IsAngled*/,
    CharSourceRange FilenameRange,
#if CAIDE_CLANG_VERSION_AT_LEAST(16, 0)
    clang::OptionalFileEntryRef /*File*/,
#elif CAIDE_CLANG_VERSION_AT_LEAST(15, 0)
    llvm::Optional<FileEntryRef> /*File*/,
#else
    const FileEntry* /*File*/,
#endif
    StringRef /*SearchPath*/,
    StringRef /*RelativePath*/,
    const Module* /*Imported*/
#if CAIDE_CLANG_VERSION_AT_LEAST(7, 0)
    , SrcMgr::CharacteristicKind /*FileType*/
#endif
    )
{
    impl->InclusionDirective(HashLoc, FilenameRange);
}

}
}

