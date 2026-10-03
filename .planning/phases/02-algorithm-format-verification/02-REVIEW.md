---
phase: 02-algorithm-format-verification
reviewed: 2026-10-04T00:40:00Z
depth: standard
files_reviewed: 1
files_reviewed_list:
  - tests/Engine/RenderEngineTests.cpp
findings:
  critical: 0
  warning: 0
  info: 2
  total: 2
status: issues_found
---

# Phase 02: Code Review Report (incremental, delta e075576)

**Reviewed:** 2026-10-04T00:40:00Z
**Depth:** standard
**Files Reviewed:** 1
**Status:** issues_found

## Summary

Scope: the WR-06 fix in `tests/Engine/RenderEngineTests.cpp` (lines 814-830), plus the rest of the two `[ambi-pin]` test cases.

**The fix is correct and complete.** Every verification below was checked against the source.

- **Correct.** `std::max (a, b)` returns `(a < b) ? b : a`. With `a` the running max and `b` NaN, `a < b` is false and `a` is kept, so the old code did drop NaN. The new code checks the three distances before the reduction, so a NaN never reaches `std::max`. The comment states this accurately.
- **Covers every operand.** A NaN or Inf in any of `active.ambiDecodeMatrix`, `reference` or `exact` makes at least one of `dNew`, `dLib`, `dRef` non-finite. Inf minus Inf gives NaN, which is also caught. So the three distance checks cover all three matrices.
- **No other max-reduction was missed.** The only other one is `worst = std::max (worst, worstHere)` (line 837). `worstHere` is already proven finite, so it cannot drop NaN.
- **Full index range is covered.** `MAX_SPEAKERS == 16` (`include/SpatialCore/Core/Types.h:9`), so the `c < 16` loop covers every column. The rows `>= numSpeakers` are covered by the second test case. Its `!= 0.0f` comparison is true for NaN, so it does count NaN as stale.
- **The other test cases have no hole.** `CHECK (worstHere <= tol)` is false for NaN. `doublePrecisionAmbiDecode` and `referenceAmbiDecode` propagate NaN through the divisions and sums into the outputs. The `std::abs (...) > std::abs (...)` pivot search can skip a NaN, but that NaN is then divided into the row and reaches the output.
- **`<cmath>` is included** (line 7), so `std::isfinite` is declared.
- **Verified by running.** Debug (`build/`) and Release (`build-release/`) both pass: `All tests passed (6590 assertions in 2 test cases)`.

The fix has two minor ergonomic costs, listed below.

## Info

### IN-10: The new `REQUIRE`s abort the whole test case on the first non-finite entry and report no location

**File:** `tests/Engine/RenderEngineTests.cpp:824-826`
**Issue:**
- `REQUIRE` throws on failure, so the first bad entry in the first bad layout ends the test case. The remaining layouts are never examined. The loop's other assertions are `CHECK`s, which keep going.
- The failure message is only `REQUIRE( std::isfinite( dNew ) )` expanded to `false`. The `INFO` at line 803 names the format. Nothing names the speaker `s` or the column `c`, and the offending value is not shown. The per-layout `worstHere`, `libVsExact` and `refVsExact` INFOs (lines 831-833) are declared after the loop, so they are not in scope when the `REQUIRE` fires.
- The test cannot tell which of the three matrices was bad (library, reference or exact), which is the first question when debugging a NaN.

**Fix:** Collect the failures instead of aborting, and report them once per layout. This also removes the roughly 6.3k per-entry assertions (see IN-11):

```cpp
int nonFinite = 0;
for (int s = 0; s < active.layout.numSpeakers; ++s)
    for (int c = 0; c < 16; ++c)
    {
        const float  dNew = std::abs (active.ambiDecodeMatrix[s][c] - reference[s][c]);
        const double dLib = std::abs (static_cast<double> (active.ambiDecodeMatrix[s][c]) - exact[s][c]);
        const double dRef = std::abs (static_cast<double> (reference[s][c]) - exact[s][c]);
        if (! std::isfinite (dNew) || ! std::isfinite (dLib) || ! std::isfinite (dRef))
        {
            if (nonFinite++ == 0)
                UNSCOPED_INFO ("first non-finite entry: s=" << s << " c=" << c
                               << " lib=" << active.ambiDecodeMatrix[s][c]
                               << " ref=" << reference[s][c] << " exact=" << exact[s][c]);
            continue;   // keep it out of the max-reduction
        }
        worstHere  = std::max (worstHere, dNew);
        libVsExact = std::max (libVsExact, dLib);
        refVsExact = std::max (refVsExact, dRef);
    }
CHECK (nonFinite == 0);
```

If abort-on-first-failure is intended, a `CAPTURE (s, c)` or `INFO` inside the loop restores the location. It is cheap at this size.

### IN-11: The guard adds about 6.3k assertions and has no negative test proving it fires

**File:** `tests/Engine/RenderEngineTests.cpp:824-826`
**Issue:**
- **Assertion count.** The `[ambi-pin]` pair went from about 300 assertions to 6590. Of those, about 6.3k are the per-entry `isfinite` `REQUIRE`s (3 per entry across about 140 speakers times 16 columns). Catch2 counts every one, so reporter output with `-s` and the totals in CI logs become noisy for what is one logical check ("the decode is finite"). Aggregating (see IN-10) restores one assertion per layout.
- **No evidence the guard fires.** WR-06 was a vacuous-pass bug, and the fix is likewise unproven. Nothing demonstrates that a NaN decode now fails the test. A mutation check, even a throwaway one outside the suite, would show it. A cheap in-suite alternative is to factor the finite-check and max-reduction into a small helper such as `accumulateWorst (double&, double)` that returns false on non-finite input, and give the helper a direct `TEST_CASE` with `NaN` and `Inf` inputs. The checks then regress loudly.

**Fix:** Use the aggregation from IN-10, plus a unit test of the helper with `std::numeric_limits<float>::quiet_NaN()` and `infinity()` (`<limits>` is already included).

---

_Reviewed: 2026-10-04T00:40:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
