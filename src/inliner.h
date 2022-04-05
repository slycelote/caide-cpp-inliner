//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include <vector>
#include <string>
#include <unordered_set>

namespace caide {
namespace internal {

// First inliner stage: inline included headers
class Inliner {
public:
    explicit Inliner(const std::vector<std::string>& clangCommandLineOptions);

    // The file is read in binary mode, so the returned string is also
    // 'in binary mode' (contains \r\n on Windows)
    std::string doInline(const std::string& cppFile);

    // Return compilation options for the inlined file. Normally, they match
    // compilation options for the inliner provided in the constructor. But if
    // the original options contained an '-include FileName' option and FileName
    // has been inlined, this option will be removed to avoid redefinition.
    std::vector<std::string> getResultingCommandLineOptions() const;

private:
    std::vector<std::string> cmdLineOptions;
    std::unordered_set<std::string> includedHeaders;
    std::vector<std::string> inlineResults;
    std::unordered_set<std::string> inlinedPathsFromCommandLine;
};

}
}

