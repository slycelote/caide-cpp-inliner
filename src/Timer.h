//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include <chrono>
#include <string>

namespace caide { namespace internal {

class ScopedTimer {
public:
    ScopedTimer(const std::string& name);
    ~ScopedTimer();

    void pause();
    void resume();

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point start;
    Clock::duration* duration = nullptr;
};

} }
