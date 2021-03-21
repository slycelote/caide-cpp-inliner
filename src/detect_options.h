//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include <string>
#include <vector>

namespace caide {
namespace internal {

std::vector<std::string> detectClangOptions(const std::string& temporaryDirectory);

}
}
