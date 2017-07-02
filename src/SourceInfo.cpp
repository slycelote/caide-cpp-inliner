//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "SourceInfo.h"

namespace caide {
namespace internal {

std::pair<clang::SourceLocation, clang::Decl::Kind> SourceInfo::makeKey(clang::Decl* nonImplicitDecl) {
    // TODO: use getLocStart() instead of getLocation() ?
    return std::make_pair(nonImplicitDecl->getLocation(), nonImplicitDecl->getKind());
}

}
}

