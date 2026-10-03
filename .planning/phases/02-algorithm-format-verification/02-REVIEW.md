---
phase: 02-algorithm-format-verification
reviewed: 2026-10-04T00:00:00Z
depth: standard
files_reviewed: 10
files_reviewed_list:
  - .gitattributes
  - docs/integration-guide.md
  - include/SpatialCore/Algorithms/DBAPAlgorithm.h
  - src/Algorithms/DBAPAlgorithm.cpp
  - src/Algorithms/MDAPAlgorithm.cpp
  - src/Algorithms/VBAPAlgorithm.cpp
  - src/Algorithms/VBIPAlgorithm.cpp
  - src/Core/FloatSemanticsGuard.h
  - src/OSC/ADMOSCReceiver.cpp
  - tests/Core/VBAPTripletSelectionTests.cpp
findings:
  critical: 0
  warning: 0
  info: 1
  total: 1
status: issues_found
---

# Phase 02: Code Review Report (iteration 3, incremental)

**Reviewed:** 2026-10-04
**Depth:** standard
**Files Reviewed:** 10
**Status:** issues_found

## Summary

Reviewed `git diff 9d4db36..HEAD` over the ten files, plus the surrounding code each change touches. All five prior findings (WR-08, IN-13, IN-14, IN-15, IN-16) are correctly and completely fixed. I found no regressions and no new defect above Info. One comment inaccuracy was introduced by the IN-16 rewrite.

Verification performed:

- **Build and tests.** `cmake --build build --target SpatialCoreTests` is up to date. `build/tests/SpatialCoreTests` gives 200 of 201 test cases and 307,473 of 307,474 assertions passing. The only failure is the known unrelated `HutubsPP2Tests.cpp:47`, plus the known FFT leak-detector line.
- **WR-08 (DBAP).** I read the full `DBAPAlgorithm.cpp`.
  - It is realtime-safe: only stack floats, `std::isfinite`, `std::clamp`, `std::sqrt` and `std::cos`/`std::sin`. There is no allocation, lock or logging. `kNonFiniteDistance` and `kMaxDistance` are `constexpr`.
  - A non-finite azimuth or elevation gives an explicit 1/sqrt(N) on every speaker. This is the same value as the old `std::max (epsilon, NaN)` path, so output for that input is unchanged.
  - Finite distances in [-1000, 1000] are untouched by `std::clamp`, so every normalised 0..1 distance and every moderately larger one gives bit-identical output to before. The clamp changes output only for |d| > 1000.
  - Before the clamp, silence began at roughly |d| of about 1e6 (`totalWeight` under 1e-12) and at about 1e19 (d^2 overflow). The clamp removes it. A clamped value still differs from, say, d = 2000 by only about 0.2% in weight.
  - The behaviour matches the guide text and the `DBAPAlgorithm.h` text exactly: unit power, 1/sqrt(N) for a non-finite direction, 0.5 for a non-finite distance (equal to the `SourcePosition` default at `Types.h:49`), and clamp to ±1000.
  - The engine path is unchanged: `RenderEngine.cpp:100` already makes every field finite before dispatch, and the guide says the same.
  - The new WR-08 test would catch a regression. It compares NaN, +Inf and -Inf distance gains bit-for-bit against the distance-0.5 reference. It requires the 0.5 reference to be non-uniform, so "pans normally" is not vacuous. It checks 1e30, -1e30 and 1e7 for non-silence and unit power. The IN-01 test now pins 1/sqrt(N) per gain, not just equality between gains.
  - `FloatSemanticsGuard.h` is now included in `DBAPAlgorithm.cpp`. The relative path `../Core/` resolves correctly and the file compiles.
- **IN-14 (jassert removal).**
  - The `jassert` is gone from all three files, and nothing else in `src/Algorithms` or `src/Core` asserts.
  - The code path that runs is identical, and the diff touches only the comments. So nothing changes through `RenderEngine`. `activateLayout` still enforces `vbapTriplets.empty() == layoutHasHeight` at `RenderEngine.cpp:729-732` with `std::abort()`.
  - The comment text, "an empty list pans by 2D VBAP, a non-empty list by 3D VBAP", is accurate for all three algorithms. VBIP and MDAP each have the same empty-list branch, including MDAP's auxiliary-source branch.
  - On a bad hand-built context the output is defined and finite. The new test checks VBAP bit-equal to `computeVBAPGains2D`, and VBAP, VBIP and MDAP finite with unit power on 7.1.4 with an empty triplet list. The Debug run prints no VBAP, VBIP or MDAP assertion lines.
- **IN-13 (.gitattributes).**
  - `git ls-files --eol` shows the three pinned files as `i/lf w/lf attr/text eol=lf`. `git check-attr` shows `text: set` and `eol: lf` for exactly those three paths.
  - Patterns with an interior slash are anchored to the repo root, and none contains a wildcard. So no other tracked file is affected. `README.md`, `docs/workflow-tutorials.md` and the other CRLF files still show `attr/` (empty) and `i/crlf w/crlf`. The LFS `.sofa` rule is untouched.
  - `git status --short` shows no tracked file modified. Only the pre-existing untracked `.gsd/`, `.planning/milestone.lock` and `.planning/state.json` appear.
- **IN-15 (test cases).**
  - The two new gap cases are consistent with the formula 2 (n + 2g). `steepFromDeg` and `steepToDeg` default to an empty window, and `inSteepWindow` is guarded by `from < to`, so the other three cases are unchanged.
  - Non-finite steps are still counted as failures, because the window branch sits after the `isfinite` check. A step that straddles the window edge is still bounded by 0.01.
  - The test passes.
- **IN-16 (comments).** The reason given for `ADMOSCReceiver.cpp` is accurate. It has no `isfinite` or `isnan` test, and the `NAN` sentinel at lines ~54-58 is detected by the consumer's Listener with `std::isnan`.

## Info

### IN-17: `FloatSemanticsGuard.h` attributes the ConstantPower, KNN, DirectBinaural and Ambisonics guards to `SpatialMath`

**File:** `src/Core/FloatSemanticsGuard.h:9-12`
**Issue:** The IN-16 rewrite says the guards are "the direction guards in SpatialMath's VBAP, ConstantPower, KNN, DirectBinaural and Ambisonics". `git grep isfinite src` shows that `src/Core/SpatialMath.cpp` holds only the VBAP 2D and 3D guards (lines 141 and 273). The ConstantPower, KNN, DirectBinaural and Ambisonics guards live in their own `src/Algorithms/*Algorithm.cpp` files (for example `KNNAlgorithm.cpp:24`). So the sentence can be read as saying `SpatialMath` contains all five. IN-16 was itself a finding about this comment listing guards that were not where it said they were, so it is worth keeping the list exact. The `#error` mechanism is not affected.
**Fix:** Reword the list so each guard is tied to its file, for example: "the VBAP direction guards in SpatialMath.cpp, the direction guards in the ConstantPower, KNN, DirectBinaural and Ambisonics algorithm files, and DBAP's non-finite direction/distance rule".

---

_Reviewed: 2026-10-04_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
