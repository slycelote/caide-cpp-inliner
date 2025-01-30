//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "util.h"
#include "clang_compat.h"
#include "clang_version.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/Utils.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Tooling/CompilationDatabase.h>

#include <llvm/Support/raw_ostream.h>

#include <sstream>
#include <string>

using namespace clang;

namespace caide {
namespace internal {

// Copied from lib/ARCMigrate/Transforms.cpp

/// \brief \arg Loc is the end of a statement range. This returns the location
/// of the token of expected type following the statement.
/// If no token of this type is found or the location is inside a macro, the returned
/// source location will be invalid.
SourceLocation findTokenAfterLocation(SourceLocation loc, ASTContext& Ctx,
        tok::TokenKind tokenType, bool IsDecl)
{
    SourceManager &SM = Ctx.getSourceManager();
    if (loc.isMacroID()) {
        if (!Lexer::isAtEndOfMacroExpansion(loc, SM, Ctx.getLangOpts(), &loc))
            return SourceLocation();
    }
    loc = Lexer::getLocForEndOfToken(loc, /*Offset=*/0, SM, Ctx.getLangOpts());

    // Break down the source location.
    std::pair<FileID, unsigned> locInfo = SM.getDecomposedLoc(loc);

    // Try to load the file buffer.
    bool invalidTemp = false;
    StringRef file = SM.getBufferData(locInfo.first, &invalidTemp);
    if (invalidTemp)
        return SourceLocation();

    const char *tokenBegin = file.data() + locInfo.second;

    // Lex from the start of the given location.
    Lexer lexer(SM.getLocForStartOfFile(locInfo.first), Ctx.getLangOpts(),
            file.begin(), tokenBegin, file.end());
    Token tok;
    lexer.LexFromRawLexer(tok);
    if (tok.isNot(tokenType)) {
        if (!IsDecl)
            return SourceLocation();
        // Declaration may be followed with other tokens; such as an __attribute,
        // before ending with a semicolon.
        return findTokenAfterLocation(tok.getLocation(), Ctx, tokenType, /*IsDecl*/true);
    }

    return tok.getLocation();
}

SourceLocation findTokenAfterLocation(SourceLocation loc, ASTContext& Ctx, tok::TokenKind tokenType) {
    return findTokenAfterLocation(loc, Ctx, tokenType, false);
}

SourceLocation findSemiAfterLocation(SourceLocation loc, ASTContext& Ctx, bool IsDecl) {
    return findTokenAfterLocation(loc, Ctx, tok::semi, IsDecl);
}

/// \brief 'Loc' is the end of a statement range. This returns the location
/// immediately after the semicolon following the statement.
/// If no semicolon is found or the location is inside a macro, the returned
/// source location will be invalid.
SourceLocation findLocationAfterSemi(SourceLocation loc, ASTContext& Ctx, bool IsDecl) {
    SourceLocation SemiLoc = findSemiAfterLocation(loc, Ctx, IsDecl);
    if (SemiLoc.isInvalid())
        return SourceLocation();
    return SemiLoc.getLocWithOffset(1);
}

std::pair<const char*, const char*> getCharRange(SourceRange range, const SourceManager& sourceManager, const LangOptions& langOpts) {
#if CAIDE_CLANG_VERSION_AT_LEAST(7,0)
    CharSourceRange rng = sourceManager.getExpansionRange(range);
#else
    range = sourceManager.getExpansionRange(range);
    // The following is actually incorrect because range could be a token range,
    // in which case we must use CharSourceRange::getTokenRange(). The only place
    // where we've encountered it so far is the range of #if condition with a newer clang,
    // which is covered by the other implementation above.
    // So for our use cases this should work. If we actually get token ranges from
    // older clang versions, we could copy the implementation of getExpansionRange
    // from latest clang.
    CharSourceRange rng = CharSourceRange::getCharRange(range);
#endif
    rng = Lexer::makeFileCharRange(rng, sourceManager, langOpts);
    if (rng.isInvalid())
        return {nullptr, nullptr};
    bool Invalid = false;
    const char* b = sourceManager.getCharacterData(rng.getBegin(), &Invalid);
    if (Invalid)
        return {nullptr, nullptr};
    const char* e = sourceManager.getCharacterData(rng.getEnd(), &Invalid);
    if (Invalid)
        return {nullptr, nullptr};
    return {b, e};
}

std::unique_ptr<tooling::FixedCompilationDatabase> createCompilationDatabaseFromCommandLine(const std::vector<std::string>& cmdLine)
{
    int argc = (int)cmdLine.size() + 1;
    std::vector<const char*> argv(argc);
    argv[0] = "--";

    for (int i = 1; i < argc; ++i)
        argv[i] = cmdLine[i-1].c_str();

#if CAIDE_CLANG_VERSION_AT_LEAST(5,0)
    std::string errorMessage;
    return tooling::FixedCompilationDatabase::loadFromCommandLine(argc, &argv[0], errorMessage);
#else
    tooling::FixedCompilationDatabase* rawPtr = tooling::FixedCompilationDatabase::loadFromCommandLine(argc, &argv[0]);
    return std::unique_ptr<tooling::FixedCompilationDatabase>(rawPtr);
#endif
}

std::string rangeToString(SourceManager& sourceManager, const SourceLocation& start, const SourceLocation& end) {
    bool invalid;
    const char* b = sourceManager.getCharacterData(start, &invalid);
    if (invalid || !b)
        return "<invalid>";
    const char* e = sourceManager.getCharacterData(end, &invalid);
    if (invalid || !e)
        return "<invalid>";
    if (e < b + 30)
        return std::string(b, e);
    else
        return std::string(b, b+30) + "[...]";
}

std::string toString(SourceManager& sourceManager, SourceLocation loc) {
    //return loc.printToString(sourceManager);
    std::string fileName = sourceManager.getFilename(loc).str();
    if (fileName.length() > 30)
        fileName = fileName.substr(fileName.length() - 30);
    std::ostringstream os;
    os << fileName << ":" <<
        sourceManager.getExpansionLineNumber(loc) << ":" <<
        sourceManager.getExpansionColumnNumber(loc);
    return os.str();
}

#if CAIDE_CLANG_VERSION_AT_LEAST(7,0)
static void debug(SourceManager& sourceManager, std::ostringstream& os, SourceLocation loc) {
    if (!loc.isValid()) os << "[invalid]";
    else if (!loc.isFileID()) os << "[not file]";
    else {
        std::string fileName = sourceManager.getFilename(loc).str();
        if (fileName.length() > 30)
            fileName = fileName.substr(fileName.length() - 30);
        os << fileName << ":" <<
            sourceManager.getSpellingLineNumber(loc) << ":" <<
            sourceManager.getSpellingColumnNumber(loc);
    }
}

static void debug(SourceManager& sourceManager, std::ostringstream& os, CharSourceRange rng) {
    if (rng.isInvalid()) os << "{invalid range}";
    else {
        os << "{";
        if (rng.isTokenRange()) os << "token ";
        if (rng.isCharRange()) os << "char ";
        os << "range}";
        debug(sourceManager, os, rng.getBegin());
        os << "--";
        debug(sourceManager, os, rng.getEnd());
    }
}

std::string toString(SourceManager& sourceManager, SourceRange range, const LangOptions* langOpts /*=nullptr*/) {
    std::ostringstream os;
    CharSourceRange rng;
    rng = sourceManager.getExpansionRange(range);
    debug(sourceManager, os, rng);
    if (langOpts) {
        rng = Lexer::makeFileCharRange(rng, sourceManager, *langOpts);
        os << " ";
        debug(sourceManager, os, rng);
    }
    return os.str();
}

#else
std::string toString(SourceManager& sourceManager, SourceRange range, const LangOptions*) {
    return toString(sourceManager, range.getBegin()) + " -- " +
        toString(sourceManager, range.getEnd());
}
#endif

std::string toString(SourceManager& sourceManager, const Decl* decl) {
    if (!decl)
        return "<invalid>";
    SourceLocation start = sourceManager.getExpansionLoc(getBeginLoc(decl));
    bool invalid;
    const char* b = sourceManager.getCharacterData(start, &invalid);
    if (invalid || !b)
        return "<invalid>";
    SourceLocation end = sourceManager.getExpansionLoc(getEndLoc(decl));
    const char* e = sourceManager.getCharacterData(end, &invalid);
    if (invalid || !e)
        return "<invalid>";
    return std::string(b, std::min(b+30, e));
}

std::string toString(ASTContext& ctx, const Stmt& stmt) {
    std::string res;
    llvm::raw_string_ostream os(res);
#if CAIDE_CLANG_VERSION_AT_LEAST(11,0)
    stmt.dump(os, ctx);
#else
    stmt.dump(os, ctx.getSourceManager());
#endif
    return res;
}

#if CAIDE_CLANG_VERSION_AT_LEAST(7,0)
static SourceLocation getBegin(const CharSourceRange& charSourceRange) {
    return charSourceRange.getBegin();
}

static SourceLocation getEnd(const CharSourceRange& charSourceRange) {
    return charSourceRange.getEnd();
}
#else
static SourceLocation getBegin(const std::pair<SourceLocation, SourceLocation>& charSourceRange) {
    return charSourceRange.first;
}

static SourceLocation getEnd(const std::pair<SourceLocation, SourceLocation>& charSourceRange) {
    return charSourceRange.second;
}
#endif

SourceLocation getExpansionStart(SourceManager& sourceManager, const Decl* decl) {
    SourceLocation start = getBeginLoc(decl);
    if (start.isMacroID())
        start = getBegin(sourceManager.getExpansionRange(start));
    return start;
}

SourceLocation getExpansionEnd(SourceManager& sourceManager, const Decl* decl) {
    SourceLocation end = getEndLoc(decl);
    if (end.isMacroID())
        end = getEnd(sourceManager.getExpansionRange(end));
    return end;
}

SourceRange getExpansionRange(SourceManager& sourceManager, const Decl* decl) {
    return SourceRange(getExpansionStart(sourceManager, decl),
            getExpansionEnd(sourceManager, decl));
}

}
}

