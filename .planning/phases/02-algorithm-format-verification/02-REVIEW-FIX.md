---
phase: 02-algorithm-format-verification
fixed_at: 2026-10-03T23:44:10Z
review_path: .planning/phases/02-algorithm-format-verification/02-REVIEW.md
iteration: 2
findings_in_scope: 5
fixed: 5
skipped: 0
status: all_fixed
---

# Phase 02: Code Review Fix Report

**Fixed at:** 2026-10-03T23:44:10Z
**Source review:** .planning/phases/02-algorithm-format-verification/02-REVIEW.md
**Iteration:** 2

**Summary:**
- Findings in scope: 5 (WR-08, IN-13, IN-14, IN-15, IN-16)
- Fixed: 5
- Skipped: 0

There are five commits, one per finding, from `7dd6a4d` to `52a2cb5`, on `gsd-remap`. No commit rewrites line endings. `git diff 4506c89..HEAD --shortstat` and the same command with `--ignore-cr-at-eol` both report 10 files, 229 insertions and 30 deletions.

**Where the work and the checks ran.** The fixes were made and committed in an isolated worktree (`.claude/worktrees/rf-02-…`, branch `gsd-reviewfix/02-49832`). That worktree had its own `build/` and `build-release/`, configured against this workspace's already-fetched `build/_deps` sources. `gsd-remap` was then fast-forwarded in this checkout. The worktree, its temp branch and the recovery sentinel were removed. Both suites were then rebuilt and rerun **in this main checkout** (`build/` and `build-release/`) and gave the same numbers, so everything below can be reproduced from this tree.

One deviation from the stock procedure: the fast-forward ran in this conductor workspace, which is the checkout that owns `gsd-remap`. It did not run in the first entry of `git worktree list` (`~/Projects/parked/SpatialCore`), because that checkout is on `main`. Merging there would have advanced the wrong branch.

## Verification

| Check | Result |
|---|---|
| Debug, full suite (`build/tests/SpatialCoreTests`) | 200 of 201 test cases pass, and 307,473 of 307,474 assertions. The only failure is the known `HutubsPP2Tests.cpp:47`. The FFT leak-detector line is also printed, as before. |
| Release, full suite (`build-release/tests/SpatialCoreTests`) | All 201 test cases pass (307,474 assertions). |
| Debug assertion lines | Before and after the fixes they are identical: 22 from `juce_MessageListener.cpp:50`, 1 from `RenderEngine.cpp:261` and 1 from the leak detector. None come from VBAP, VBIP or MDAP. |

The baseline before this round was 198 of 199 test cases in Debug and all 199 in Release. The 2 new test cases are WR-08 and IN-14. IN-15 adds cases to the existing WR-05 test.

**Mutation checks.** Each mutation was a throwaway edit. It was reverted with `git checkout --` and never committed.

| Guard | Mutation | Result |
|---|---|---|
| WR-08 test | Old distance handling restored (`distance = source.distance`) | Fails: 252 of 340 assertions. The ±Inf cases are silent, NaN gives equal gains instead of the 0.5 pan, and huge distances are silent. |
| WR-08 test | Non-finite distance mapped to 1.0 instead of 0.5 | Fails: 180 of 340 assertions. The gains no longer equal distance 0.5. |
| WR-08 test | Finite-distance clamp removed | Fails: 48 of 340 assertions. Huge finite distances (1e30, -1e30, 1e7) are silent. |
| IN-14 test | VBAP's empty-list branch made silent | Fails: 63 of 409 assertions. |
| IN-14 test | MDAP's main empty-list branch replaced by `return` | Fails: 24 of 409 assertions. |
| IN-14 test (diagnostic) | jassert put back in VBAP | The test still passes, but the run prints 24 `JUCE Assertion failure` lines (one per VBAP call). This shows the test reaches the old assert path. The shipped code prints 0. |
| IN-15 cases | Gap bridge stops after the first gap (`break`) | Fails, only on the new two-gap case: 8 extras instead of 14, 6,330 uncovered directions, 891 elevated-leak gains and a step of 1.0. The old 3 cases pass, so this was unpinned before. |
| IN-15 cases | `q == 0` wrap gap not unwrapped (`gap += 0.0`) | Fails: 4 assertions. Only the two-gap case has a bridged wrap gap. |
| IN-15 cases | Threshold raised to 179.6 degrees | Fails on the 179.5-degree case and the two-gap case. The old cases pass, so the 179 to 180 degree band is now pinned directly. |

## Fixed Issues

### WR-08: The guide says DBAP "never goes silent" and gives equal gains for a non-finite distance, but a non-finite distance with a finite direction gives silence

**Files modified:** `src/Algorithms/DBAPAlgorithm.cpp`, `include/SpatialCore/Algorithms/DBAPAlgorithm.h`, `docs/integration-guide.md`, `tests/Core/VBAPTripletSelectionTests.cpp`
**Commit:** 7dd6a4d
**Status:** fixed: requires human verification. This is a behaviour change chosen by the fixer, so confirm that 0.5 is the distance you want.
**Applied fix:**
- **One explicit rule, written in the code.** It no longer depends on `std::max` argument order. At the top of `computeGains`:
  - A non-finite azimuth or elevation gives equal gains of 1/sqrt(N) on every speaker. This is the same value as before, but it is now set explicitly.
  - A non-finite distance (NaN, +Inf or -Inf) is treated as **0.5**, so the source still pans by its direction. 0.5 is the `SourcePosition` default, and it is the value `RenderEngine` renders for a distance that has never been finite, so a direct caller and the engine agree.
  - A finite distance is clamped to [-1000, 1000]. The probe found a second silence: a huge *finite* distance such as 1e7 or 1e30 makes d² overflow or underflow, which zeroes every weight. Normalised distances are 0..1, so the clamp never moves a real value.
- **Result.** DBAP now returns finite, unit-power, non-silent gains for every input when `numSpeakers > 0`. It uses stack values only, with no allocation or logging. The file now includes `FloatSemanticsGuard.h`, `<cmath>` and `<algorithm>`.
- **Docs.** The integration guide now says exactly this: "DBAP never goes silent and always returns unit power: a non-finite azimuth or elevation gives equal gains (1/sqrt(N)) on every speaker, and a non-finite distance (NaN, +Inf or -Inf) is treated as 0.5, the `SourcePosition` default, so the source still pans by its direction (a finite distance is clamped to -1000..1000 so the arithmetic cannot overflow)." The `DBAPAlgorithm.h` contract says the same.
- **Tests.**
  - New `[robust]` test on Quad and 7.1.4 with 4 finite directions. For NaN, +Inf and -Inf distance it checks that the gains are finite, not all zero, unit power, and bit-equal to the distance-0.5 gains. It also checks that the distance-0.5 gains are unequal, so "pans normally" is not vacuous. For distances of 1e30, -1e30 and 1e7 it checks finite, not silent, unit power.
  - The IN-01 test's DBAP branch now checks each gain against 1/sqrt(N). Its old comment, "std::max (epsilon, NaN) returns epsilon", is gone.
- **Engine path.** Unchanged. `sanitizeSources` already makes the distance finite, and the new clamp does not affect 0..1 values.

### IN-13: Three files were converted from CRLF to LF wholesale inside content commits, hiding the real edits from review and blame

**Files modified:** `.gitattributes`
**Commit:** 52a2cb5
**Status:** fixed. The conversion is documented, and its recurrence is guarded. No line endings were rewritten.
**Applied fix:**
- **Which commits converted which files.** Before the conversion, every line of each file was CRLF:

  | File | Converting commit | CRLF lines before → after |
  |---|---|---|
  | `docs/integration-guide.md` | `8899bcf` fix(02): WR-03 make the no-enclosing-triplet path assert-free and never silent | 282 → 0 (of 282) |
  | `.claude/skills/spatial-audio-dsp/SKILL.md` | `8899bcf` (same commit) | 583 → 0 (of 583) |
  | `include/SpatialCore/IO/AmbisonicsCodec.h` | `191af3c` docs(02): IN-02 name the azimuth direction in both axis conventions | 43 → 0 (of 43) |

  The earlier phase commits (`3f88997`, `b4f0046`, `2b7653b`, `2c4e38d`) kept CRLF. Every later commit kept LF.
- **The real edits in those two commits.** Each line pair below shows the raw stat, then the stat with `--ignore-cr-at-eol`:
  - `8899bcf`: SKILL.md 1166 → **2**, and integration-guide.md 568 → **8** (7 insertions and 3 deletions in total).
  - `191af3c`: AmbisonicsCodec.h 87 → **5**, SKILL.md 4 → 4, and integration-guide.md 5 → 5.
- **Whole-phase stats, whitespace-insensitive.** Output of `git diff --ignore-cr-at-eol --stat b338fb0^..HEAD -- <files>`:
  ```
   .claude/skills/spatial-audio-dsp/SKILL.md | 42 +++++++++------
   docs/integration-guide.md                 | 87 +++++++++++++++++++++++++++++++
   include/SpatialCore/IO/AmbisonicsCodec.h  | 24 ++++++++-
   3 files changed, 135 insertions(+), 18 deletions(-)
  ```
  The raw `git diff --stat b338fb0^..HEAD` on the same files reports 1158, 525 and 78 changed lines (939 insertions and 822 deletions). Over `b338fb0^..4506c89`, the range before this round, the CR-insensitive stats are 42, 84 and 24 (132 insertions and 18 deletions). The extra 3 lines in the guide come from the WR-08 sentence in this round.
- **Guard against recurrence.** Three lines were added to `.gitattributes`, one per file: `<path> text eol=lf`. Each line covers exactly one of the three paths, and a comment names the two commits. The files are already LF in the index and on disk (`git ls-files --eol` shows `i/lf w/lf attr/text eol=lf`). The rule therefore renormalises nothing. After it was added, `git status` showed only `.gitattributes` as modified, in the worktree and again in this checkout after the fast-forward. The rest of the repo is untouched: `README.md` and `docs/workflow-tutorials.md` stay CRLF with no attribute.
- **Why not restore CRLF.** A second wholesale rewrite would hide the real edits from blame and diff again. You asked for no further line-ending rewrite.

### IN-14: `VBAPAlgorithm`, `VBIPAlgorithm` and `MDAPAlgorithm::computeGains` still `jassert` on the audio thread (the fixer's open question)

**Files modified:** `src/Algorithms/VBAPAlgorithm.cpp`, `src/Algorithms/VBIPAlgorithm.cpp`, `src/Algorithms/MDAPAlgorithm.cpp`, `tests/Core/VBAPTripletSelectionTests.cpp`
**Commit:** 8ae2eed
**Status:** fixed
**Applied fix:**
- **Asserts removed.** All three `jassert (ctx.triplets.empty() == ! layoutHasHeight (ctx.layout))` lines are gone, the same way WR-03 handled the 3D path. No Debug diagnostic is kept on the audio thread, so nothing allocates or logs there.
- **Where the invariant is still enforced.** `RenderEngine::activateLayout` already checks it on the message thread and aborts on a mismatch (D-02a). Each comment now says that, explains why there is no assert, and states the defined outcome for a hand-built context: an empty list pans by 2D VBAP (azimuth only), and a non-empty list pans by 3D VBAP.
- **Engine path.** Unchanged. The code that runs is identical.
- **Test.** A new `[robust]` test builds a 7.1.4 context with an empty triplet list. Over 24 directions, VBAP must equal `computeVBAPGains2D` exactly, and VBAP, VBIP and MDAP must be finite with unit power. In Debug, no assertion line is printed.

### IN-15: The WR-05 gap test pins one bridged gap only, so the multi-gap path and the 2(n+2g) formula for g = 2 are unpinned

**Files modified:** `tests/Core/VBAPTripletSelectionTests.cpp`
**Commit:** 5670e53
**Status:** fixed
**Applied fix:**
- **Two cases added to `cases[]`, as the review suggested.**
  - `0/0.5/180` plus 2 heights. This layout has two bridged gaps (179.5 and 180 degrees, the second one wrapping). The extras are pinned at 2(n + 2g) = 14 for n = 3 and g = 2. There must be no silent or uncovered below-horizon direction, power must be unit, and there must be no elevated-speaker leak.
  - `0/+90.25/-90.25` plus 2 heights. This is a 179.5-degree rear gap, so the 179 to 180 degree margin is tested directly, not through float rounding of an exactly-180 gap.
- **Continuity.** The 0.01 bound is kept for both new cases. The only exception is a per-case azimuth window (-1 to 1.5 degrees, two-gap case only) that covers the steep ordinary sine-law pan between the 0.5-degree pair. Steps inside that window must still be finite. Outside it, the largest step is 0.0025. The other cases have an empty window, so their checks are unchanged.

### IN-16: `FloatSemanticsGuard.h` lists "ADM-OSC parse guards" among isfinite tests, but `ADMOSCReceiver.cpp` contains none

**Files modified:** `src/Core/FloatSemanticsGuard.h`, `src/OSC/ADMOSCReceiver.cpp`
**Commit:** 29e8cf5
**Status:** fixed (comments only)
**Applied fix:**
- **Corrected reason.** The guard comment now says why `ADMOSCReceiver.cpp` includes the header. The file has no `isfinite` or `isnan` test. It forwards single-axis `/azim`, `/elev` and `/dist` messages with the other two axes set to the `NAN` macro. That is a sentinel the consumer's Listener detects with `std::isnan`, and under finite-math-only the NaN is undefined.
- **Accurate list.** The list of guarded files is now correct, and it includes DBAP's new non-finite rule from WR-08.
- **Wider include rule.** The rule now also covers files that produce NaN or Inf on purpose.
- **Include line.** The comment on the include line in `ADMOSCReceiver.cpp` names the same reason.

---

_Fixed: 2026-10-03T23:44:10Z_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 2_
