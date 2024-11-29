//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#include "Timer.h"

// #define CAIDE_TIMER

#ifdef CAIDE_TIMER

#include <iostream>
#include <map>
#include <stack>


namespace caide { namespace internal {

namespace {

struct TimeReport {
    using Duration = std::chrono::steady_clock::duration;
    Duration duration;

    std::map<std::string, TimeReport> children;
};

struct ReportPrinter {
    TimeReport root;
    std::stack<TimeReport*> cur;
    static const int INDENT = 2;

    ReportPrinter() { cur.push(&root); }
    ~ReportPrinter() { print(root, "", -INDENT); }

    std::chrono::milliseconds print(const TimeReport& node, const std::string& name, int indent) {
        auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(node.duration);
        auto other = durationMs;
        if (indent >= 0)
            print(durationMs, name, indent);

        for (const auto& kv : node.children)
            other -= print(kv.second, kv.first, indent + INDENT);

        if (indent >= 0 && !node.children.empty() && other > std::chrono::milliseconds{0})
            print(other, "Other", indent + INDENT);

        return durationMs;
    }

    void print(std::chrono::milliseconds duration, const std::string& name, int indent) {
        std::string durationStr = std::to_string(duration.count());
        std::cerr << durationStr << " ms ";
        for (size_t i = durationStr.size(); i < (size_t)6; ++i)
            std::cerr << ' ';
        for (int i = 0; i < indent; ++i)
            std::cerr << ' ';
        std::cerr << name << '\n';
    }
} printer;

}

ScopedTimer::ScopedTimer(const std::string& name) {
    TimeReport* prev = printer.cur.top();
    TimeReport& cur = prev->children[name];
    printer.cur.push(&cur);
    duration = &cur.duration;
    resume();
}

ScopedTimer::~ScopedTimer() {
    pause();
    printer.cur.pop();
}

void ScopedTimer::pause() {
    if (start != Clock::time_point::min()) {
        *duration += Clock::now() - start;
        start = Clock::time_point::min();
    }
}

void ScopedTimer::resume() {
    start = Clock::now();
}

}}

#else

namespace caide { namespace internal {

ScopedTimer::ScopedTimer(const std::string&) { }
ScopedTimer::~ScopedTimer() = default;
void ScopedTimer::pause() { }
void ScopedTimer::resume() { }

}}

#endif

