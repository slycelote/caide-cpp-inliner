#include "../caideInliner.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>


using namespace std;

int main(int argc, const char* argv[]) {
    vector<string> sourceFiles;
    string tmpDirectory = "./caide-tmp";
    string outputFile = "./caide-tmp/result.cpp";
    vector<string> clangOptions;
    vector<string> macrosToKeep;
    int maxConsecutiveEmptyLines = 2;

    const string clangOptionsEnd = "--";
    const string directoryFlag = "-d";
    const string outputFlag = "-o";
    const string keepMacrosFlag = "-k";
    const string emptyLinesFlag = "-l";

    int i = 1;
    for (; i < argc && clangOptionsEnd != argv[i]; ++i) {
        clangOptions.emplace_back(argv[i]);
    }

    for (++i; i < argc; ++i) {
        if (directoryFlag == argv[i]) {
            ++i;
            if (i < argc) tmpDirectory = argv[i];
        } else if (outputFlag == argv[i]) {
            ++i;
            if (i < argc) outputFile = argv[i];
        } else if (keepMacrosFlag == argv[i]) {
            ++i;
            if (i < argc) macrosToKeep.emplace_back(argv[i]);
        } else if (emptyLinesFlag == argv[i]) {
            ++i;
            if (i < argc) maxConsecutiveEmptyLines = strtol(argv[i], nullptr, 10);
        } else {
            sourceFiles.emplace_back(argv[i]);
        }
    }

    caide::CppInliner inliner(tmpDirectory);
    inliner.clangCompilationOptions.swap(clangOptions);
    inliner.macrosToKeep.insert(inliner.macrosToKeep.end(),
        macrosToKeep.begin(), macrosToKeep.end());
    inliner.maxConsequentEmptyLines = maxConsecutiveEmptyLines;
    inliner.inlineCode(sourceFiles, outputFile);

    return 0;
}

