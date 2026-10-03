---
phase: 02-algorithm-format-verification
fixed_at: 2026-10-03T23:20:43Z
review_path: .planning/phases/02-algorithm-format-verification/02-REVIEW.md
iteration: 1
findings_in_scope: 15
fixed: 15
skipped: 0
status: all_fixed
---

# Phase 02: Code Review Fix Report

**Fixed at:** 2026-10-03T23:20:43Z
**Source review:** .planning/phases/02-algorithm-format-verification/02-REVIEW.md
**Iteration:** 1 (third review/fix round of the phase)

**Summary:**
- Findings in scope: 15 (WR-02, WR-03, WR-04, WR-05, WR-07, IN-01 to IN-09, IN-12)
- Fixed: 15
- Skipped: 0

Warnings were fixed first, in the order WR-05, WR-03, WR-02, WR-04, WR-07. The info items followed. There are 17 commits (`3a2a1b2` to `8afcc75`). WR-05 has two follow-up docs commits.

**Where the work and the checks ran:** in the main checkout of this conductor workspace (`kelowna`, branch `gsd-remap`), not in a separate worktree. The workspace is already an isolated git worktree. The environment says not to leave it, and the `build/` and `build-release/` trees that the checks require live here. Every number below can be reproduced from this tree.

## Verification

| Check | Result |
|---|---|
| Debug: `cmake --build build --target SpatialCoreTests -j8`, then `build/tests/SpatialCoreTests` | 198 of 199 test cases pass, 306,694 of 306,695 assertions. The only failure is the known `HutubsPP2Tests.cpp:47` checksum. |
| Release: `cmake --build build-release --target SpatialCoreTests -j8`, then the full suite | All 199 test cases pass, 306,695 assertions. |
| Reference headers regenerated with `.context/venv` | `ShReference.h`, `EarReference.h` and `PanningReference.h` all regenerate byte-identically. `ShReference.h` was regenerated in IN-02 because its generator's comment changed. The only diff is that comment line, and every value is unchanged. |
| Shipped-layout triplets, before and after WR-05 | All 15 shipped layouts give bit-identical triplet lists: every field and every float bit (`cmp` of a dump). No EAR pin moved. |
| Debug assertion lines | `SpatialMath.cpp` no longer asserts anywhere in the suite. The remaining lines come from earlier tests: RenderEngine's oversized-block guard, the OSC MessageListener and the FFT leak detector. |

Before the fixes there were 195 test cases, 194 passing. The 4 extra test cases are the new tests for WR-03, IN-01, IN-03 and IN-04. The WR-05 test was already included in that count of 195.

**Mutation checks.** Each mutation was a throwaway edit, reverted and never committed. In each case the guard failed when the defect was put back:

| Guard | Mutation | Result |
|---|---|---|
| WR-05 `[ear][gap]` | Gap bridge disabled (`if (true \|\| gap < kGapBridgeRad)`) | 11 assertions fail: 4 extras instead of 10, 2,428 and 1,649 silent directions, uncovered directions, gain step of 1.0 |
| WR-03 `[robust]` | Nearest-speaker branch disabled | 16 assertions fail |
| WR-07 | Zero pivot in `denseDecode`, plus a NaN SH sum and a NaN parity value | Old `AmbisonicsCodecTests` passes all 4 tests (vacuous). The new code fails 3 tests (61 `nonFinite == 0` checks). A NaN injected into the sweep and band gains fails 242 sweep checks and 8 band checks. |
| IN-12 | Order-3 decode skipped for height layouts in `activateLayout` | Old `PanningLawTests` passes all 10 test cases (vacuous). The new code fails 4 test cases (power, on-speaker, mirror, continuity) on all 8 height rigs. |
| WR-04 | Each of the 5 guarded sources compiled with `-ffast-math` and with `-ffinite-math-only` | The `#error` fires in every case. With normal flags, all compile cleanly. A `-ffast-math` consumer file that includes the public headers still compiles. |

## Fixed Issues

### WR-05: `appendLowerHemisphereTriplets` leaves holes when the ear-level ring has an azimuth gap of 180 degrees or more

**Files modified:** `src/IO/SpeakerLayout.cpp`, `include/SpatialCore/IO/SpeakerLayout.h`, `tests/Core/VBAPTripletSelectionTests.cpp`, `docs/integration-guide.md`, `.claude/skills/spatial-audio-dsp/SKILL.md`
**Commits:** 3a2a1b2 (fix and test), 891a6b8 (docs), 8afcc75 (exact threshold in the docs)
**Status:** fixed. The behaviour inside the gap is a design choice, so it needs a human to confirm it.
**Applied fix:**
- **Why not the review's fix.** The review suggested declining, i.e. appending nothing for such a layout. Those directions would then stay silent, and the brief requires them to make sound. One triangle also cannot cover a gap of 180 degrees or more. So instead, each gap of 179 degrees or more gets two virtual ear-level vertices at one third and two thirds of the way across. They downmix 1:1 onto the gap's two edge speakers, the same way the -30 degree copies already downmix.
- **Result.** The ring surrounds the listener again, and the existing hull builds its caps and wedges. Inside the gap, the first third stays on one edge speaker, the middle third pans between the two, and the last third stays on the other. VBAPTriplet gets no new fields, and the audio path is unchanged.
- **Why 179 degrees.** From about 179.93 degrees the wrapping cap's determinant is already below the hull epsilon, so the bridge starts 1 degree short of 180.
- **Triplet count.** The documented count is now 2(n + 2g) for g bridged gaps. It is 2n on every shipped layout, because their widest gap is 140 degrees.

**Reproduction.** The new `[ear][gap]` test samples 5,000 random plus 8,100 grid below-horizon directions.

| Layout | Extras before | Silent before | Extras after | Silent after |
|---|---|---|---|---|
| Front-only 0/±30 ring | 4 | 2,428 | 10 | 0 |
| 0/±90 ring | 4 | 1,649 | 10 | 0 |
| 160-degree control | 6 | 0 | 6 | 0 |

After the fix the test also finds 0 uncovered directions, unit power everywhere and no gain on elevated speakers. The largest gain step between neighbouring 0.1-degree samples is 0.0053.

### WR-03: A regular-only triplet list has no coverage below the horizon, and the failure was an assert on the audio thread

**Files modified:** `src/Core/SpatialMath.cpp`, `include/SpatialCore/Core/SpatialMath.h`, `include/SpatialCore/IO/SpeakerLayout.h`, `tests/Core/VBAPTripletSelectionTests.cpp`, `docs/integration-guide.md`, `.claude/skills/spatial-audio-dsp/SKILL.md`
**Commit:** 8899bcf
**Status:** fixed. This changes behaviour, so it needs a human to confirm it.
**Applied fix:**
- **Assert removed.** The audio-thread `jassertfalse` is gone. JUCE's assert path logs, which allocates.
- **Fallback.** The D-06b largest-minimum-gain fallback is kept as it was, so its pinned values still hold. When that fallback clamps to zero power, the nearest real speaker now gets unity, which was the fallback before Phase 2. Only stack floats are used.
- **Silence.** An empty list and a non-finite direction stay silent, as they are pinned to be.
- **Documentation.** `computeVBAPGains3D`, the builder header, the guide and the skill now say that callers must pass the output of both builders.
- **Option chosen.** I chose "safe fallback plus documentation" over merging the two builders. Merging would change `buildVBAPTripletsForLayout`'s output for existing callers and for the `activateLayout` D-02a guard.

**New test.** With a regular-only list on every height layout, 64,800 below-horizon directions take the fallback. 27,885 of them were silent before. The test requires unit power at every one, D-06b gains where the old fallback was audible, and the exact nearest-speaker answer where it was not.

### WR-02: `vbapTriplets` holds three kinds of entry, and the wedge was identified by an undocumented implicit sentinel

**Files modified:** `include/SpatialCore/IO/SpeakerLayout.h`, `include/SpatialCore/Engine/RenderEngine.h`, `src/Core/SpatialMath.cpp`, `tests/Core/VBAPTripletSelectionTests.cpp`, `tests/IO/SpeakerLayoutTests.cpp`
**Commit:** 2ae900c
**Applied fix:**
- **Explicit kind.** `VBAPTriplet` now has `enum class Kind { regular, nadirCap, pairWedge }` and an inline `kind()` accessor. A table at the struct documents the encoding.
- **One classifier.** `computeVBAPGains3D` uses `kind()` for both tier selection and output mapping, and both test files call it instead of re-deriving the rule.
- **Comments.** The `lowerHemisphere` doc now names both lower kinds and their order (caps before wedges). `LayoutState::vbapTriplets` is commented.
- **Option chosen.** I used an accessor derived from the existing fields rather than a stored `Kind` field. A stored field that defaults to `regular` would be a second source of truth. It could disagree with `lowerHemisphere` on any triplet built field by field (DR-3). `VBAPTriplet` stays an aggregate, and the selection logic is identical.
- **Test.** The DR-3 test pins the encoding: a non-zero mask is a cap, a zero mask is a wedge, and the default is regular.

### WR-04: The no-fast-math invariant was enforced only by comments

**Files modified:** `src/Core/FloatSemanticsGuard.h` (new), `src/Core/SpatialMath.cpp`, `src/Engine/RenderEngine.cpp`, `src/Algorithms/KNNAlgorithm.cpp`, `src/Algorithms/DirectBinauralAlgorithm.cpp`, `src/OSC/ADMOSCReceiver.cpp`, `CMakeLists.txt`, `docs/integration-guide.md`
**Commit:** 61dd931
**Applied fix:**
- **New private header.** `src/Core/FloatSemanticsGuard.h` stops the build with `#error` when it sees `__FAST_MATH__`, a non-zero `__FINITE_MATH_ONLY__`, or `_M_FP_FAST`. Clang defines `__FINITE_MATH_ONLY__` as 0 by default, so the check tests its value.
- **Where it is included.** Every source that relies on a non-finite test includes it. The IN-01 commit later added ConstantPower and Ambisonics.
- **Consumers.** No public header can reach it, so OpenSpatialDelay keeps its per-target fast-math. I checked OSD's CMakeLists: it applies fast-math with `target_compile_options PRIVATE` on its own targets only.
- **Why there is no separate CMake check.** The header sees the effective flags of each compile, however they arrive (target options, `CMAKE_CXX_FLAGS`, `add_compile_options`), and that includes MSVC `/fp:fast`.

### WR-07: The NaN-dropping `std::max` reduction was still present in the Ambisonics codec tests

**Files modified:** `tests/TestNumerics.h` (new), `tests/IO/AmbisonicsCodecTests.cpp`, `tests/Engine/RenderEngineTests.cpp`, `tests/Algorithms/PanningLawTests.cpp`, `tests/Core/VBAPTripletSelectionTests.cpp`
**Commit:** 3f264d9
**Applied fix:**
- **Shared helper.** `accumulateWorstFinite` moved into the shared `tests/TestNumerics.h`.
- **Ambisonics codec tests.** The SN3D addition-theorem, D-05 parity and round-trip tests now count non-finite values, name the first one, and `CHECK (nonFinite == 0)`.
- **Lower-priority sites.** The review also named two other sites, and both are fixed too:
  - `PanningLawTests`' `maxStepOverSweep` reports non-finite steps, and both callers check the count.
  - The EAR band test counts a non-finite gain as a violation.

### IN-01: The guide's "return silence" list left out ConstantPower and Ambisonics

**Files modified:** `src/Algorithms/ConstantPowerAlgorithm.cpp`, `src/Algorithms/AmbisonicsAlgorithm.cpp`, `tests/Core/VBAPTripletSelectionTests.cpp`, `docs/integration-guide.md`
**Commit:** 5576768
**Applied fix:**
- **Explicit guards.** ConstantPower and Ambisonics now check `std::isfinite` and return early, so their silence no longer depends on the argument order of `std::max`. Finite input is unchanged.
- **Corrected guide.** The guide was missing two algorithms, and it was also too broad. It now says:
  - every algorithm except DBAP is silent for a non-finite azimuth or elevation;
  - on flat layouts, VBAP, VBIP and MDAP ignore elevation;
  - DBAP gives equal gains on every speaker.
- **New test.** It pins that exact behaviour for all 8 algorithms, on Quad and on 7.1.4, for NaN, +Inf and -Inf in each field.

### IN-02: "Positive azimuth toward +Y (left)" used AmbiX axis names

**Files modified:** `include/SpatialCore/Core/SpatialMath.h`, `include/SpatialCore/IO/AmbisonicsCodec.h`, `src/Core/SpatialMath.cpp`, `docs/integration-guide.md`, `.claude/skills/spatial-audio-dsp/SKILL.md`, `tests/reference/gen_sh_reference.py`, `tests/reference/ShReference.h`
**Commit:** 191af3c
**Applied fix:**
- **Wording.** Every place now says "toward the listener's left (AmbiX +Y; SpatialCore's internal +x)".
- **Regenerated header.** `ShReference.h` was regenerated. Only its comment line changed, and it regenerates byte-identically.

### IN-03: `getDecodeMatrix` failed silently

**Files modified:** `include/SpatialCore/IO/AmbisonicsCodec.h`, `src/IO/AmbisonicsCodec.cpp`, `src/Engine/RenderEngine.cpp`, `tests/IO/AmbisonicsCodecTests.cpp`
**Commit:** b87eac0
**Status:** fixed. The public return type changed, so it needs a human to confirm it.
**Applied fix:**
- **Return value.** `getDecodeMatrix` now returns `bool`. It returns false and writes nothing when:
  - `numSpeakers < 0`;
  - `numSpeakers > MAX_SPEAKERS`;
  - the order is outside 0..6. Order -3 used to decode with M = 4.

  A call with 0 speakers is a valid empty decode and returns true.
- **Option chosen.** I took "return bool" plus "reject a negative order", not "zero the buffer on rejection". Zeroing `numSpeakers * M` floats could overrun a caller's buffer that was sized for fewer rows, and it would contradict the pinned D-20 "buffer untouched" test.
- **Compatibility.** Changing `void` to `bool` is source-compatible for any call that ignores the result. No consumer checkout calls the function directly.
- **Engine.** `activateLayout` now checks the result. It runs on the message thread, and its memset already leaves a silent decode if the call is rejected.
- **New test.** It pins every rejection.

### IN-04: Hold-last-good is keyed by slot, which the guide's "per object" wording did not reflect

**Files modified:** `docs/integration-guide.md`, `include/SpatialCore/Engine/RenderEngine.h`, `tests/Engine/RenderEngineTests.cpp`
**Commit:** bfe0a3a
**Applied fix:**
- **Option chosen.** I took the review's reword option. The other option, resetting on a false-to-true `objectLive` edge, has a problem: the engine cannot tell a reused slot from the same OpenSpatialDelay tap being re-enabled, and a re-enabled tap should keep its own last position. Rewording also leaves the audio path unchanged.
- **New wording.** The guide and the header comment now say:
  - the held values belong to the slot;
  - they update every block, whether the slot is live or not;
  - only the constructor and `prepare()` reset them;
  - a consumer should send a complete finite position when it assigns a slot.
- **New test.** It pins this. A finite azimuth sent while the slot is not live becomes the held value. A later NaN renders bit-identically to it, and differently from the default.

### IN-05: The wedge's "pair pan = EAR QuadRegion" equivalence assumes 0-degree ear-level speakers

**Files modified:** `include/SpatialCore/IO/SpeakerLayout.h`, `src/IO/SpeakerLayout.cpp`
**Commit:** 6138d4e
**Applied fix:**
- **Option chosen.** I took the review's "state it in the header contract" option. Tightening the ear-level limit to 1 degree would drop speakers between 1 and 10 degrees out of the ring on consumer layouts, which changes their output.
- **What I measured** from the wedge's linear system:
  - The wedge exactly equals the pair's horizon pan when the pair's two speakers have equal |elevation|. Pairs at 0/0, 10/10 and 5/-5 all give a difference of 0.
  - When the elevations differ, each gain is divided by cos(elevation) before renormalising. For a 0/10-degree pair 30 degrees apart, the largest difference is 0.0059.
- **Contract.** The header contract now states this.

### IN-06: The test re-implemented the production tier classifier, and one comment was badly reflowed

**Files modified:** `tests/Algorithms/PanningLawTests.cpp` (plus the WR-02 commit's test files)
**Commits:** 2ae900c (shared classifier, see WR-02), e235386 (reflow)
**Applied fix:**
- **Classifier.** `testTier` and `SpeakerLayoutTests` now call `VBAPTriplet::kind()`, through WR-02.
- **Comment.** The comment with the 106-character line is reflowed to 80 columns. The wording is unchanged.

### IN-07: The +0.1% epsilon "smallest change the pin must catch" is resolved on only 5 of 15 layouts

**Files modified:** `tests/Engine/RenderEngineTests.cpp`
**Commit:** b642a8e
**Applied fix:** I re-measured each layout with a double-precision solve.

| Layouts | How far +0.1% moves the decode | Bound |
|---|---|---|
| 7.0, 7.1, 7.1.2, 7.1.4, 7.1.6 | 5.85e-5 to 6.30e-5 | Above 2.5e-5, so the pin catches it |
| The other ten | 9.6e-7 (Quad) to 2.2e-5 (5.1.2) | Below 2.5e-5 |

The comment now says this. It also says that a regression confined to those ten layouts is caught only if it is of the 0.1-to-1 kind. This is a comment-only change.

### IN-08: `doublePrecisionAmbiDecode` dropped the library's guards

**Files modified:** `tests/Engine/RenderEngineTests.cpp`
**Commit:** 0f37417
**Applied fix:**
- **Bounds.** The helper now does `REQUIRE (N >= 0)` and `REQUIRE (N <= M)` at the top.
- **Pivots.** It also requires every pivot to be at least 1e-10, the threshold below which the library skips a pivot. The helper does not skip such a pivot the way the library does. It is the exact reference, so a skipped pivot would hide the problem.
- Part (b) of the finding had already been resolved.

### IN-09: Comment hygiene

**Files modified:** `tests/Engine/RenderEngineTests.cpp`
**Commit:** 96263f1
**Applied fix:**
- The duplicated debug-doc path citation is dropped.
- "2.5x below" is corrected to "2.3x below (5.8e-5)", and both ratios now name their numbers.
- A note at the `namespace` line explains why `referenceAmbiDecode` is unindented: it is a byte-for-byte copy of the d43cb15 version.

### IN-12: Ambisonics could be silent everywhere on height layouts and still pass every panning-law check

**Files modified:** `tests/Algorithms/PanningLawTests.cpp`
**Commit:** 224ec32
**Applied fix:** I measured first. Ambisonics is never silent on any shipped layout: 0 of 399,600 samples on a 5-degree elevation grid. The changes:
- **Exemption.** The silence exemption, renamed `mayBeSilentBelowHorizon`, now covers only samples below the horizon on height layouts.
- **Floors.** The power sweep requires at most half of those below-horizon samples to be exempt, and at least half of all samples to be at unit power.
- **On-speaker law.** It is now `UnitPowerOnly`, which requires unit power at each speaker.
- **Mirror and continuity.** Both now count silent samples and require that count to be 0.

As the mutation check above shows, each of these four laws now fails independently on an all-zero decode.

## Notes

- **Not changed: debug asserts in `computeGains`.** `VBAPAlgorithm`, `VBIPAlgorithm` and `MDAPAlgorithm` each `jassert` in `computeGains` that the LayoutContext's triplets match the layout's height. These are Debug-only checks for a mismatched hand-built context. No finding covers them, so I left them alone. They are the same kind of audio-thread diagnostic that WR-03 removed from `computeVBAPGains3D`, in case you want them treated the same way.
- **Not changed: flat-layout 2D VBAP.** On a flat layout with a gap of 180 degrees or more, 2D VBAP (`computeVBAPGains2D`) is also silent at the back. That is outside WR-05, which covers the height-layout lower hemisphere only. No shipped flat layout has such a gap.

---

_Fixed: 2026-10-03T23:20:43Z_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
