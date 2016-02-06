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

    // Even if clang is built in Visual Studio, it doesn't correctly determine necessary options.
    // Try to guess them automatically.
#ifdef _MSC_VER
    std::ostringstream mscVersionOption;
    mscVersionOption << "-fmsc-version=" << _MSC_VER;
    inliner.clangCompilationOptions.push_back(mscVersionOption.str());
#if _MSC_VER >= 1900
    // VS 2015
    // https://social.msdn.microsoft.com/Forums/vstudio/en-US/86bc577b-528c-469c-a506-15383a44c111/missing-corecrth-from-the-default-include-folder-for-vs215?forum=vcgeneral
    inliner.clangCompilationOptions.push_back("-isystem");
    inliner.clangCompilationOptions.push_back("C:\\Program Files (x86)\\Windows Kits\\10\\Include\\10.0.10150.0\\ucrt");
#endif
#endif

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

