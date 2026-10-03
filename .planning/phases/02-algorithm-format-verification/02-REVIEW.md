---
phase: 02-algorithm-format-verification
reviewed: 2026-10-03T10:45:00Z
depth: standard
files_reviewed: 12
files_reviewed_list:
  - .claude/skills/spatial-audio-dsp/SKILL.md
  - README.md
  - docs/integration-guide.md
  - include/SpatialCore/IO/SpeakerLayout.h
  - src/Core/SpatialMath.cpp
  - src/IO/SpeakerLayout.cpp
  - tests/Algorithms/PanningLawTests.cpp
  - tests/Core/VBAPTripletSelectionTests.cpp
  - tests/IO/SpeakerLayoutTests.cpp
  - tests/reference/EarReference.h
  - tests/reference/README.md
  - tests/reference/gen_ear_reference.py
findings:
  critical: 0
  warning: 3
  info: 2
  total: 5
status: issues_found
---

# Phase 2: Code Review Report (incremental, gap-closure 02-08 / 02-09)

**Reviewed:** 2026-10-03T10:45:00Z
**Depth:** standard
**Files Reviewed:** 12
**Status:** issues_found

## Summary

This is an incremental review of `git diff e5f8e8c..HEAD`: plan 02-08 (pair-pan wedges in
`appendLowerHemisphereTriplets`, three-tier selection in `computeVBAPGains3D`) and plan 02-09 (docs).

The change is sound for every shipped layout. What I verified, rather than read:

- I compiled `src/IO/SpeakerLayout.cpp` and `src/Core/SpatialMath.cpp` against a stubbed `juce_core.h` and
  swept 200,000 random below-horizon directions on each of the 8 shipped height layouts. There were no
  `jassertfalse` hits, no NaN, and output power was 1 everywhere.
- In the -30..0 degree band, the gains equal an independent 2D pair pan of the azimuth-neighbouring
  ear-level speakers. On 5.1.4 they differ only for |az| of 111 degrees or more.
- I ran a 3-degree grid over azimuth -180..177 and elevation -90..-1 on all 8 layouts against PyPI `ear`
  2.1.0 from `.context/venv`. The only differences above 1e-4 were on 5.1.4, for |az| >= 111 degrees at
  elevations -57..-3. That matches the "5.1.4 behind the listener" exception stated in README.md,
  docs/integration-guide.md and SKILL.md, so the doc claims hold.
- Above-horizon selection is unchanged: tier 0 is the old pass 0, iterated in the same order. The new
  per-triplet work (`selectionTier`) uses only integer tests, and there is no allocation or lock on the audio
  path.
- Cap-pair extraction (`pairA` / `pairB`) is correct for each nadir slot position.
- The wedge determinant threshold (`sin(dAz) >= 1e-3`) is looser than the cap's (`0.75 sin(dAz) >= 1e-3`),
  so a wedge is never dropped while its cap survives.

Two earlier findings (WR-02, WR-03) are still present in the changed code and are re-reported below with
updated detail. WR-04 and IN-01..IN-04 are not in the changed hunks and are untouched by this change, so they
are not re-reported here; their dispositions stand.

## Warnings

### WR-02: `LayoutState::vbapTriplets` has three kinds of entry, and the wedge kind is identified by an undocumented implicit sentinel

**File:** `include/SpatialCore/Engine/RenderEngine.h:247`, `include/SpatialCore/IO/SpeakerLayout.h:30-52`,
`src/Core/SpatialMath.cpp:243-256`
**Issue:** The vector now carries regular triplets, nadir-cap triangles and pair-pan wedges. The field in
`RenderEngine.h` still has no comment saying so, and `VBAPTriplet::lowerHemisphere`'s doc ("only consulted
when no ordinary triplet contains the source") now describes only part of the order. A wedge is distinguished
from a cap only by `nadirVertex >= 0 && nadirMask == 0`. That classifier is written out twice, in
`selectionTier` (SpatialMath.cpp:248) and again in the test's `testTier`
(VBAPTripletSelectionTests.cpp:1033). Anything that hand-builds a lower-hemisphere triplet (the header
promises field-by-field construction stays supported, DR-3) with `nadirMask == 0` is silently reclassified
as a wedge and moves to the last tier. Nothing enforces the encoding. If `nadirMask` is ever allowed to be
0 for a cap, selection order changes with no compile or test signal.
**Fix:** Make the kind explicit and keep the existing fields as the data.
```cpp
// SpeakerLayout.h, VBAPTriplet (default 0 keeps field-by-field consumers valid)
enum class Kind : std::uint8_t { regular = 0, nadirCap, pairWedge };
Kind kind = Kind::regular;   // lowerHemisphere == (kind != regular), kept for compatibility
```
Set it in `appendLowerHemisphereTriplets`, switch on it in `selectionTier`, and have the tests read the same
field. At minimum, comment `vbapTriplets` ("regular, then nadir caps, then pair wedges; see
VBAPTriplet") and update the `lowerHemisphere` field doc to name all three tiers.

### WR-03: A regular-only triplet list still has no coverage below the horizon, and the failure is an assert on the audio thread

**File:** `src/Core/SpatialMath.cpp:337-355`, `include/SpatialCore/IO/SpeakerLayout.h:96-97`
**Issue:** Unchanged by 02-08. `buildVBAPTripletsForLayout` alone leaves every below-horizon direction
unenclosed. A consumer that skips `appendLowerHemisphereTriplets` still gets `jassertfalse` on every call in
a Debug build, from `processBlock`, and in Release it gets the max-min-gain fallback clamped to 0, which can
be all-zero. The wedge change widens the gap: the safe set is now "regular + caps + wedges", so a
half-migrated list (for example one that appended only caps) also falls through. The Debug assert on the
audio thread is the permitted diagnostic (D-06c) only when no shipped path can reach it, and the public API
makes it reachable.
**Fix:** Either make the pair of builders one entry point (`buildVBAPTripletsForLayout` calls
`appendLowerHemisphereTriplets` itself, and `RenderEngine::activateLayout` keeps its guard on the regular
count by capturing `size()` first), or document on `computeVBAPGains3D` that the list must come from both
builders and that below-horizon directions assert otherwise.

### WR-05: `appendLowerHemisphereTriplets` leaves holes when the ear-level ring has an azimuth gap of 180 degrees or more, contradicting its documented 2n output

**File:** `src/IO/SpeakerLayout.cpp:293-375`, `include/SpatialCore/IO/SpeakerLayout.h:99-114`
**Issue:** The header says the function appends "2n triplets for n ear-level speakers" and lists only three
no-op conditions (flat, a speaker below -10 degrees, fewer than 3 ear-level speakers). The hull facet test
orients each plane "away from the origin", which is wrong when the ear-level ring does not enclose the
origin. The cap for the wrapping pair is then missing (or its determinant is 0 at exactly 180), and because
wedges are derived only from caps, its wedge is missing too. I reproduced this with the real
`SpeakerLayout.cpp` / `SpatialMath.cpp`, using random below-horizon directions:

| Layout (ear-level + heights) | extras (expected 2n) | non-enclosed directions (of 20000) | silent output |
|---|---|---|---|
| 0, +/-30 + 3 heights (front-only) | 4 (6) | 16661 | 3587 |
| 0, +/-90 + 2 heights (180 gap) | 4 (6) | 9989 | 2549 |
| 0, +/-100 + 2 heights (160 gap) | 6 (6) | 0 | 0 |

Each non-enclosed direction hits `jassertfalse` in Debug, and about 13-18% of the lower sphere is silent in
Release. No shipped layout is affected (all 8 swept clean), but `appendLowerHemisphereTriplets` is a public
function and `SpeakerLayout` is a plain public struct, so a consumer-defined layout can trigger it. The
output is silent rather than wrong, with no way for the caller to tell.
**Fix:** Detect the condition and decline, in the same way as the existing early returns, and document it.
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
Add a test that asserts `extras == 0` or a complete 2n set for such a layout, and amend the header's list of
no-op cases.

## Info

### IN-05: The wedge's "pair pan = EAR QuadRegion" equivalence assumes 0 degree ear-level speakers, but the code admits up to +/-10 degrees

**File:** `src/IO/SpeakerLayout.cpp:196-197` (comment), `:201` (`kEarLevelLimitRad`), `:382-387`
**Issue:** The comment states the equivalence "assumes the ear-level speakers sit at 0 degrees elevation, as
every shipped layout does", but nothing enforces it. `kEarLevelLimitRad` still classifies speakers up to 10
degrees as ear-level, and the wedge vertices use their real elevation. The -30 degree copies sit at an
absolute -30, not relative. For such a layout the below-horizon result is a plain 2D-ish pair pan that no
longer corresponds to the EAR construction the docs describe. This is not a defect for shipped layouts.
**Fix:** Either tighten the ear-level limit to the same ~1 degree `kHeightThresholdRad` used by
`layoutHasHeight`, or state the limitation in the header contract rather than only in a .cpp comment.

### IN-06: The test re-implements the production tier classifier, and one comment in PanningLawTests.cpp is badly reflowed

**File:** `tests/Core/VBAPTripletSelectionTests.cpp:1033-1038`, `tests/Algorithms/PanningLawTests.cpp:943-948`
**Issue:** `testTier` duplicates `selectionTier`. If the production rule changes, the "exactly one wedge
encloses, no cap or regular triplet does" assertion keeps passing against the old rule, so a divergence
produces no failure. In PanningLawTests.cpp the rewritten comment has one ~110-character line followed by
short lines (the "seam between band and cap" sentence), which is cosmetic but visible in the diff.
**Fix:** Expose a shared predicate (this folds into the explicit `Kind` field in WR-02) and use it from both
the engine and the tests. Reflow the comment to the file's wrap width.

---

_Reviewed: 2026-10-03T10:45:00Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
