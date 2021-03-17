//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "../caideInliner.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>


using std::ifstream;
using std::string;
using std::vector;

static string trim(const string& s) {
    std::size_t i = s.find_first_not_of(" ");
    std::size_t j = s.find_last_not_of(" ");
    if (i != string::npos && j != string::npos && i <= j)
        return s.substr(i, j - i + 1);
    return s;
}

static bool startsWith(const string& s, const string& prefix) {
    return s.length() >= prefix.length() &&
        std::mismatch(prefix.begin(), prefix.end(), s.begin()).first == prefix.end();
}

static bool endsWith(const string& s, const string& prefix) {
    return s.length() >= prefix.length() &&
        std::mismatch(prefix.rbegin(), prefix.rend(), s.rbegin()).first == prefix.rend();
}

static vector<string> readNonEmptyLines(const string& filePath) {
    vector<string> lines;
    ifstream file{filePath.c_str()};
    string line;

    while (std::getline(file, line)) {
        if (line.find_first_not_of(" \t\r\n") != string::npos)
            lines.push_back(line);
    }

    return lines;
}

static string pathConcat(const string& directory, const string& fileName) {
    return directory + "/" + fileName;
}

static void heuristicIncludeSearchPaths(const string& tempDirectory, vector<string>& compilationOptions) {
    vector<string> searchPaths;
    // Try to infer for g++
    bool foundGccIncludeDirectories = false;
    const char* cxx = ::getenv("CXX");
    if (!cxx)
        cxx = "g++";
    const string emptyFileName = pathConcat(tempDirectory, "empty.cpp");
    const string emptyFileName2 = pathConcat(tempDirectory, "empty.exe");
    const string gccLogFileName = pathConcat(tempDirectory, "gcclog.txt");
    (void)std::ofstream(emptyFileName.c_str());
    // TODO: escape whitespace and quotes.
    // TODO: use correct locale somehow.
    std::stringstream command;
    command << cxx << " -x c++ -c -v " << emptyFileName << " -o " << emptyFileName2 << " 2>" << gccLogFileName;
    std::system(command.str().c_str());
    ifstream logFile(gccLogFileName.c_str());
    string line;
    bool isSearchList = false;
    while (std::getline(logFile, line)) {
        if (startsWith(line, "End of search list"))
            break;

        if (endsWith(line, "search starts here:"))
            isSearchList = true;
        else if (isSearchList) {
            foundGccIncludeDirectories = true;
            searchPaths.push_back(trim(line));
        }
    }

    if (foundGccIncludeDirectories) {
        // Don't let clang guess libstdc++ include directories, but still use clang builtin path.
        compilationOptions.push_back("-nostdlibinc");
    }

    for (string& s : searchPaths) {
        // std::cout << "Search path: '" << s << "'" << std::endl;
        compilationOptions.push_back("-isystem");
        compilationOptions.push_back(std::move(s));
    }
}

static bool runTest(const string& testDirectory, const string& tempDirectory) {
    // Setup
    vector<string> cppFiles = readNonEmptyLines(pathConcat(testDirectory, "fileList.txt"));
    for (string& s : cppFiles)
        s = pathConcat(testDirectory, s);

    for (int i = 1; i <= 9; ++i) {
        std::ostringstream fileName;
        fileName << i << ".cpp";
        const string filePath = pathConcat(testDirectory, fileName.str());
        ifstream file{filePath.c_str()};
        if (file)
            cppFiles.push_back(filePath);
    }

    caide::CppInliner inliner{tempDirectory};
    inliner.clangCompilationOptions = readNonEmptyLines(pathConcat(testDirectory, "clangOptions.txt"));
    for (string& opt : inliner.clangCompilationOptions) {
        const static string TEST_ROOT_MARKER = "TEST_ROOT";
        auto p = opt.find(TEST_ROOT_MARKER);
        if (p != string::npos) {
            opt.replace(p, TEST_ROOT_MARKER.length(), testDirectory);
        }
    }

    heuristicIncludeSearchPaths(tempDirectory, inliner.clangCompilationOptions);
    const char* verbose = std::getenv("CAIDE_TEST_VERBOSE");
    if (verbose && *verbose == '1')
        inliner.clangCompilationOptions.push_back("-v");

    inliner.macrosToKeep = readNonEmptyLines(pathConcat(testDirectory, "macrosToKeep.txt"));

    const string outputFilePath = pathConcat(tempDirectory, "result.cpp");

    // Run
    inliner.inlineCode(cppFiles, outputFilePath);

    // Assert
    const string etalonFilePath = pathConcat(testDirectory, "etalon.cpp");
    const vector<string> output = readNonEmptyLines(outputFilePath);
    const vector<string> etalon = readNonEmptyLines(etalonFilePath);

    const int minLength = (int)std::min(output.size(), etalon.size());
    for (int i = 0; i < minLength; ++i) {
        if (output[i] != etalon[i]) {
            std::cout
                << "< " << etalon[i] << "\n"
                << "> " << output[i] << "\n";
            return false;
        }
    }

    if (output.size() < etalon.size()) {
        std::cout << "Unexpected end of file: " << outputFilePath << "\n";
        return false;
    }

    if (output.size() > etalon.size()) {
        std::cout << "Unexpected end of file: " << etalonFilePath << "\n";
        return false;
    }

    return true;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: test-tool <temp-directory> [<test-directory>...]\n";
        return 1;
    }

    const string tempDirectory{argv[1]};

    int numFailedTests = 0;
    for (int i = 2; i < argc; ++i) {
        try {
            if (!runTest(argv[i], tempDirectory)) {
                ++numFailedTests;
            }
        } catch (const std::exception& e) {
            ++numFailedTests;
            std::cout << e.what() << "\n";
        }
    }

    return numFailedTests;
}

