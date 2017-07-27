//                        Caide C++ inliner
//
// This file is distributed under the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or (at your
// option) any later version. See LICENSE.TXT for details.

#pragma once

#include <functional>
#include <map>

namespace caide {
namespace internal {


template<typename Key, typename Compare = std::less<Key>>
class IntervalSet {
private:
    using IntervalMap = std::map<Key, Key, Compare>;

public:
    using const_iterator = typename IntervalMap::const_iterator;

    explicit IntervalSet(const Compare& comp = Compare())
        : isLess(comp)
        , intervals(isLess)
    {}

    const_iterator begin() const {
        return intervals.begin();
    }

    const_iterator end() const {
        return intervals.end();
    }

    /// Add an interval [left, right], both ends inclusive.
    /// Assumes left <= right.
    void add(const Key& left, const Key& right) {
        auto it = intervals.upper_bound(left);
        auto prev = it;
        bool leftIsCovered = true;
        if (it == intervals.begin())
            leftIsCovered = false;
        else {
            --prev;
            // prev is the last interval with prev->first <= left.
            leftIsCovered = !isLess(prev->second, left);
        }

        if (leftIsCovered) {
            // left is covered by prev.
            it = prev;
            if (isLess(it->second, right)) {
                // Extend the interval.
                it->second = std::move(right);
            } else {
                // right is covered too - no changes.
                return;
            }
        } else
            it = intervals.emplace_hint(it, left, right);

        // At this point, all intervals to the left of it don't intersect each other or
        // intervals to the right of (and including) it. Some intervals following it may intersect
        // it, so we have to merge them (by extending the right end of it).
        auto jt = intervals.upper_bound(it->second);
        auto lastIntersecting = std::prev(jt);
        // Optimization: no need to call comparison function with equal intervals.
        if (lastIntersecting != it && isLess(it->second, lastIntersecting->second))
            it->second = std::move(lastIntersecting->second);

        // Intervals in (it, jt) (both non-inclusive) are now completely inside of it. Erase them.
        intervals.erase(std::next(it), jt);
    }

    /// Does any interval of the set intersect [left, right] (both ends inclusive)?
    /// Assumes left <= right.
    bool intersects(const Key& left, const Key& right) const {
        auto it = intervals.upper_bound(right);
        // If [left, right] doesn't intersect the set, it must be completely between prev(it) and it.

        if (it == intervals.begin())
            return false;
        --it;

        if (!isLess(it->second, left))
            return true;
        return false;
    }

private:
    Compare isLess;

    /// Stores pairwise non-intersecting intervals, in increasing order. Keys are left ends
    /// of the intervals, values are corresponding right ends.
    IntervalMap intervals;
};

}
}

