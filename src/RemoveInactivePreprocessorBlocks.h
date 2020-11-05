//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include "clang_version.h"

#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/PPCallbacks.h>

#include <memory>
#include <set>
#include <string>

namespace clang {
    class LangOptions;
    class SourceManager;
    class MacroDirective;
}

namespace caide {
namespace internal {

class SmartRewriter;

class RemoveInactivePreprocessorBlocks: public clang::PPCallbacks {
private:
    struct RemoveInactivePreprocessorBlocksImpl;
    std::unique_ptr<RemoveInactivePreprocessorBlocksImpl> impl;

public:
    RemoveInactivePreprocessorBlocks(clang::SourceManager& sourceManager_, const clang::LangOptions& langOptions,
           SmartRewriter& rewriter_, const std::set<std::string>& macrosToKeep_);
    ~RemoveInactivePreprocessorBlocks();

    void MacroDefined(const clang::Token& MacroNameTok, const clang::MacroDirective* MD) override;

#if CAIDE_CLANG_VERSION_AT_LEAST(5,0)
    void MacroUndefined(const clang::Token& MacroNameTok, const clang::MacroDefinition& MD, const clang::MacroDirective* Undef) override;
#elif CAIDE_CLANG_VERSION_AT_LEAST(3,7)
    void MacroUndefined(const clang::Token& MacroNameTok, const clang::MacroDefinition& MD) override;
#else
    void MacroUndefined(const clang::Token& MacroNameTok, const clang::MacroDirective* MD) override;
#endif

#if CAIDE_CLANG_VERSION_AT_LEAST(3,7)
    void MacroExpands(const clang::Token& /*MacroNameTok*/, const clang::MacroDefinition& MD,
                      clang::SourceRange Range, const clang::MacroArgs* /*Args*/) override;
#else
    void MacroExpands(const clang::Token& /*MacroNameTok*/, const clang::MacroDirective* MD,
                      clang::SourceRange Range, const clang::MacroArgs* /*Args*/) override;
#endif

    void If(clang::SourceLocation Loc, clang::SourceRange ConditionRange, ConditionValueKind ConditionValue) override;
#if CAIDE_CLANG_VERSION_AT_LEAST(3,7)
    void Ifdef(clang::SourceLocation Loc, const clang::Token& MacroNameTok, const clang::MacroDefinition& /*MD*/) override;
    void Ifndef(clang::SourceLocation Loc, const clang::Token& MacroNameTok, const clang::MacroDefinition& /*MD*/) override;
#else
    void Ifdef(clang::SourceLocation Loc, const clang::Token& MacroNameTok, const clang::MacroDirective* /*MD*/) override;
    void Ifndef(clang::SourceLocation Loc, const clang::Token& MacroNameTok, const clang::MacroDirective* /*MD*/) override;
#endif

    void Elif(clang::SourceLocation Loc, clang::SourceRange ConditionRange, ConditionValueKind ConditionValue, clang::SourceLocation /*IfLoc*/ ) override;
    void Else(clang::SourceLocation Loc, clang::SourceLocation /*IfLoc*/) override;
    void Endif(clang::SourceLocation Loc, clang::SourceLocation /*IfLoc*/) override;

    // EndOfMainFile() is called too late; instead, we call this one manually in the consumer.
    void Finalize();
};

}
}

