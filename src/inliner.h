//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include <vector>
#include <string>
#include <set>

// First inliner stage: inline included headers
class Inliner {
public:
    explicit Inliner(const std::vector<std::string>& clangCommandLineOptions);

    // The file read in binary mode, so the returned string is also
    // 'in binary mode' (contains \r\n on Windows)
    std::string doInline(const std::string& cppFile);

private:
    std::vector<std::string> cmdLineOptions;
    std::set<std::string> includedHeaders;
    std::vector<std::string> inlineResults;
};

