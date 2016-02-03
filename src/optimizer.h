//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include <vector>
#include <set>
#include <string>

// Second inliner stage: remove unused code
class Optimizer {
public:
    Optimizer(const std::vector<std::string>& cmdLineOptions,
              const std::vector<std::string>& macrosToKeep);
    std::string doOptimize(const std::string& cppFile);

private:
    std::vector<std::string> cmdLineOptions;
    std::set<std::string> macrosToKeep;
};

