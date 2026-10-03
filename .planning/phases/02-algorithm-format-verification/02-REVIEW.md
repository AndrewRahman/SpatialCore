---
phase: 02-algorithm-format-verification
reviewed: 2026-10-04T00:00:00Z
depth: standard
files_reviewed: 42
files_reviewed_list:
  - .claude/skills/spatial-audio-dsp/SKILL.md
  - .gitignore
  - CMakeLists.txt
  - README.md
  - docs/integration-guide.md
  - include/SpatialCore/Algorithms/DBAPAlgorithm.h
  - include/SpatialCore/Algorithms/MDAPAlgorithm.h
  - include/SpatialCore/Algorithms/VBIPAlgorithm.h
  - include/SpatialCore/Core/SpatialMath.h
  - include/SpatialCore/Engine/RenderEngine.h
  - include/SpatialCore/IO/AmbisonicsCodec.h
  - include/SpatialCore/IO/SpeakerLayout.h
  - src/Algorithms/AmbisonicsAlgorithm.cpp
  - src/Algorithms/ConstantPowerAlgorithm.cpp
  - src/Algorithms/DirectBinauralAlgorithm.cpp
  - src/Algorithms/KNNAlgorithm.cpp
  - src/Algorithms/MDAPAlgorithm.cpp
  - src/Algorithms/VBAPAlgorithm.cpp
  - src/Algorithms/VBIPAlgorithm.cpp
  - src/Core/FloatSemanticsGuard.h
  - src/Core/SpatialMath.cpp
  - src/Engine/RenderEngine.cpp
  - src/IO/AmbisonicsCodec.cpp
  - src/IO/SpeakerLayout.cpp
  - src/OSC/ADMOSCReceiver.cpp
  - tests/Algorithms/PanningLawTests.cpp
  - tests/Algorithms/SpatializationAlgorithmTests.cpp
  - tests/CMakeLists.txt
  - tests/Core/VBAPTripletSelectionTests.cpp
  - tests/Engine/RenderEngineTests.cpp
  - tests/IO/AmbisonicsCodecTests.cpp
  - tests/IO/SpeakerLayoutTests.cpp
  - tests/TestNumerics.h
  - tests/reference/.gitignore
  - tests/reference/EarReference.h
  - tests/reference/PanningReference.h
  - tests/reference/README.md
  - tests/reference/ShReference.h
  - tests/reference/gen_ear_reference.py
  - tests/reference/gen_panning_reference.py
  - tests/reference/gen_sh_reference.py
  - tests/reference/layouts_from_cpp.py
findings:
  critical: 0
  warning: 1
  info: 4
  total: 5
status: issues_found
---

# Phase 2: Code Review Report

**Reviewed:** 2026-10-04
**Depth:** standard
**Files Reviewed:** 42
**Status:** issues_found

## Summary

This is the re-review after the fix round `3a2a1b2..8afcc75` (15 prior findings). The review covered
`git diff b338fb0^..HEAD` with priority on the production changes of that round. Both suites were
built and run: Debug has 198 of 199 test cases passing (the only failure is the known
`HutubsPP2Tests.cpp:47` checksum, plus the known FFT leak-detector line), and Release has all 199
passing. The working tree was not modified. The probe programs live under `/tmp/probe` and link
against the existing `build/libSpatialCore.a`.

**All 15 prior findings are verified fixed. None regressed. No finding reuses an old ID.**
There are no critical findings. No realtime-safety violation was introduced by the fix round.

What I checked beyond reading the diff:

- **WR-05 gap bridge (`src/IO/SpeakerLayout.cpp`).** I wrote an independent probe that builds
  both triplet lists and sweeps below-horizon directions at 0.1 degree azimuth resolution. I ran it
  over rear gaps of 170, 178, 178.9, 178.99, 179.0, 179.01, 179.5, 179.9, 179.99, 180, 180.5, 200,
  250, 300, 340, 358 and 359.9 degrees. I also ran two-gap layouts (gaps of 179.5 and 180, and of
  179.2 and 180), a 3-speaker cluster, elevated ear-level speakers, and 40 seeded random clustered
  rings. Results:
  - Silent directions: zero in every case.
  - Power off unit: zero in every case.
  - Elevated-speaker leak (a source below the horizon feeding a speaker above it): zero, except for
    one probe layout that deliberately had an ear-level speaker at -8 degrees. There the leak comes
    from the regular triplets of that speaker, not from the bridge.
  - Extras: 2(n+2g) on every non-degenerate case, and 2n whenever no gap is bridged.
  - Gain steps: at or below 0.0053 for a rear gap of 179 degrees or more. The only larger steps
    are at neighbouring speakers a degree or less apart, which is the ordinary sine-law slope of the
    pan.

  For a rear gap of 179 to 180 degrees the bridge removes a real discontinuity. With the threshold
  mutated to exactly 180 degrees, gaps of 179.99 and 180.00 degrees give a hard 1.0 gain step; with
  the shipped 179 degree threshold they give 0.0025. The 1 degree margin is therefore justified.
- **Shipped layouts.** I dumped the widest ear-level gap and the extras per layout: the widest gap
  is 140 degrees on 5.0, 5.1, 5.1.2 and 5.1.4, and the extras equal exactly 2n on all 15. The
  construction code path is unchanged for them (same vertex order, same `makeHullVertex` inputs).
- **WR-03 fallback.** It uses only stack floats and has no logging, locking or allocation. It runs
  only when `usedFallback` is set and the clamped result is silent. An empty list and a non-finite
  direction stay silent. Confirmed.
- **WR-02 `kind()`.** It is integer-only, derived from existing fields, and matches the old
  `selectionTier` rule for every case, including the hand-built `lowerHemisphere` triplet with
  `nadirVertex == -1`, which is classed as a cap exactly as before.
- **WR-04 `FloatSemanticsGuard.h`.** It is private to `src/`, included only from `.cpp` files with a
  relative path, and reachable from no public header. Consumer targets that use fast-math on their
  own targets are unaffected. It fires for the SpatialCore target's flags however they arrive.
  The consumer claim about OpenSpatialDelay could not be checked, because no OSD checkout exists on
  this machine.
- **IN-03 `getDecodeMatrix`.** Changing `void` to `bool` is source-compatible for callers that
  ignore the result. There are no other callers in the repo. The `activateLayout` check runs on the
  message thread. The stack arrays total about 32 KB, as documented.
- **`ADMOSCReceiver.cpp`.** The diff is the include only.

## Warnings

### WR-08: The guide says DBAP "never goes silent" and gives equal gains for a non-finite distance, but a non-finite distance with a finite direction gives silence

**File:** `docs/integration-guide.md:253-254` (claim); `src/Algorithms/DBAPAlgorithm.cpp:21-37` (behaviour);
`tests/Core/VBAPTripletSelectionTests.cpp:700-755` (the test that pins only part of the claim)
**Issue:** The IN-01 fix rewrote the guide to say: "DBAP never goes silent: a non-finite direction
or distance gives equal gains on every speaker." The sentence is false for distance. I called
`DBAPAlgorithm::computeGains` on Quad against the built library:

| distance | az / el (rad) | result |
|---|---|---|
| NaN | 0.3 / 0.2 | 0.5 0.5 0.5 0.5 (equal, as documented) |
| +Inf or -Inf | 0.3 / 0.2 | 0 0 0 0 (silent) |
| +Inf or -Inf | 0.3 / 0 | 0.5 0.5 0.5 0.5 (equal, because `Inf * 0` is NaN) |

The reason is that `srcX/Y/Z` become ±Inf, `distSq` becomes Inf, `std::max (epsilon, Inf)` is Inf,
every weight is 0, `totalWeight` is 0 and the gain loop is skipped. The new test varies only
azimuth and elevation (distance is fixed at 0.5), so the documented distance clause is not pinned
anywhere.

The equal-gains result for NaN is also not designed. It comes from `std::max (epsilon, NaN)`
returning its first argument. IN-01 removed exactly that dependence from ConstantPower and
Ambisonics, and this fix now documents it as a contract for DBAP. `DBAPAlgorithm.cpp` also does not
include `FloatSemanticsGuard.h`, although the guard header says to include it "from every src/ file
that relies on a non-finite test". In practice the other guarded files in the same target would
still trip the `#error`, so the target is protected. The gap is that the file does not declare
the dependency.

The engine path is unaffected because `sanitizeSources` makes the distance finite. The affected
callers are the direct `computeGains` callers the guide addresses (OpenSpatialDelay).
**Fix:** Choose one of two options, and add a distance case to the IN-01 test either way.
1. Make DBAP deterministic. At the top of `computeGains`, after zeroing the output:
   ```cpp
   if (! std::isfinite (source.azimuthRad) || ! std::isfinite (source.elevationRad)
       || ! std::isfinite (source.distance))
   {
       // equal gains (documented) -- or return for silence; pick one and say so
       const float g = 1.0f / std::sqrt (static_cast<float> (numSpeakers));
       for (int s = 0; s < numSpeakers; ++s) outputGains[s] = g;
       return;
   }
   ```
   Add `#include "../Core/FloatSemanticsGuard.h"`, `<cmath>` and `<algorithm>` to the file.
2. Or fix the sentence to what the code does: "DBAP: a NaN direction or distance gives equal gains;
   an infinite distance gives silence". Pin that in the test as well.

## Info

### IN-13: Three files were converted from CRLF to LF wholesale inside content commits, hiding the real edits from review and blame

**File:** `docs/integration-guide.md`, `.claude/skills/spatial-audio-dsp/SKILL.md`,
`include/SpatialCore/IO/AmbisonicsCodec.h`
**Issue:** At `7311e5a` these files had 282, 583 and 43 CRLF lines. At HEAD they have none. The edits
themselves were small: about 45 changed lines in the guide, 6 in SKILL.md, and a few comment lines
and the `bool` return in the header. The line-ending flip made `git diff 7311e5a..HEAD --stat`
report 585, 1,166 and 93 changed lines, and every line in `AmbisonicsCodec.h` shows as modified. The
fix report does not mention it. The repo is mixed (`README.md` and `docs/workflow-tutorials.md`
are still CRLF) and there is no `.gitattributes` rule for line endings, so the change was
incidental to the editing tool, not a policy.
**Fix:** Either restore the original line endings in those three files, or commit the normalisation
on its own, with a `.gitattributes` `eol` rule so it does not flip again. Content commits should
then show only the real edits.

### IN-14: `VBAPAlgorithm`, `VBIPAlgorithm` and `MDAPAlgorithm::computeGains` still `jassert` on the audio thread (the fixer's open question)

**File:** `src/Algorithms/VBAPAlgorithm.cpp:16`, `src/Algorithms/VBIPAlgorithm.cpp:27`,
`src/Algorithms/MDAPAlgorithm.cpp:46`
**Issue:** Decision: this is a finding, at Info level, not Warning. It is not WR-03 again.
- **What it is.** `jassert (ctx.triplets.empty() == ! layoutHasHeight (ctx.layout))` runs on every
  call, and `RenderEngine::computeObjectGains` calls these from `renderBlock`. JUCE's debug
  assertion path logs through `juce::logAssertion`, which allocates. That breaks the project's
  "no malloc, locks or logging in any function called from processBlock" rule, in Debug builds.
- **Why it is not a warning.**
  - Through `RenderEngine` the condition cannot be false. `activateLayout` aborts on the opposite
    mismatch (D-02a). Flat and non-speaker formats get an empty list. Height formats always get a
    non-empty one.
  - WR-03 was reachable from valid input (a partial list). This one needs a hand-built
    `LayoutContext`, which the facade rule (SC-13) says a consumer does not build.
  - The result in the violating case is defined and finite. A height layout with an empty list
    pans by 2D VBAP, and a flat layout with a non-empty list uses 3D VBAP. The assert only adds a
    diagnostic.
  - It is the recorded D-01 decision, and the code comments say so.
- **Why it is still worth reporting.** One consumer in a Debug build that violates the contract
  gets an allocating log line per object per block. The same condition is now checked in three
  places.
**Fix:** Cheapest: delete the three asserts, since the outcome is defined and documented. Or move
the check to `RenderEngine::activateLayout` (message thread), which already guards the same
invariant, and leave the algorithms assert-free.

### IN-15: The WR-05 gap test pins one bridged gap only, so the multi-gap path and the 2(n+2g) formula for g = 2 are unpinned

**File:** `tests/Core/VBAPTripletSelectionTests.cpp:1479-1490` (the `cases[]` table)
**Issue:** All three cases have `bridgedGaps` of 0 or 1. The production code handles several gaps
in one loop (`for p ... gap < kGapBridgeRad`). Two gaps of 179 degrees or more are possible, for
example speakers at 0, 0.5 and 180 degrees. The documented count `2(n + 2g)` is only checked for
g = 0 and g = 1. I verified g = 2 by hand (extras are 14 for n = 3, silent directions are 0, and
power is unit), so the code is correct today. A regression in the loop, such as the second gap
overwriting the first or `q == 0` wrapping wrongly, would pass the current test.

The exactly-180 case does catch a threshold set to 180 degrees, but only through float rounding
of the 90 degree azimuth (the computed gap is about 8.7e-8 rad under pi). Nothing tests the
179 to 180 degree band directly.
**Fix:** Add two cases to `cases[]`:
```cpp
{ "two bridged gaps: 0/0.5/180 plus 2 heights",
  { { 0, 0 }, { 0.5f, 0 }, { 180, 0 }, { 45, 45 }, { -45, 45 } }, 3, 2 },
{ "179.5-degree rear gap, inside the margin",
  { { 0, 0 }, { 90.25f, 0 }, { -90.25f, 0 }, { 45, 45 }, { -45, 45 } }, 3, 1 },
```
The continuity bound of 0.01 does not apply to the first case, because its 0.5 degree pair has a
steep sine-law slope. For that case, check silence, unit power and the count only, or sample the
step at a coarser azimuth increment.

### IN-16: `FloatSemanticsGuard.h` lists "ADM-OSC parse guards" among isfinite tests, but `ADMOSCReceiver.cpp` contains none

**File:** `src/Core/FloatSemanticsGuard.h:11-12`; `src/OSC/ADMOSCReceiver.cpp:2,54-69`
**Issue:** The header comment says the D-06/D-19 guards include "the ADM-OSC parse guards", all
`std::isfinite` or `std::isnan` tests. `ADMOSCReceiver.cpp` has no such test. It produces the NaN
sentinel with the `NAN` macro (`/azim`, `/elev` and `/dist` forward the other two axes as NaN). The
`std::isnan` test lives in the consumer's Listener, which this repo does not contain. The include
is still correct, because `NAN` is meaningless under `-ffinite-math-only`. The comment gives the
wrong reason, which will mislead whoever next decides whether the include can be dropped.
**Fix:** Reword the guard comment to say the receiver emits a NaN sentinel that fast-math would
make undefined, and that the consumer's `isnan` check depends on it.

---

_Reviewed: 2026-10-04_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
