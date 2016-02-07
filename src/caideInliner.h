/*                        Caide C++ inliner

   This file is distributed under the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or (at your
   option) any later version. See LICENSE.TXT for details.
*/

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

int caideInlineCppCode(
        const struct CaideCppInlinerOptions* options,
        const char** cppFilePaths,
        int numCppFiles,
        const char* outputFilePath);

#ifdef __cplusplus
} /* extern "C" */
#endif
