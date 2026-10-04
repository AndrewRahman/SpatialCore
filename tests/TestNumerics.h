#pragma once

#include <algorithm>
#include <cmath>

// Shared numeric helpers for SpatialCore's test files (test scope only).

namespace spatialcore_test
{

/** Folds one distance into a running maximum. std::max (a, b) is
    (a < b) ? b : a, so a NaN distance would be silently dropped and never
    reach a tolerance CHECK (WR-06, WR-07): a "worst <= bound" check then passes
    vacuously on garbage. A non-finite distance is therefore rejected here: it
    returns false and leaves the maximum untouched, so the caller can count it
    and fail loudly with CHECK (nonFinite == 0). */
template <typename T>
bool accumulateWorstFinite (T& worst, T distance)
{
    if (! std::isfinite (distance))
        return false;
    worst = std::max (worst, distance);
    return true;
}

} // namespace spatialcore_test
