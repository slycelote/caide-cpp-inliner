#pragma once

/*!
    \file caideInliner.h
    \brief C interface for caide inliner API

    See documentation in caideInliner.hpp file for details.
*/

#ifdef __cplusplus
extern "C" {
#endif

struct CaideCppInlinerOptions {
    const char* temporaryDirectory;

    const char** clangCompilationOptions;
    int numClangOptions;

    const char** macrosToKeep;
    int numMacrosToKeep;

    int maxConsequentEmptyLines;
};

void caideInlineCppCode(
        const CaideCppInlinerOptions* options,
        const char** cppFilePaths,
        int numCppFiles,
        const char* outputFilePath);

#ifdef __cplusplus
} /* extern "C" */
#endif

