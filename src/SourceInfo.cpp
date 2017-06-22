#include "SourceInfo.h"

namespace caide {
namespace internal {

std::pair<clang::SourceLocation, clang::Decl::Kind> SourceInfo::makeKey(clang::Decl* nonImplicitDecl) {
    // TODO: use getLocStart() instead of getLocation() ?
    return std::make_pair(nonImplicitDecl->getLocation(), nonImplicitDecl->getKind());
}

}
}

