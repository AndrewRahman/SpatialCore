---
phase: 02-algorithm-format-verification
fixed_at: 2026-10-04T00:00:00Z
review_path: .planning/phases/02-algorithm-format-verification/02-REVIEW.md
iteration: 1
findings_in_scope: 1
fixed: 1
skipped: 0
status: all_fixed
---

# Phase 02: Code Review Fix Report

**Fixed at:** 2026-10-04
**Source review:** .planning/phases/02-algorithm-format-verification/02-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 1 (fix_scope: critical_warning; IN-07, IN-08 and IN-09 are out of scope)
- Fixed: 1
- Skipped: 0

## Fixed Issues

### WR-06: NaN decode entries are silently ignored, so the pin (and the new anchor checks) can pass vacuously

**Files modified:** `tests/Engine/RenderEngineTests.cpp`
**Commit:** e075576
**Applied fix:** In the per-layout loop of the order-3 decode pin, the three per-entry distances (`dNew`, `dLib`, `dRef`) are now computed into locals. Each is checked with `REQUIRE (std::isfinite (...))` before it enters the `std::max` reduction. A NaN in `active.ambiDecodeMatrix`, `reference` or `exact` now fails the test instead of being dropped. The reviewer's suggested fix was applied as written. The code still matched the review context (lines 817-819). A comment at the site explains why `std::max` drops NaN. `<cmath>` was already included.

Status note: this is a test-only change. It adds assertions and does not change any library behaviour or tolerance. It is not a logic-bug fix to library code, so no human logic verification is flagged.

## Verification

**Where verification ran:** the main checkout of this worktree (`/Users/andrewrahman/conductor/workspaces/SpatialCore/kelowna`) on branch `gsd-remap`. The existing `build/` (Debug) and `build-release/` (Release) trees were used, so the numbers are reproducible from this tree. No nested worktree was created, because the orchestrator ran this agent inside an already-isolated worktree and asked it to stay there. The fix was committed directly on `gsd-remap`.

Both trees were rebuilt with `cmake --build <dir> -j`. Both built cleanly.

| Run | Result |
|-----|--------|
| `build/tests/SpatialCoreTests "[ambi-pin]"` (Debug) | All tests passed: 6590 assertions in 2 test cases |
| `build-release/tests/SpatialCoreTests "[ambi-pin]"` (Release) | All tests passed: 6590 assertions in 2 test cases |
| `build/tests/SpatialCoreTests` (full suite, Debug) | 193 test cases: 192 passed, 1 failed. 311348 assertions: 311347 passed, 1 failed |

The assertion count for `[ambi-pin]` rose from 110 (reviewer's pre-fix figure) to 6590. The new `REQUIRE (std::isfinite)` calls contribute 3 per speaker/coefficient entry across the 15 layouts, so the NaN guards are actually being evaluated.

**Pre-existing failure (known, unrelated, not touched):** `tests/Binaural/HutubsPP2Tests.cpp:47`, `CHECK (checksum == kGoldenChecksum)`. The actual checksum was 8806157918509638672 and the golden value was 11402032843575911607. It is the only failing assertion in the full suite. The run also printed a JUCE leaked-object (FFT) assertion at process exit, which comes from the same HRTF test run and is not related to this change.

---

_Fixed: 2026-10-04_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
