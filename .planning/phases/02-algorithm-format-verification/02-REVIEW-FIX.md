---
phase: 02-algorithm-format-verification
fixed_at: 2026-10-04T01:10:00Z
review_path: .planning/phases/02-algorithm-format-verification/02-REVIEW.md
iteration: 1
findings_in_scope: 2
fixed: 2
skipped: 0
status: all_fixed
---

# Phase 02: Code Review Fix Report

**Fixed at:** 2026-10-04
**Source review:** .planning/phases/02-algorithm-format-verification/02-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 2 (fix_scope: all; IN-10 and IN-11, both in the `[ambi-pin]` order-3 decode pin)
- Fixed: 2
- Skipped: 0

This report replaces the earlier iteration-1 report for the WR-06 fix (commit 8470cd8, still in git history). The review it answers is the incremental review of delta e075576.

## Fixed Issues

### IN-10: The new `REQUIRE`s abort the whole test case on the first non-finite entry and report no location

**Files modified:** `tests/Engine/RenderEngineTests.cpp`
**Commit:** 884252c
**Applied fix:** Replaced the three per-entry `REQUIRE (std::isfinite (...))` with a per-layout `nonFinite` counter. A non-finite `dNew`, `dLib` or `dRef` is kept out of the max-reduction. Only the first bad entry per layout is reported, through `UNSCOPED_INFO` naming `s`, `c` and the library, reference and exact values. One `CHECK (nonFinite == 0)` per layout follows the loop. A bad entry no longer aborts the test case, so every layout is examined. The first-entry-only report stops a fully non-finite layout from printing up to 240 lines.

### IN-11: The guard adds about 6.3k assertions and has no negative test proving it fires

**Files modified:** `tests/Engine/RenderEngineTests.cpp`
**Commit:** 2894be6
**Applied fix:** The assertion-count half was already resolved by the IN-10 commit. For the negative-test half, the finite-check and max-reduction moved into a templated helper, `accumulateWorstFinite (T& worst, T distance)`, in the existing anonymous namespace. It returns false and leaves the maximum untouched on a non-finite distance. The `[ambi-pin]` loop now calls it for all three distances. A new `[engine][ambi-pin]` TEST_CASE feeds it `quiet_NaN`, `+infinity` and `-infinity` (double), plus `quiet_NaN` and `infinity` (float). It checks that each is rejected, that the maximum is unchanged, and that later finite values still fold in.

**Commit split:** Two commits. The split is clean. IN-10 is the inline aggregation and works on its own. IN-11 is a refactor of that code into a helper plus a new test case.

## Verification

All gates ran in the main checkout at `/Users/andrewrahman/conductor/workspaces/SpatialCore/kelowna`, which is itself an isolated Conductor git worktree on branch `gsd-remap`. No nested `.claude/worktrees/rf-*` worktree was created. `build/` and `build-release/` live in this checkout, and the requested gates run against them. So they are reproducible from this tree.

- Debug, `cmake --build build --target SpatialCoreTests -j8`, then `[ambi-pin]`: **All tests passed (141 assertions in 3 test cases)**. The count was 6590 in 2 test cases. After the IN-10 commit alone it was 125 in 2 test cases. The new helper test adds 16.
- Release, `cmake --build build-release --target SpatialCoreTests -j8`, then `[ambi-pin]`: **All tests passed (141 assertions in 3 test cases)**.
- Mutation check, test only, never committed and reverted:
  - On the IN-10 code, a NaN injected into `active.ambiDecodeMatrix[2][5]` made `[ambi-pin]` fail. Output: `CHECK( nonFinite == 0 )`, `1 == 0`, `first non-finite decode entry: s=2 c=5 lib=nan ref=... exact=...`. It failed on every layout (15 failures), which shows the other layouts are still examined.
  - The same injection on the final helper-based code failed the same way (126 passed, 15 failed).
  - Disabling the `isfinite` test inside `accumulateWorstFinite` made the new helper test fail (11 failed), so the helper test does guard the helper.
  - Confirmed afterwards that `git diff` shows no mutation and the file matches the committed content.
- Full Debug suite: 194 test cases, 193 passed, 1 failed. The one failure is `tests/Binaural/HutubsPP2Tests.cpp:47` (`checksum == kGoldenChecksum`), the expected pre-existing and unrelated failure. The "Leaked objects detected: 1 instance(s) of class FFT" message after the summary appears with that failing test.

WR-06 does not regress. A non-finite decode entry in the library, reference or exact matrix still fails `[ambi-pin]`.

---

_Fixed: 2026-10-04_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
