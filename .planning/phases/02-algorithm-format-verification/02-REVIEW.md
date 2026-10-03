---
phase: 02-algorithm-format-verification
reviewed: 2026-10-04T00:00:00Z
depth: standard
files_reviewed: 36
files_reviewed_list:
  - README.md
  - docs/integration-guide.md
  - include/SpatialCore/Algorithms/DBAPAlgorithm.h
  - include/SpatialCore/Algorithms/MDAPAlgorithm.h
  - include/SpatialCore/Algorithms/VBIPAlgorithm.h
  - include/SpatialCore/Core/SpatialMath.h
  - include/SpatialCore/Engine/RenderEngine.h
  - include/SpatialCore/IO/AmbisonicsCodec.h
  - include/SpatialCore/IO/SpeakerLayout.h
  - src/Algorithms/DirectBinauralAlgorithm.cpp
  - src/Algorithms/KNNAlgorithm.cpp
  - src/Algorithms/MDAPAlgorithm.cpp
  - src/Algorithms/VBAPAlgorithm.cpp
  - src/Algorithms/VBIPAlgorithm.cpp
  - src/Core/SpatialMath.cpp
  - src/Engine/RenderEngine.cpp
  - src/IO/AmbisonicsCodec.cpp
  - src/IO/SpeakerLayout.cpp
  - tests/Algorithms/PanningLawTests.cpp
  - tests/Algorithms/SpatializationAlgorithmTests.cpp
  - tests/CMakeLists.txt
  - tests/Core/VBAPTripletSelectionTests.cpp
  - tests/Engine/RenderEngineTests.cpp
  - tests/IO/AmbisonicsCodecTests.cpp
  - tests/IO/SpeakerLayoutTests.cpp
  - tests/reference/.gitignore
  - tests/reference/EarReference.h
  - tests/reference/PanningReference.h
  - tests/reference/README.md
  - tests/reference/ShReference.h
  - tests/reference/gen_ear_reference.py
  - tests/reference/gen_panning_reference.py
  - tests/reference/gen_sh_reference.py
  - tests/reference/layouts_from_cpp.py
  - .claude/skills/spatial-audio-dsp/SKILL.md
  - .gitignore
findings:
  critical: 0
  warning: 5
  info: 10
  total: 15
status: issues_found
---

# Phase 02: Code Review Report

**Reviewed:** 2026-10-04
**Depth:** standard (full-phase re-review, `git diff b338fb0^..HEAD`)
**Files Reviewed:** 36
**Status:** issues_found

## Summary

I reviewed all of the phase's production code (VBAP 2D/3D with the EAR lower-hemisphere triplets, VBIP, MDAP, KNN, DirectBinaural guards, the hold-last-good sanitiser, `getDecodeMatrix`, `activateLayout`), the test suites and the offline reference generators.

**Build and run results (Debug and Release, macOS arm64):**
- Debug: 193 of 194 test cases pass. The one failure is `HutubsPP2Tests.cpp:47`, which is known and unrelated.
- Release (`build-release`): all 194 test cases pass, 304,899 assertions.
- The three `tests/reference/*.h` headers regenerate byte-identically from the generators in `.context/venv`, so they are current.

**Overall:** no critical finding. I found no realtime-safety violation, no UB and no wrong audio output on a shipped layout. Production code under `src/` and `include/` is unchanged since the previous full review (`f68fa3a`), so every still-open production finding still holds. The one new defect class is in the tests: the max-reduction that silently drops NaN (WR-06, fixed only in `RenderEngineTests.cpp`) is still present in `AmbisonicsCodecTests.cpp`.

**Prior findings:**
- **Still hold (13), re-reported under the same IDs and titles:** WR-02, WR-03, WR-04, WR-05, IN-01, IN-02, IN-03, IN-04, IN-05, IN-06, IN-07, IN-08, IN-09. IN-08 is now narrower, because part (b) is resolved (see IN-08).
- **No longer hold:** none outright. IN-08 (b) is the only part that no longer applies.
- **Fixed and not regressed:**
  - WR-01: the guide and skill now scope the #22 coplanar-tie jumps to above the horizon.
  - WR-06: `accumulateWorstFinite` rejects NaN and Inf.
  - IN-10: non-finite entries are counted per layout with the first location named.
  - IN-11: the negative test `RenderEngine: the ambi-pin max-reduction rejects non-finite distances…` exists and passes.

**New findings:** WR-07 (warning) and IN-12 (info).

## Warnings

### WR-02: `LayoutState::vbapTriplets` has three kinds of entry, and the wedge kind is identified by an undocumented implicit sentinel

**File:** `include/SpatialCore/Engine/RenderEngine.h:247`, `include/SpatialCore/IO/SpeakerLayout.h:34-43`, `src/Core/SpatialMath.cpp:243-255`, `tests/Core/VBAPTripletSelectionTests.cpp:1032-1038`, `tests/IO/SpeakerLayoutTests.cpp:376`
**Issue:** The vector holds regular triplets, nadir-cap triangles and pair-pan wedges. Three things are still true:
- `std::vector<VBAPTriplet> vbapTriplets;` in `RenderEngine.h:247` still has no comment.
- The `lowerHemisphere` field doc in `SpeakerLayout.h:42` still reads "only consulted when no ordinary triplet contains the source". That is now one of three tiers.
- A wedge is identified only by `nadirVertex >= 0 && nadirMask == 0`. The rule is written in three places: `selectionTier` (`SpatialMath.cpp:252`), the test's `testTier` (`VBAPTripletSelectionTests.cpp:1037`) and the test at `SpeakerLayoutTests.cpp:376` (`nadirMask != 0` means cap).

Anything that hand-builds a lower-hemisphere triplet with `nadirMask == 0` (the header promises field-by-field construction stays supported, DR-3) is silently reclassified as a wedge and moves to the last tier. Nothing enforces the encoding. If `nadirMask` is ever allowed to be 0 for a cap, selection order changes with no compile or test signal.
**Fix:** Make the kind explicit and keep the existing fields as data.
```cpp
// SpeakerLayout.h, VBAPTriplet (default 0 keeps field-by-field consumers valid)
enum class Kind : std::uint8_t { regular = 0, nadirCap, pairWedge };
Kind kind = Kind::regular;   // lowerHemisphere == (kind != regular), kept for compatibility
```
Set it in `appendLowerHemisphereTriplets`, switch on it in `selectionTier`, and have the tests read the same field. At minimum, comment `vbapTriplets` ("regular, then nadir caps, then pair wedges; see VBAPTriplet") and update the `lowerHemisphere` doc to name all three tiers.

### WR-03: A regular-only triplet list still has no coverage below the horizon, and the failure is an assert on the audio thread

**File:** `src/Core/SpatialMath.cpp:337-355`, `include/SpatialCore/IO/SpeakerLayout.h:94-97` and `:111-113`, `include/SpatialCore/Core/SpatialMath.h:91-92`
**Issue:** `buildVBAPTripletsForLayout` alone leaves every below-horizon direction unenclosed. A consumer that skips `appendLowerHemisphereTriplets`, or appends only part of the set, gets `jassertfalse` on every call in a Debug build, from `processBlock`. In Release it gets the max-min-gain fallback clamped to 0, which can be all-zero.

The header explains why the two builders are separate (`SpeakerLayout.h:111-113`). `computeVBAPGains3D`'s declaration (`SpatialMath.h:91-92`) says nothing about the requirement. The test at `VBAPTripletSelectionTests.cpp:744-800` deliberately drives exactly this path, which proves the public API makes it reachable. The Debug assert on the audio thread is the permitted diagnostic (D-06c) only when no shipped path can reach it.
**Fix:** Either make one entry point (`buildVBAPTripletsForLayout` calls `appendLowerHemisphereTriplets` itself, and `activateLayout` captures the regular count first for its D-02a guard), or document on `computeVBAPGains3D` that the list must come from both builders and that below-horizon directions assert otherwise.

### WR-04: The "SpatialCore must not be compiled with fast-math" invariant is enforced only by comments

**File:** `src/Core/SpatialMath.cpp:139` and `:272`, `src/Engine/RenderEngine.cpp:99`, `src/Algorithms/KNNAlgorithm.cpp:23`, `src/Algorithms/DirectBinauralAlgorithm.cpp:25`, `docs/integration-guide.md:239-240`, `CMakeLists.txt:164-179`
**Issue:** Every D-06/D-19 guard is a `std::isfinite` check, correct only if its translation unit is compiled without `-ffinite-math-only`. `CMakeLists.txt` anticipates a future change that wants `-ffast-math` back, and a consumer can add it through `CMAKE_CXX_FLAGS` or `add_compile_options` before `add_subdirectory(SpatialCore)`. A repo grep finds no `__FAST_MATH__` or `__FINITE_MATH_ONLY__` check anywhere. If it happens, every guard folds to "finite" and:
- the sanitiser stops holding;
- KNN and 3D VBAP emit NaN gains;
- DirectBinaural emits NaN delays.

This is the exact failure Phase 2 removed, and it would return with no diagnostic.
**Fix:** Turn the invariant into a compile error in each guarded TU, or once in a small private header they all include:
```cpp
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__)
 #error "SpatialCore's non-finite guards (D-06/D-19) require IEEE semantics; do not compile SpatialCore with -ffast-math / -ffinite-math-only"
#endif
```
For MSVC `/fp:fast`, a CMake check on the `SpatialCore` target's effective options can do the same job.

### WR-05: `appendLowerHemisphereTriplets` leaves holes when the ear-level ring has an azimuth gap of 180 degrees or more, contradicting its documented 2n output

**File:** `src/IO/SpeakerLayout.cpp:238-375` (orientation test at `:320-329`, no gap check after `:273`), `include/SpatialCore/IO/SpeakerLayout.h:99-114`
**Issue:** The header says the function appends "2n triplets for n ear-level speakers" and lists three no-op cases: flat, a speaker below -10 degrees, and fewer than 3 ear-level speakers. The hull-facet test orients each plane "away from the origin", which is wrong when the ear-level ring does not enclose the origin. The cap for the wrapping pair is then missing, and since wedges derive only from caps, so is its wedge.

I re-ran this today against the real `SpeakerLayout.cpp` and `SpatialMath.cpp` with 5,000 random below-horizon directions:

| Layout (ear-level, plus heights) | extras (expected 2n) | silent directions (power < 0.5) |
|---|---|---|
| 0, +/-30 plus 3 heights | 4 (6) | 784 (15.7%) |
| 0, +/-90 plus 2 heights | 4 (6) | 633 (12.7%) |
| 0, +/-100 plus 2 heights | 6 (6) | 0 |

Each non-enclosed direction hits `jassertfalse` in Debug. No shipped layout is affected. `SpeakerLayout` is a plain public struct and the function is public, so a consumer-defined layout can trigger it, and the result is silence rather than an error.
**Fix:** Detect the condition and decline, like the existing early returns, and document it.
```cpp
// after the ear-level vertices are collected, before building the hull
std::vector<double> azs;                     // wrapped to [0, 2*pi)
for (const auto& v : verts) azs.push_back(wrapTwoPi(std::atan2(v.x, v.y)));
std::sort(azs.begin(), azs.end());
double maxGap = azs.front() + 2.0 * M_PI - azs.back();
for (size_t i = 1; i < azs.size(); ++i) maxGap = std::max(maxGap, azs[i] - azs[i - 1]);
if (maxGap >= M_PI - 1e-3)                   // ring does not surround the listener
    return;
```
Add a test asserting `extras == 0` or a complete 2n set for such a layout, and amend the header's list of no-op cases.

### WR-07: The NaN-dropping `std::max` reduction fixed by WR-06 is still present in the Ambisonics codec tests

**File:** `tests/IO/AmbisonicsCodecTests.cpp:215-217` and `:220`, `:233-238`, `:441-445`
**Issue:** WR-06 was fixed only in `RenderEngineTests.cpp` (`accumulateWorstFinite`). Three other tests in this file use the same pattern. A non-finite result never raises `worst`, so the final `CHECK (worst <= bound)` passes vacuously:
- `:215-217`: `dev > worst` is false for NaN, so `worst` stays 0. This is the SN3D addition-theorem test. A NaN `sum` is silently skipped.
- `:235-236`: `std::max (worst, std::abs (NaN))` returns `worst`, because libc++ and libstdc++ define `max(a, b)` as `(a < b) ? b : a`. This is the D-05 parity test.
- `:441`: the same `std::max` in the encode, decode, re-encode round trip. `denseDecode` (`:367-372`) divides by `diag` with no zero guard, so a singular or garbage `E` yields NaN everywhere. The test then reports `worst == 0` and passes. This is the one with real exposure.

The `[golden]` test checks finiteness only at one direction (az 64, el 10), so it is not a backstop for the 2000 and 200 random directions used here.
**Fix:** Count non-finite values and assert on the count, as `RenderEngineTests.cpp:717-723` does. Move `accumulateWorstFinite` into a shared test header and use it here:
```cpp
if (! std::isfinite (dev)) { ++nonFinite; continue; }
...
CHECK (nonFinite == 0);
```
The same shape appears in `PanningLawTests.cpp:354-358` (`maxStepOverSweep`) and `VBAPTripletSelectionTests.cpp:1196-1198`. There other tests (the power sweep and the coverage test) catch NaN, so those are lower priority.

## Info

### IN-01: The guide's "return silence" list leaves out ConstantPower and Ambisonics, which are silent only because of argument order

**File:** `docs/integration-guide.md:236`, `src/Algorithms/ConstantPowerAlgorithm.cpp:30-31`, `src/Algorithms/AmbisonicsAlgorithm.cpp:40`
**Issue:** The guide names VBAP, VBIP, MDAP, KNN and DirectBinaural as silent for a non-finite direction. ConstantPower and Ambisonics are also silent:
- `juce::jlimit (-1, 1, NaN)` returns NaN.
- `std::max (0.0f, NaN)` returns its first argument, 0.

Only DBAP is non-silent: `std::max (epsilon, NaN)` returns `epsilon`, which gives equal gains. The silence has no explicit guard. Rewriting `std::max (0.0f, x)` as `std::max (x, 0.0f)` would propagate NaN.
**Fix:** Correct the guide's list to "everything except DBAP". Optionally add explicit `isfinite` early-returns in those two files so the behaviour does not depend on argument order.

### IN-02: "Positive azimuth toward +Y (left)" uses AmbiX axis names, which contradict SpatialCore's own Cartesian frame

**File:** `include/SpatialCore/Core/SpatialMath.h:81`, `include/SpatialCore/IO/AmbisonicsCodec.h:14`, `docs/integration-guide.md:189`, `src/Core/SpatialMath.cpp:13`
**Issue:** In SpatialCore's own frame x = cos(el) sin(az) and y = cos(el) cos(az) is front (`computeVBAPGains3D`, `makeHullVertex`, `layouts_from_cpp.vec`). Positive azimuth therefore moves toward +x. The docblocks say "toward +Y", which is true only in AmbiX axis naming (X front, Y left).
**Fix:** Say "positive azimuth toward the listener's left (AmbiX +Y; SpatialCore's internal +x)".

### IN-03: `getDecodeMatrix` fails silently with no way for the caller to detect it

**File:** `src/IO/AmbisonicsCodec.cpp:52` and `:55`, `include/SpatialCore/IO/AmbisonicsCodec.h:27-31`
**Issue:** If `numSpeakers > MAX_SPEAKERS` or the order exceeds 6, the function returns without writing, and its return type is `void`. `activateLayout` defends against this with a prior `memset`. Any other caller with an uninitialised or reused buffer decodes stale data and gets no signal. A negative `order` below -1 also yields a positive `M`, because `(order + 1)^2` is positive, so it computes a decode instead of rejecting.
**Fix:** Return `bool` (a minor-version addition), reject `order < 0`, or zero the `numSpeakers * M` floats on rejection so the result is deterministic silence.

### IN-04: Hold-last-good is keyed by slot and never reset when a slot is reused, which the guide's "per object" wording does not reflect

**File:** `src/Engine/RenderEngine.cpp:77-114`, `docs/integration-guide.md:231-233`
**Issue:** The guide says a field "that has never been finite renders as azimuth 0, elevation 0, distance 0.5". State is per slot and reset only in the constructor and `prepare()`. The hold also runs for non-live slots, so garbage on an unused slot becomes that slot's "last good" value. When a slot is reused for a different object whose first update is a partial ADM-OSC message (NaN fields), that object renders at the previous occupant's last position, not at the defaults.
**Fix:** Reset a slot's held values when `objectLive[t]` goes false to true (and skip the hold update while not live), or reword the guide to "per object slot since the last `prepare()`".

### IN-05: The wedge's "pair pan = EAR QuadRegion" equivalence assumes 0 degree ear-level speakers, but the code admits up to +/-10 degrees

**File:** `src/IO/SpeakerLayout.cpp:196-197` (comment), `:201` and `:249` and `:263` (`kEarLevelLimitRad`), `:382-387`, `include/SpatialCore/IO/SpeakerLayout.h:99-109`
**Issue:** The comment states the equivalence "assumes the ear-level speakers sit at 0 degrees elevation, as every shipped layout does". Nothing enforces it. `kEarLevelLimitRad` still classifies speakers up to 10 degrees as ear-level, and the wedge vertices use their real elevation. The -30 degree copies sit at an absolute -30, not relative. For such a layout the below-horizon result is a 2D-ish pair pan that no longer corresponds to the EAR construction the docs describe. This is not a defect for shipped layouts, and the header contract (`:99-109`) does not state the limitation.
**Fix:** Tighten the ear-level limit to the ~1 degree `kHeightThresholdRad`, or state the limitation in the header contract rather than only in a `.cpp` comment.

### IN-06: The test re-implements the production tier classifier, and one comment in PanningLawTests.cpp is badly reflowed

**File:** `tests/Core/VBAPTripletSelectionTests.cpp:1032-1038`, `tests/IO/SpeakerLayoutTests.cpp:376`, `tests/Algorithms/PanningLawTests.cpp:943-951`
**Issue:** `testTier` duplicates `selectionTier`, and `SpeakerLayoutTests.cpp` classifies a cap by `nadirMask != 0` independently. If the production rule changes, the "exactly one wedge encloses, no cap or regular triplet does" assertion keeps passing against the old rule. In `PanningLawTests.cpp` the rewritten comment has one ~112-character line (`:947`) followed by short lines, which is cosmetic but visible in the diff.
**Fix:** Expose a shared predicate (this folds into the explicit `Kind` in WR-02) and use it from the engine and the tests. Reflow the comment to the file's wrap width.

### IN-07: The +0.1% epsilon "smallest change the pin must catch" is resolved on only 5 of 15 layouts

**File:** `tests/Engine/RenderEngineTests.cpp:692-696`, `:708`
**Issue:** The comment states the +0.1% Tikhonov change moves the decode by 5.8e-5 to 6.3e-5 on 7.0, 7.1, 7.1.2, 7.1.4 and 7.1.6. The bound is one absolute number, 2.5e-5, applied to every layout. On the other 10 layouts the same change moves the decode by less than that, so it is not caught there. The pin as a whole still fails for a +0.1% change (02-10-SUMMARY records this), so the claim is true overall. But the comment does not say the guarantee rests on 5 layouts, or that a regression confined to the other layouts has a coarser floor.
**Fix:** Add one sentence saying the +0.1% resolution holds on the 7.x layouts only and that other layouts rely on the O(0.1 to 1) class of regressions. A scale-aware per-layout bound is the stronger option.

### IN-08: `doublePrecisionAmbiDecode` drops the library's guards and shares float E with the other two decoders

**File:** `tests/Engine/RenderEngineTests.cpp:729-796`
**Issue:** Part (a) still holds. The double solve omits the library's `if (std::abs (diagVal) < 1e-10f) continue;` skip and has no `N <= M` guard. `E[M][M]` indexed by `s < N` would overflow if `N > 16`. This cannot happen with the current layouts (maximum 15), but it is an unguarded stack write in a test helper, and a zero diagonal would give NaN.

Part (b) is resolved. The `evalSH` docblock now names where SH correctness is pinned (`SpatialMath.h:84-87`, the 49 scipy values in `AmbisonicsCodecTests.cpp` `[sn3d]`). The shared-E design is therefore documented, not an unexplained gap.
**Fix:** Add `REQUIRE (N <= M)` at the top of the helper.

### IN-09: Comment hygiene

**File:** `tests/Engine/RenderEngineTests.cpp:673-707`
**Issue:** Three small problems remain in the derivation comment:
- It cites the debug-doc path twice (`:674` and `:705-707`), with a duplicated "full derivation" sentence.
- It says the bound is "2.5x below the +0.1% change" (`:698-699`). The ratio is 5.8e-5 / 2.5e-5, about 2.3x, which is what 02-10-SUMMARY says.
- `referenceAmbiDecode` (`:576`) is intentionally unindented so it diffs against the blob, while the helper block after it (`:662` on) is indented in the same anonymous namespace. A note at the `namespace` line would stop someone "fixing" the indentation and breaking the byte-for-byte diff.

**Fix:** Drop the duplicate paragraph, change "2.5x" to "2.3x", and note the intentional indentation split.

### IN-12: Ambisonics may be silent everywhere on height layouts and still pass every panning-law check

**File:** `tests/Algorithms/PanningLawTests.cpp:84`, `:105`, `:520-525`, `:470-473`
**Issue:** For Ambisonics, `mayBeSilentOffFlatHorizon` makes exact silence lawful on every direction of every height layout (the exemption applies when `! flatHorizon`, which is always true on height rigs). The other Ambisonics laws are vacuous too:
- on-speaker is `FiniteOnly`;
- mirror and continuity pass on all-zero gains.

A regression that left the engine's order-3 decode matrix unfilled for height layouts would pass this whole file. The `[ambi-pin]` test in `RenderEngineTests.cpp` is the only backstop. It is a different test file, so the panning-law suite alone gives no signal.
**Fix:** In the power sweep, count non-silent directions per rig and require a floor (for example, at least 50% of the sampled directions at unit power for Ambisonics on every layout). Keep the exemption only for the below-horizon samples it is meant for.

---

_Reviewed: 2026-10-04_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
