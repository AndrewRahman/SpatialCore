---
phase: 02-algorithm-format-verification
reviewed: 2026-10-01T07:44:24Z
depth: standard
files_reviewed: 35
files_reviewed_list:
  - .claude/skills/spatial-audio-dsp/SKILL.md
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
findings:
  critical: 0
  warning: 4
  info: 4
  total: 8
status: issues_found
---

# Phase 2: Code Review Report

**Reviewed:** 2026-10-01T07:44:24Z
**Depth:** standard
**Files Reviewed:** 35
**Status:** issues_found

## Summary

I reviewed the Phase 2 diff (`b338fb0^..HEAD`) across the eight changed source files, seven headers, the new and changed tests, the offline reference generators, and the three doc surfaces. I gave the generated `*Reference.h` headers a structural check only.

The core numerical changes hold up:
- **SN3D constants (orders 4-6):** spot-checked. For example, Y4^4 = sqrt(35)/8 matches sqrt(2/8!) * 105.
- **Bounded `std::remainder` wrap:** bit-identical to the old single subtraction up to 3*pi (Sterbenz), as the comment claims.
- **Two-pass triplet search:** leaves the regular pass and its output unchanged above the horizon. The D-06b fallback covers the whole list when both passes miss.
- **Decoder refactor (D-09):** the `memset` plus `getDecodeMatrix` replacement maps exactly onto `ambiDecodeMatrix[s][c]`, and the `static_assert` guards it.
- **Hold-last-good sanitiser:** uses only a member copy of a trivially-copyable struct and fixed arrays, so it does not allocate on the audio thread.

I traced how OpenSpatialDelay (OSD) uses this branch (`/tmp/osd-dr3-check`). OSD builds its `LayoutContext` from `renderEngine.getActiveLayout()`, so it receives the lower-hemisphere triplets. It applies `-ffast-math` PRIVATE to its own targets only, so the `.cpp` guards survive in today's consumer build.

There are no blockers. The findings are about contract and robustness:
- The doc understates the coplanar-tie defect in the new lower-hemisphere band.
- A public triplet list now has mixed semantics, and nothing on the field documents it.
- The standalone builder API gives incomplete coverage below the horizon, and a hand-built context hits a Debug assert on the audio thread.
- The no-fast-math invariant behind every D-06 guard is enforced only by a comment.

Accepted items are not re-reported:
- the HUTUBS PP2 failure
- the JUCE FFT leak false positive
- the deferred skill/README drift
- the `ADMOSCReceiver.h` warnings
- the lower-hemisphere tie itself (recorded in #22)
- the D-02 Release `std::abort`
- the D-06c Debug-only `jassertfalse`

## Narrative Findings (AI reviewer)

## Warnings

### WR-01: Integration guide limits the coplanar-tie gain jumps to "above the horizon", but the new lower-hemisphere band has the same jumps

**File:** `docs/integration-guide.md:211-215` (code: `src/IO/SpeakerLayout.cpp:283-336`, `src/Core/SpatialMath.cpp:279-307`)
**Issue:** `appendLowerHemisphereTriplets` keeps every supporting-plane triangle. Each ear-level/-30 degree trapezoid (A, B, A', B') is a coplanar quad, because B' - A = c(B - A) + (A' - A). So both diagonals are emitted, and the min-sum selection ties exactly, with float rounding deciding the winner. After downmix, the two triangulations give different gains, not just a different index set.

I reproduced this on 7.1.4 with L (30 degrees) and Lss (90 degrees) at az 60, el -15. Triplet (A, B, A') gives 0.747 / 0.665, and triplet (A, B, B') gives the mirror 0.665 / 0.747. Both have gain sum 1.1847054. The phase's own sweep measured 0.104 per 0.1 degree in this band (02-05 SUMMARY), and the test bound was relaxed to 0.12 (`PanningLawTests.cpp:944`).

The defect is accepted and tracked in #22 (issue body line 19). The consumer-facing contract does not say so: the guide says "a small azimuth move **above the horizon** can jump the gains". A consumer reading "From 0 to -30 degrees a source stays on the ear-level speakers its horizon pan uses" will not expect a source descending at a fixed azimuth to lean toward one speaker, with the side chosen by rounding. The guide's own preamble says a doc/code disagreement is a doc defect. The skill step 4 (`SKILL.md:45`) is also scoped only to the regular-triplet case.
**Fix:** Widen the sentence to cover both bands, for example:
```markdown
Where two triangulations of a coplanar quad tie exactly, float rounding decides, so a small
azimuth move can jump the gains -- above the horizon on height layouts, and in the 0 to -30
degree band below it (the ear-level/-30 degree trapezoids are coplanar quads; up to ~0.10 per
0.1 degree measured). Tracked in AndrewRahman/SpatialCore#22 and not fixed.
```
The same clause belongs in the skill's below-horizon paragraph.

### WR-02: `LayoutState::vbapTriplets` now has mixed semantics, and nothing on the field documents it

**File:** `include/SpatialCore/Engine/RenderEngine.h:247`, `src/Engine/RenderEngine.cpp:735`, `src/IO/SpeakerLayout.cpp:248,316-331`
**Issue:** `activateLayout` appends lower-hemisphere triplets to the same public vector that consumers read through `getActiveLayout()`. OSD does this at `PluginProcessor.cpp:2821/2852`. In those entries, `i/j/k` are not a speaker basis:
- Duplicate indices are legal: a virtual -30 vertex carries the index of the speaker above it.
- The nadir slot holds a placeholder (`firstEar`, `SpeakerLayout.cpp:248`) that must be ignored.

Any code that walks the list with v1.0 semantics mis-pans below the horizon. That is the pre-Phase-2 loop of `outGains[tri.i] = g0*scale; ...`, which OSD still carries as its own `computeVBAPGains3D` at `PluginProcessor.cpp:2431-2478`. Such code overwrites duplicate slots instead of accumulating them and routes the whole nadir share to one speaker. OSD's copy is currently unused (DR-3 warnings) and is scheduled for deletion, so nothing breaks today. But the field's type and name are unchanged while its meaning has changed, which goes against the CLAUDE.md backward-compatibility rule. Only the `VBAPTriplet` struct comment warns about it.
**Fix:** Document the contract where consumers will see it. Better still, keep the lists apart so legacy walkers stay correct:
```cpp
struct LayoutState
{
    ...
    // Regular triplets first, then ITU-R BS.2127 lower-hemisphere triplets
    // (lowerHemisphere == true). For the latter, i/j/k may repeat and the
    // nadirVertex slot index is meaningless -- only computeVBAPGains3D may
    // interpret them. Skip entries with lowerHemisphere set if you walk this list.
    std::vector<VBAPTriplet> vbapTriplets;
};
```
Also add a cross-repo follow-up so OSD's dead `computeVBAPGains3D` copy is deleted before anyone revives it against this list.

### WR-03: `buildVBAPTripletsForLayout` alone yields incomplete coverage, and the failure mode is a Debug assert on the audio thread

**File:** `include/SpatialCore/IO/SpeakerLayout.h:84-87`, `src/Core/SpatialMath.cpp:319-324`
**Issue:** `buildVBAPTripletsForLayout` is a public function, and its header comment still describes it as the way to build 3D VBAP triplets. Its result now covers only the upper hemisphere. A `LayoutContext` built from it, rather than from `RenderEngine::getActiveLayout()`, makes `computeVBAPGains3D` miss both passes for every below-horizon direction. Several callers build contexts this way:
- the `makeCtx` helpers in `SpatializationAlgorithmTests.cpp`
- the next consumer, OpenSpatialPanner
- any external code

The consequences:
- **Debug:** `jassertfalse` fires once per source per block on the audio thread, which breaks into the debugger inside a DAW.
- **Release:** output comes from the D-06b largest-min-gain fallback. That is neither the EAR construction nor the v1.0 nearest-speaker behaviour it replaced.

The D-01 comment in `VBAPAlgorithm.cpp:13-16` covers only the empty-triplets case, not this partial-triplets case. `appendLowerHemisphereTriplets` is documented as "deliberately separate", but nothing on the builder tells a caller that it must also call the append.
**Fix:** At minimum, update the builder's docblock:
```cpp
// Builds the REGULAR (upper-hemisphere) triplets only. A LayoutContext used for
// VBAP/VBIP/MDAP must also call appendLowerHemisphereTriplets(layout, triplets)
// afterwards, or every below-horizon source takes the D-06b fallback (and fires a
// Debug jassert on the audio thread). RenderEngine::getActiveLayout() already does both.
```
Better, add a single entry point such as `buildAllVBAPTripletsForLayout(layout, out)` (build + append) and point consumers and test helpers at it.

### WR-04: The "SpatialCore must not be compiled with fast-math" invariant is enforced only by comments

**File:** `src/Core/SpatialMath.cpp:139,257`, `src/Engine/RenderEngine.cpp:99`, `src/Algorithms/KNNAlgorithm.cpp:23`, `src/Algorithms/DirectBinauralAlgorithm.cpp:25`, `docs/integration-guide.md:227-228`
**Issue:** Every D-06/D-19 guard is a `std::isfinite` check. Each one is correct only if its translation unit is compiled without `-ffinite-math-only`. The guide states this as a rule, and `CMakeLists.txt:164-179` explicitly anticipates "a future plan needs -ffast-math perf back on SpatialCore". Nothing fails the build if that happens. A consumer could add `-ffast-math` to `CMAKE_CXX_FLAGS`, or call `add_compile_options(-ffast-math)` before `add_subdirectory(SpatialCore)`. Either would compile `SpatialCore` with fast-math and silently fold every guard to "finite":
- the sanitiser stops holding
- KNN and 3D VBAP emit NaN gains
- DirectBinaural emits NaN delays

The bounded `std::remainder` keeps 2D VBAP from hanging, but NaN still reaches the output buffers. This is the exact failure Phase 2 set out to remove, and it would come back with no diagnostic.
**Fix:** Turn the invariant into a compile error in each guarded TU, or once in a small private header they all include:
```cpp
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__)
 #error "SpatialCore's non-finite guards (D-06/D-19) require IEEE semantics; do not compile SpatialCore with -ffast-math / -ffinite-math-only"
#endif
```
For MSVC `/fp:fast`, a CMake check on the `SpatialCore` target's effective options can do the same job.

## Info

### IN-01: The guide's "return silence" list leaves out ConstantPower and Ambisonics, which are silent only because of argument order

**File:** `docs/integration-guide.md:224-225`, `src/Algorithms/ConstantPowerAlgorithm.cpp:30-31`, `src/Algorithms/AmbisonicsAlgorithm.cpp:40`
**Issue:** The guide names VBAP, VBIP, MDAP, KNN and DirectBinaural as silent for a non-finite direction, which implies the rest are not. The 02-07 SUMMARY key-decision says so directly: "DBAP, ConstantPower and Ambisonics return finite non-silent gains". In fact ConstantPower and Ambisonics are also silent:
- `juce::jlimit(-1, 1, NaN)` returns NaN.
- `std::max(0.0f, NaN)` returns its first argument, 0.

Only DBAP is non-silent: `std::max(epsilon, NaN)` returns epsilon, which gives equal gains. This silence has no explicit guard. It depends on the argument order of `std::max`, and rewriting it as `std::max(gain, 0.0f)` would propagate NaN.
**Fix:** Correct the guide's list to "everything except DBAP". Optionally add explicit `isfinite` early-returns in those two `.cpp` files so the behaviour does not depend on `std::max` argument order.

### IN-02: "Positive azimuth toward +Y (left)" uses AmbiX axis names, which contradict SpatialCore's own Cartesian frame

**File:** `include/SpatialCore/Core/SpatialMath.h:81`, `include/SpatialCore/IO/AmbisonicsCodec.h:12`, `docs/integration-guide.md` (Ambisonics channel convention)
**Issue:** Throughout SpatialCore, x = cos(el)sin(az) and y = cos(el)cos(az) is front: see `computeVBAPGains3D`, `makeHullVertex`, and `layouts_from_cpp.vec`. So in SpatialCore's own frame, positive azimuth moves toward **+x**. The new convention docblocks say "toward +Y", which is true only in AmbiX axis naming (X front, Y left). A reader comparing with the panning code will see a contradiction.
**Fix:** Say "positive azimuth toward the listener's left (AmbiX +Y; SpatialCore's internal +x)".

### IN-03: `getDecodeMatrix` fails silently with no way for the caller to detect it

**File:** `src/IO/AmbisonicsCodec.cpp:49-55`, `include/SpatialCore/IO/AmbisonicsCodec.h:27-30`
**Issue:** If `numSpeakers > MAX_SPEAKERS` or the order is above 6, the function returns without writing, and its return type is `void`. `activateLayout` defends against this with a prior `memset`. Any other public caller with an uninitialised or reused buffer decodes with garbage or stale data and gets no signal.
**Fix:** Return `bool`, which is a minor-version API addition. Alternatively, on rejection zero the `numSpeakers * M` floats the contract says the caller supplied, so a rejected call is at least deterministic silence.

### IN-04: Hold-last-good is keyed by slot and never reset when a slot is reused, which the guide's "per object" wording does not reflect

**File:** `src/Engine/RenderEngine.cpp:91-114`, `docs/integration-guide.md:219-221`
**Issue:** The guide says a field "that has never been finite renders as azimuth 0, elevation 0, distance 0.5". The state is per slot and is reset only in the constructor and in `prepare()`. When a slot goes non-live and is later reused for a different object whose first update is a partial ADM-OSC message (NaN fields), that object renders at the previous occupant's last position, not at the defaults.
**Fix:** Reset a slot's held values when `objectLive[t]` goes from false to true, or reword the guide to "per object slot since the last `prepare()`".

---

_Reviewed: 2026-10-01T07:44:24Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
