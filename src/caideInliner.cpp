//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "caideInliner.hpp"
#include "caideInliner.h"

#include "inliner.h"
#include "optimizer.h"

#include <algorithm>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>


using std::istringstream;
using std::ofstream;
using std::string;
using std::vector;


namespace caide {

static string trimEndPathSeparators(const string& path) {
    string result{path};
    auto lastSymbol = result.find_last_not_of("/\\");
    if (lastSymbol != string::npos)
        result.erase(lastSymbol + 1);
    return result;
}

CppInliner::CppInliner(const string& temporaryDirectory_)
    : clangCompilationOptions{}
    , macrosToKeep{"_WIN32", "_WIN64", "_MSC_VER", "__GNUC__"}
    , maxConsequentEmptyLines{2}
    , temporaryDirectory{trimEndPathSeparators(temporaryDirectory_)}
{
}

static void concatFiles(const vector<string>& cppFilePaths, const string& outputFilePath) {
    ofstream out{outputFilePath};
    for (const string& filePath : cppFilePaths) {
        std::ifstream in{filePath};
        if (!in)
            throw std::runtime_error(string("File not found: " + filePath));
        out << in.rdbuf();
        out << '\n'; // in case there was no return at end of file
    }
}

static bool isPragmaOnce(string line) {
    auto it = std::remove_if(line.begin(), line.end(),
                [](char c) { return c == ' ' || c == '\t' || c == '\r'; });
    line.erase(it, line.end());
    // This is technically incorrect in view of multiline macros, multiline strings etc...
    return line == "#pragmaonce";
}

static void removePragmaOnce(const string& textInBinaryMode, const string& outputFilePath) {
    istringstream in{textInBinaryMode};
    ofstream out{outputFilePath, std::ios::binary};
    string line;
    while (std::getline(in, line)) {
        if (!isPragmaOnce(line))
            out << line << '\n';
    }
}

static bool isWhitespaceOnly(const string& text) {
    return text.find_first_not_of(" \t\r") == string::npos;
}

static void removeEmptyLines(const string& textInBinaryMode,
                             int maxConsequentEmptyLines,
                             const string& outputFilePath)
{
    if (maxConsequentEmptyLines < 0)
        maxConsequentEmptyLines = std::numeric_limits<int>::max();
    istringstream in{textInBinaryMode};
    ofstream out{outputFilePath, std::ios::binary};
    int currentConsequentEmptyLines = 0;
    bool readNonEmptyLine = false;
    string line;
    while (std::getline(in, line)) {
        if (isWhitespaceOnly(line))
            ++currentConsequentEmptyLines;
        else {
            currentConsequentEmptyLines = 0;
            readNonEmptyLine = true;
        }

        if (readNonEmptyLine && currentConsequentEmptyLines <= maxConsequentEmptyLines)
            out << line << '\n';
    }
}

static string pathConcat(const string& path, const string& fileName) {
    string result{path};
    result.push_back('/');
    result += fileName;
    return result;
}

void CppInliner::inlineCode(const vector<string>& cppFilePaths, const string& outputFilePath) const {
    const string concatStage{pathConcat(temporaryDirectory, "concat.cpp")};
    const string inlinedStage{pathConcat(temporaryDirectory, "inlined.cpp")};

    concatFiles(cppFilePaths, concatStage);

    internal::Inliner inliner{clangCompilationOptions};
    std::string inlinedCode{inliner.doInline(concatStage)};
    removePragmaOnce(inlinedCode, inlinedStage);

    internal::Optimizer optimizer{clangCompilationOptions, macrosToKeep};
    std::string onlyReachableCode{optimizer.doOptimize(inlinedStage)};
    removeEmptyLines(onlyReachableCode, maxConsequentEmptyLines, outputFilePath);
}

} // namespace caide

static vector<string> arrayToCppVector(const char** array, int size) {
    vector<string> res(size);
    for (int i = 0; i < size; ++i)
        res[i].assign(array[i]);
    return res;
}

extern "C" int caideInlineCppCode(
        const CaideCppInlinerOptions* options,
        const char** cppFilePaths,
        int numCppFiles,
        const char* outputFilePath)
{
    try {
        caide::CppInliner inliner(options->temporaryDirectory);
        inliner.clangCompilationOptions = arrayToCppVector(
            options->clangCompilationOptions, options->numClangOptions);
        inliner.macrosToKeep = arrayToCppVector(options->macrosToKeep, options->numMacrosToKeep);
        inliner.maxConsequentEmptyLines = options->maxConsequentEmptyLines;
        vector<string> files = arrayToCppVector(cppFilePaths, numCppFiles);
        inliner.inlineCode(files, outputFilePath);
        return 0;
    } catch (const std::exception& e) {
        return 1;
    } catch (...) {
        return 2;
    }
}
