//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#ifdef CAIDE_DEBUG_MODE

#include <iostream>

#define dbg(vals) std::cerr << vals
#define CAIDE_FUNC __FUNCTION__ << std::endl

#else

#define dbg(vals)
#define CAIDE_FUNC ""

#endif

