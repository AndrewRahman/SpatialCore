---
phase: 02-algorithm-format-verification
verified: 2026-10-03T22:30:00Z
status: human_needed
score: 88/88 must-haves verified
covered_files:
  - .claude/skills/spatial-audio-dsp/SKILL.md
  - .gitignore
  - .planning/phases/02-algorithm-format-verification/02-01-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-01-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-02-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-02-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-03-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-03-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-04-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-04-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-05-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-05-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-06-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-06-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-07-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-07-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-08-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-08-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-09-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-09-SUMMARY.md
  - .planning/phases/02-algorithm-format-verification/02-10-PLAN.md
  - .planning/phases/02-algorithm-format-verification/02-10-SUMMARY.md
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
covered_digest: "v2:sha256:8d6a2060632fb63607523029d71a5d6efdaad9ad4f3abc64159c4b032fbbff45"
behavior_unverified: 0
overrides_applied: 0
re_verification:
  previous_status: human_needed
  previous_score: 77/78
  gaps_closed:
    - "G-02-10: [ambi-pin] failed in Release on Apple Silicon (UAT test 10, severity major): now passes in Release (build-release/) and Debug (build/); Release ctest 193/193"
    - "02-01 backstop truth (D-02a Release abort): previously present-but-behavior-unverified, now carries observed-behavior evidence from UAT test 8 (SIGABRT exit 134 in a scratch Release build, control exit 0)"
  gaps_remaining: []
  regressions: []
coincidental_reliance_items:
  - truth: "Finite inputs are untouched by every guard and every std::isfinite check lives in a .cpp, so OpenSpatialDelay's -ffast-math build cannot fold it away (02-04)"
    reason: undeclared-precondition
    harden: "The guards hold only because the SpatialCore target itself is compiled without -ffast-math / -ffinite-math-only. Turn it into a compile-time #error on __FAST_MATH__ / __FINITE_MATH_ONLY__ in the guarded TUs (review WR-04). Unchanged by this round."
advisory:
  - finding: "WR-06 (02-REVIEW.md): the std::max reductions in [ambi-pin] drop NaN, so a NaN decode passes the pin and the new double-precision anchor vacuously"
    category: other
    reason: "Reproduced by the verifier: with every library decode entry forced to NaN, [ambi-pin] still passed (110 assertions, Release). The codec's own [io][ambisonics] tests do catch it (2 of 7 cases failed under the same mutation), so a NaN decode is not undetected suite-wide. Two-line fix (REQUIRE isfinite per entry) is in the review."
    evidence_status: "reproduced by verifier (temporary mutation of src/IO/AmbisonicsCodec.cpp, reverted, git status clean)"
  - finding: "Phase 1 criterion 4 mechanical check drifted from 0 to 11 matches (3 sites are not in the Owner/Repo#N form)"
    category: other
    reason: "See 'Cross-phase regression check' below. 9 of 11 are fully qualified AndrewRahman/SpatialCore#N; the 3 unqualified sites are tests/Algorithms/PanningLawTests.cpp:65, :836 (bare #22) and tests/IO/AmbisonicsCodecTests.cpp:170 (SpatialCore#11, no owner). Cosmetic: all name SpatialCore's own tracker."
    evidence_status: "reproduced by verifier (grep, line list below); live issue resolution not re-run because gh returns HTTP 401"
human_verification:
  - test: "Accept the 2.5e-5 (25 parts per million) bound for the [ambi-pin] decode check, in place of the old 1e-6 (one part per million). 02-10's one plain-English yes/no question, harvested here, not answered."
    expected: "Answer yes or no. Yes: accept it (recommended). The decoder in the library is unchanged, so nothing you hear changes. The old limit came from a debug-build measurement and was never derived; the new limit is derived from the matrix conditioning, re-checked against a double-precision solve on every run, and I reproduced that it still fails for a deliberate +0.1% change to the decoder in both Release and Debug. No: keep 1e-6 and force identical rounding with a compiler flag on the whole library, which slows the audio code and means re-checking OpenSpatialDelay's sound; that needs a new decision before any change."
    why_human: "Loosening a test's limit is a trust judgment. The numbers are checked, the acceptance is the user's. Nothing else is blocked on the answer."
  - test: "Judgment-tier prohibitions from plan 02-10 (4). I recorded a NON-AUTHORITATIVE judgment for each; all 4 hold. See the Prohibitions table."
    expected: "Accept or reject. unverified-prohibition: human review recommended."
    why_human: "Judgment-tier items are never silently absorbed into a pass. The 25 earlier prohibitions were already accepted in UAT tests 5 and 9."
---

# Phase 2: Algorithm & Format Verification Verification Report

**Phase Goal:** Every algorithm and every advertised format is confirmed correct by a test, and the Ambisonics convention is written down.
**Verified:** 2026-10-03T22:30:00Z
**Status:** human_needed
**Re-verification:** Yes, after gap closure (plan 02-10, UAT gap G-02-10); the G-02-2 round that preceded it is kept below for the record

## Summary

Gap G-02-10 is closed. `[ambi-pin]` and the full suite now pass in a Release build on Apple Silicon (`build-release/`, `CMAKE_BUILD_TYPE=Release`) as well as in Debug (`build/`). All four ROADMAP success criteria still hold, nothing regressed, and there are no blockers. Status is `human_needed` rather than `passed` for one reason: plan 02-10 asks the user a yes/no question (accept a 25-parts-per-million limit for the decode check instead of one part per million) and its four judgment-tier prohibitions are flagged for human review. Two warnings are recorded and neither blocks: WR-06 (the check's `std::max` reductions drop NaN) and a cosmetic drift in the Phase 1 issue-reference grep.

**Test run (my own, this pass):**

| Run | Result |
|---|---|
| `cmake --build build` and `build-release` (`SpatialCoreTests`) | both up to date, exit 0; `CMAKE_BUILD_TYPE` confirmed Debug and Release in the two caches |
| Debug, `./build/tests/SpatialCoreTests` (full, no exclusion) | **193 cases, 192 passed, 1 failed; 304,868 assertions, 1 failed.** The one failure is `tests/Binaural/HutubsPP2Tests.cpp:47` (golden checksum, got `0x7a35c1f848c2a410`, expected `0x9e3c2875eeade4b7`). Matches the orchestrator's 192/193. |
| Debug, with that case excluded | All passed (304,865 assertions in 192 test cases) |
| Release, `./build-release/tests/SpatialCoreTests` (full, no exclusion) | All passed (304,868 assertions in 193 test cases); HUTUBS PP2 passes in Release |
| Release, `ctest --test-dir build-release/tests` | 100% passed, 193 of 193. Matches the orchestrator's figure. |
| `[ambi-pin]` alone, Release and Debug | both: All tests passed (110 assertions in 2 test cases) |
| Ten Phase 2 tags (`[g02-2]`, `[band]`, `[vbap3d-identity]`, `[consumer-surface]`, `[ear]`, `[io][layout]`, `[panning-law]`, `[sn3d]`, `[roundtrip]`, `[tracer]`), each alone in both trees | all 20 runs pass, identical assertion totals in both builds (203, 828, 262,104, 13, 19,215, 19,119, 7,191, 153, 6, 114) |

The HUTUBS PP2 Debug failure is pre-existing (introduced with the golden in 2d6dc08, plan 08-04), is recorded in `deferred-items.md`, and belongs to EXTR-02 (Phase 3), not to any Phase 2 truth. The source diff since the last verification pass (`git diff a322673 HEAD -- . ':!.planning' ':!.gsd'`) is two files: `.gitignore` (+1 line) and `tests/Engine/RenderEngineTests.cpp`. So everything verified in the earlier round could only have regressed through that test file, and the tag sweep above passes.

## G-02-10 closure (the focus of this re-verification)

**The gap, per 02-UAT.md test 10:** the full test suite must pass in a Release build, the build type CI uses; `[ambi-pin]` failed there (RenderEngineTests.cpp:700, decode matrix differing from the test's reference decoder by up to 5.3e-6 against a 1e-6 bound on 10 of 15 layouts).

**Code evidence (read, not taken from SUMMARY):**

- `tests/Engine/RenderEngineTests.cpp:708-709`: `kAmbiPinTolerance = 2.5e-5f`, `kAmbiFloatVsDoubleTolerance = 4.0e-5`. The `[ambi-pin]` test (line 785) applies `CHECK (worstHere <= kAmbiPinTolerance)` per layout and again over all 15 (line 832), plus two double-precision anchor checks per layout (lines 825-826), and `layoutsChecked == 15` is asserted.
- `doublePrecisionAmbiDecode` (lines 715-782) is a real double-precision replica of the decoder (same Tikhonov term, Gauss-Jordan with partial pivoting, same `D = E^T inv` product) fed the same float SH values, and is compared against `active.ambiDecodeMatrix`, the array `RenderEngine::activateLayout` fills through `AmbisonicsCodec::getDecodeMatrix`. It is not tautological.
- The derivation comment (lines 673-707) matches the debug session's numbers (cond 501 to 1786, spread 0 Debug / 5.3e-6 Release, 1.0e-5 worst variant, 1.4e-5 / 1.7e-5 / 1.8e-5 float-vs-double). Stale wording is gone: the header comment at lines 562-572 now says "within float rounding, not exact", and the test title no longer says "equals". Minor: the comment says the bound is "2.5x below" the +0.1% change while the real ratio is 5.8e-5 / 2.5e-5 = 2.3x (review IN-09).
- `git diff 1ac244a HEAD -- src include CMakeLists.txt tests/CMakeLists.txt .github` is empty (0 lines). No floating-point flag was added. The 85-line `referenceAmbiDecode` (old lines 574-658, now 576-660) is byte-identical to `1ac244a` (`diff` of the two ranges is empty).
- `.gitignore` is CRLF throughout and its last line is `build-release/\r\n`; `git check-ignore -v build-release` reports `.gitignore:7`. `git status` shows only the three pre-existing untracked entries (`.gsd/`, `.planning/milestone.lock`, `.planning/state.json`).
- Recurrence guard: `02-VALIDATION.md` carries the Release suite command (line 27) and the Release tree configure recipe (line 28) and requires the Release suite before `/gsd-verify-work` (line 41); `.planning/codebase/TESTING.md` states the both-build-types rule and why. `deferred-items.md` lists the four 02-10 deferred entries (tight test-local tolerances, no arm64/FMA CI leg, HUTUBS PP2 Debug-only, CI ctest step runs zero tests).

**My own mutation proof (independent of the SUMMARY's, run in this worktree and reverted).** `float epsilon = 0.01f` to `0.01001f` at `src/IO/AmbisonicsCodec.cpp:75` (+0.1%), rebuilt, `[ambi-pin]` run:

| Build | Result with the mutation | After `git checkout` of the file and rebuild |
|---|---|---|
| Release | **FAILED**: 11 assertions failed (5 per-layout pin checks at :824, 5 anchor checks at :825, the all-layout check at :832); worst `abs(new - old)` 6.28e-05 against 2.5e-5; library vs double solve 5.79e-05 to 6.29e-05 against 4.0e-05 | 110 assertions pass |
| Debug | **FAILED**: 11 of 110 assertions failed, same shape | 110 assertions pass (Release ctest 193/193 re-confirmed after the revert) |

So the bound is loose enough to pass the real Release spread (about 5.3e-6) and tight enough to catch a +0.1% decoder change, in both build types, with two independent checks tripping. `git status --short src` was clean afterwards.

**Verdict: G-02-10 is CLOSED.**

### WR-06 reproduced (warning, not a blocker)

The review's one warning is real. `std::max (acc, std::abs (x - y))` returns `acc` when `y` is NaN, so NaN entries are dropped. I proved it two ways: a compiled snippet (`std::max (0.0f, std::abs (NaN - 1.0f))` gives 0, so `0 <= 2.5e-5` passes), and a temporary mutation forcing every library decode entry (`AmbisonicsCodec.cpp:130`) to quiet NaN: `[ambi-pin]` still reported **All tests passed (110 assertions in 2 test cases)**. The mutation was reverted. Impact is bounded: under the same mutation the codec's own `[io][ambisonics]` cases failed (2 of 7, `AmbisonicsCodecTests.cpp:112` `REQUIRE (std::isfinite (v))`), so a NaN decode out of `getDecodeMatrix` is still caught by the suite. What is overstated is the pin's own guarantee for the new anchor ("independent"). The fix is the two-line `REQUIRE (std::isfinite (...))` in 02-REVIEW.md. It is also the only open finding that touches this plan's artifact, so it is worth doing before the phase is closed, but nothing in a must-have depends on it. The disposition ledger records it as `open`.

## G-02-2 closure (previous round, kept for the record)

**The gap, per 02-UAT.md:** on height layouts a source between 0 and -30 degrees elevation must pan as EAR does: the horizon pan holds (7.1.4 az 60 stays 0.7071 / 0.7071 on M+030 / M+090), no lean, no side flip.

**Code evidence (read, not taken from SUMMARY):**

- `src/IO/SpeakerLayout.cpp` `appendLowerHemisphereTriplets`: the four overlapping trapezoid triangles are no longer emitted (the loop `continue`s on any facet without the nadir vertex). For each nadir-cap facet it records the neighbouring ear-level pair (`capPairs`) and then pushes one pair-pan wedge per pair: the two real speakers plus the virtual nadir, `nadirVertex = 2`, `nadirMask = 0`, `nadirGain = 0`. That is 2n lower-hemisphere triplets for n ear-level speakers.
- `src/Core/SpatialMath.cpp` `computeVBAPGains3D`: `selectionTier` (integer tests only) classifies regular, cap and wedge. The loop is `for (pass = 0; pass < 3 && bestTri < 0; ++pass)`, so regular triplets run first with the original loop body, then caps, then wedges. Tolerance is still `-1e-6f` (one occurrence at line 312). No allocation, lock or logging was added. The output mapping for a zero-share wedge gives the pair's 2D pan, power-normalised.
- Public surface: `git diff 306b025 HEAD -- include/` contains comment lines only (the single non-comment `+` line is blank). `VBAPTriplet`, `LayoutContext` and the `SpatializationAlgorithm` interface are unchanged. `[consumer-surface]` passes.
- `src/Engine/` has no diff since 306b025, so the `activateLayout` guard and abort are untouched.

**My own probe (independent of the Catch2 tests).** A standalone program, compiled against this branch and `build/libSpatialCore.a`, drove `RenderEngine::setOutputFormat` then `VBAPAlgorithm::computeGains`. The probe was in `/tmp/g022probe`, outside the repo. Results:

| Format | Azimuth | Elevations 0, -5, -15, -25, -30 | Gains |
|---|---|---|---|
| 7.1.4 | 60 | identical at every elevation | speaker 0 = 0.7071, speaker 3 = 0.7071, all others below 1e-4 |
| 7.1.4 | -60 | identical | speakers 1 and 4 = 0.7071 |
| 7.1.4 | 45 | identical | 0.9391 / 0.3437 |
| 5.1.4 | 60 | identical | 0.8374 / 0.5466 (matches the pinned ear value for el -15) |
| 5.1.4 | -60, 45 | identical | mirror / 0.9616 / 0.2746 |

Before the fix the earlier verifier measured 0.7210 / 0.6929 at el -5, 0.7472 / 0.6646 at -15 and 0.6768 / 0.7361 at -25. No lean, no side flip, no height speaker used.

**Oracle evidence:**

- `tests/reference/EarReference.h` gained 17 `kEarBand*` entries (26 band cases) produced by `gen_ear_reference.py` from PyPI ear 2.1.0. The regenerate-diff is empty. The only lines removed from the header are the old "band deliberately NOT pinned" comment. The 13 nadir-cap pins are unchanged.
- 5.1.4's band pins stop at |az| 100 (the generator carves out beyond 110). The header's 5.1.4 cases are (60,-15), (-70,-25) and (-100,-20).
- `[ear][golden][band]` matches ear within 1e-5 (measured max 1.79e-7). `[ear][band]` checks an independent double-precision pair pan at 720 x 9 directions on all 8 layouts, plus exactly one enclosing wedge and no regular or cap enclosure. The band continuity bound in `PanningLawTests.cpp` is now 0.01 for the band (-5, -15, -25) and a new band/cap seam scope (-35, -40). It was 0.12.
- `[vbap3d-identity]` (262,104 assertions) is bit-identical to `d43cb15` above the horizon, so issue #22's above-horizon tie is untouched, as intended.

**Docs match code:** the guide, README and skill state the pair-region band, the PyPI ear 2.1.0 match, the 5.1.4 exception and #22 above the horizon only. I confirmed the phrases are present (`one pan region` once in the guide and twice in the skill, `PyPI ear 2.1.0` likewise, `except behind the listener on 5.1.4` in README, `above the horizon on height layouts` in the skill) and that CRLF is intact on every line (guide 282/282, README 144/144, skill 583/583). The guide's "about -59 degrees" depth is a derivation, not a measurement. 02-09-SUMMARY states that openly and I re-derived it: tan(el) = tan(-30)/cos(70) gives -59.3 for a 140-degree pair.

**Verdict: G-02-2 is CLOSED.**

## UAT carry-forward

`02-UAT.md` is `status: diagnosed`: 10 tests, 8 passed, 1 issue (test 10, G-02-10), 1 skipped (test 7). G-02-2 inside it is already `resolved`; G-02-10 is still `failed` because the file has not been updated since the 02-10 closure. The orchestrator should move test 10 to pass (evidence above) and G-02-10 to `resolved`.

| UAT test | UAT result | Status now |
|---|---|---|
| 1 DR-3 cross-repo build | pass | Carried. `src`, `include` and CMake are untouched since 02-09, so the OSD build result still applies. |
| 2 Below-horizon band | pass (G-02-2 resolved by 02-08, 02-09) | Re-checked: `[g02-2]`, `[band]`, `[ear]`, `[vbap3d-identity]`, `[consumer-surface]` pass in both builds. |
| 3 WR-03 hand-built contexts | pass (accepted under D-01) | Carried. |
| 4 Process gates | pass | Carried. `gh` returns HTTP 401 this session, so `gh issue view 22` was not re-run. |
| 5 Judgment prohibitions (17) | pass | Carried. |
| 6 5.1.4 rear gap | pass (user: "yes, leave it as it is") | Carried. This closes the human item the previous report listed. |
| 7 Listening check | skipped (deferred follow-up for the next round of listening reviews) | Carried as a deferred follow-up, not a gate. |
| 8 D-02a Release abort | pass (SIGABRT exit 134, control exit 0) | Carried; see the 02-01 row below. `src/Engine` is unchanged. |
| 9 Judgment prohibitions (25) | pass | Carried. |
| 10 Release suite | issue (G-02-10) | **Now passes**, as shown above. Needs the UAT file updated. |

## Goal Achievement

### ROADMAP Success Criteria

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | AmbisonicsCodec channel order and normalisation confirmed ACN/SN3D or FuMa, stated in code and docs (SpatialCore#11) | ✓ VERIFIED | Unchanged by 02-08 and 02-09 (no diff to `AmbisonicsCodec.*` or `[sn3d]` tests since the first verification). Convention docblocks in `SpatialMath.h`, `AmbisonicsCodec.h` and the guide. `[sn3d]` runs the 49 scipy literals within 1e-5, the addition theorem at orders 1-6, and negative-elevation parity. The scipy reference regenerates byte-identically. |
| 2 | The 3D triplet fallback in VBAP/VBIP/MDAP either no longer exists or fails loudly | ✓ VERIFIED | Both nearest-speaker paths stay deleted: `grep -rn nearestSpeaker3DFallback src/` is empty. The engine abort sits at `RenderEngine.cpp:729`. The no-enclosing-triplet case still hits Debug `jassertfalse` plus the largest-min-gain triplet (`SpatialMath.cpp`). The new pair wedges make coverage better, not worse: `[ear][coverage]` still passes (0 uncovered finite directions on all 8 height layouts). |
| 3 | All 23 OutputFormat entries resolve; all 15 layouts return populated channel indices and LFE placement | ✓ VERIFIED | `[io][golden]`, `[format-resolve]`, `[io][layout]` (19,119 assertions / 7 cases this run) all pass. The layout test now asserts the exact 2n extras structure. |
| 4 | Ambisonics encode/decode round-trips within tolerance at every order up to 6 | ✓ VERIFIED | `[roundtrip]` passes, with the previously noted limit: the order-6 decode half is test-local (D-11/D-12, shipped decoder capped at order 3 / 16 speakers). |

### Plan must-have truths

**02-01 to 02-07 (60 truths, unchanged scope).** I re-ran the tags that back them (`[tracer]`, `[vbap3d-identity]`, `[consumer-surface]`, `[ear]`, `[panning-law]`, `[io][layout]` and the full suite) and re-ran all three reference generators. Everything that was VERIFIED before is still VERIFIED, with these changes:

| Truth | Before | Now | Why |
|---|---|---|---|
| 02-07: docs describe below-horizon as EAR, with no nearest-speaker (accuracy caveat) | ✓ VERIFIED (as worded) | ✓ VERIFIED | The 0 to -30 band is now EAR-exact, so the caveat is gone |
| 02-04: below-horizon VBAP matches ear 2.1.0 (as scoped to the nadir cap) | ✓ VERIFIED (as scoped) | ✓ VERIFIED | The band is now pinned too |
| 02-02: numpy/scipy/ear only after human verification | ? UNCERTAIN | ✓ VERIFIED | UAT test 4: the user's own delegation is quoted in 02-02-SUMMARY |
| 02-03: nothing filed until a human approved the text | ? UNCERTAIN | ✓ VERIFIED | UAT test 4: filed issue body identical to the approved draft `02-03-ISSUE-BODY.md` |
| 02-01: D-02a abort, every build type, not reachable from renderBlock | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED (backstop) | ✓ VERIFIED | UAT test 8 observed the behavior: a Release build with the triplet builder sabotaged aborted in `setOutputFormat(Surround7_1_4)` (SIGABRT, exit 134, the "returned" line never printed) and returned normally with the sabotage reverted (exit 0). Placement re-confirmed: one `std::abort()` at `src/Engine/RenderEngine.cpp:729`, none in `renderBlock`. It is a one-off scratch experiment, not a regression test (no Catch2 test can survive an abort), so it stays a manual backstop. |
| 02-04: finite inputs untouched, isfinite only in .cpp | ✓ VERIFIED (coincidental-reliance) | ✓ VERIFIED (coincidental-reliance) | Unchanged advisory (WR-04). |

**02-08: below-horizon band matches EAR (9 truths)**

| Truth | Status | Evidence |
|---|---|---|
| 7.1.4 az 60 holds 0.7071 / 0.7071 from 0 to -30; az -60 mirrors on speakers 1 and 4; no lean, no side flip | ✓ VERIFIED | `[g02-2]` passes (203 assertions). My own probe reproduces it through `RenderEngine::setOutputFormat` and `VBAPAlgorithm::computeGains`. |
| Exactly 2n lower triplets (n caps with every ear-level bit and `1/sqrt(n)`, n wedges with `nadirMask = 0`, `nadirGain = 0`), no trapezoid triangle remains | ✓ VERIFIED | Builder code read. `[io][layout][ear]` asserts extras == 2n, n caps, n wedges and matching pair sets. |
| Selection order: regular, then cap, then wedge; no min-sum tie below the horizon | ✓ VERIFIED | `selectionTier` plus `pass < 3`. `[ear][band]` asserts exactly one enclosing wedge and no regular or cap enclosure per band direction. |
| Band gains equal an independent double-precision 2D pair pan (within 1e-5) on all 8 layouts, 0.5-degree azimuth, 9 elevations, including 5.1.4's rear gap | ✓ VERIFIED | `[band]` passes (828 assertions). The summary reports max 2.98e-7. |
| 26 band directions pinned against ear 2.1.0 within 1e-5, regenerate-diff empty, no 5.1.4 pin beyond 110 degrees, 13 cap pins unchanged | ✓ VERIFIED | I ran the regenerate-diff myself (identical). The header's removed lines are comments only. 5.1.4 pins are at az 60, -70 and -100. |
| Band and band/cap seam continuity at most 0.01 per 0.1 degree; the old bound was 0.12 | ✓ VERIFIED | `PanningLawTests.cpp:966-967` scopes `-30..0 band` and `band/cap seam` at 0.01f. `[panning-law]` passes. |
| Above the horizon nothing changes (bit-identical to d43cb15; #22 untouched) | ✓ VERIFIED | `[vbap3d-identity]` passes (262,104 assertions). Pass-0 loop body unchanged. |
| Tolerance stays -1e-6f in every pass | ✓ VERIFIED | One `g0 >= -1e-6f` site (`SpatialMath.cpp:312`) serves all passes. |
| Public surface unchanged (comment-only header diff, `[consumer-surface]` passes) | ✓ VERIFIED | Header diff is comments only; `[consumer-surface]` passes; the OSD build compiles. |

**02-09: docs, gate, DR-3 (9 truths)**

| Truth | Status | Evidence |
|---|---|---|
| Integration guide states the pair region, 0.7071 / 0.7071, EAR match except 5.1.4 behind the listener (U+135 / U-135), #22 above the horizon only | ✓ VERIFIED | Text read in the diff; required phrases present. |
| README VBAP bullet states the EAR match except behind the listener on 5.1.4 | ✓ VERIFIED | `grep` confirms. |
| Skill states the same in both below-horizon paragraphs and scopes step 4's tie | ✓ VERIFIED | `one pan region` x2, `PyPI ear 2.1.0` x2, `above the horizon on height layouts` x1. |
| All three docs keep CRLF on every line | ✓ VERIFIED | 282/282, 144/144, 583/583. |
| WR-01 recorded as fixed in 02-REVIEW-DISPOSITION.md | ✓ VERIFIED | Frontmatter and table row both say `fixed`. The count now reads `open: 10, total: 11` because the later review (ec957c1) added WR-05, IN-05 and IN-06 and dropped nothing else, so the "open count 7" in the plan was correct at the time of 02-09 and has since moved. |
| Phase gate: full suite minus HUTUBS passes; every Phase 2 tag including `[band]` and `[g02-2]` passes | ✓ VERIFIED | My run reproduces 304,835 assertions / 192 cases; the tag sweep above passes. |
| DR-3: OSD 30391cd compiles and links against this branch, OSD repo status unchanged, nothing installed | ✓ VERIFIED | Build log evidence above; `git -C ~/conductor/repos/openspatialdelay-v1 status --porcelain` is empty. I did not rebuild OSD (instructions). |
| 02-09-SUMMARY carries the superseding cross-repo follow-ups (4 files, five audible changes, #22 comment suggested but not posted) | ✓ VERIFIED | Section present with all three. |
| The user gets one plain-English yes/no question and Claude runs the checks | ✓ VERIFIED | Question present in 02-09-SUMMARY, harvested as a human item below. |

**02-10: honest [ambi-pin] bound (9 truths)**

| Truth | Status | Evidence |
|---|---|---|
| `[ambi-pin]` passes in Release (arm64, `build-release/`) and Debug within `kAmbiPinTolerance = 2.5e-5` on all 15 layouts | ✓ VERIFIED | Both builds: 110 assertions in 2 test cases pass. Release ctest 193/193. This supersedes plan 02-06's "within 1e-6" truth, which the debug session showed was never achievable on arm64 Release. |
| The bound is derived, and the comment records the derivation and no longer says the pin is exact | ✓ VERIFIED | Comment lines 673-707 read and cross-checked against the debug session; header comment and test title now say "within float rounding". The 2.5x vs 2.3x wording slip is IN-09. |
| Each float decoder is within `kAmbiFloatVsDoubleTolerance = 4.0e-5` of a double-precision decode on all 15 layouts in both build types | ✓ VERIFIED | Checks at :825-826 pass in both builds. They tripped under my +0.1% mutation (5.79e-05 to 6.29e-05), so they are live. |
| +0.1% Tikhonov change fails the pin in both builds (at least 3 per-layout checks plus the all-layout check) and the tree passes again once reverted | ✓ VERIFIED | Reproduced by me: 11 failed assertions in each build (5 per-layout, 5 anchor, 1 all-layout), 110 pass after revert. |
| Library decoder and verbatim reference unchanged; no FP flag | ✓ VERIFIED | `git diff 1ac244a HEAD -- src include CMakeLists.txt tests/CMakeLists.txt .github` is empty; `referenceAmbiDecode` byte-identical. |
| Phase gate in both build types: Debug full suite except HUTUBS; Release under ctest with no exclusion; Phase 2 tags in both | ✓ VERIFIED | My runs: Debug 192/193 (HUTUBS only), Release 193/193; ten tags each in both builds pass. The SUMMARY's 17-tag sweep was not repeated in full; the ten above include every tag that backs the ROADMAP criteria. |
| Recurrence guard: Release command in VALIDATION, build-type rule in TESTING.md, `build-release/` gitignored | ✓ VERIFIED | Files read; `git check-ignore` confirms. The `ctest` directory caveat the executor found (`build-release/tests`, not the build root) is recorded in both files and is correct. |
| Out-of-scope items recorded, not fixed (tight tolerances, no arm64/FMA CI leg, HUTUBS Debug-only) | ✓ VERIFIED | Four `02-10:` entries in `deferred-items.md` (the fourth, CI's ctest step running zero tests, was found during execution). `.github/` diff is empty. |
| The user gets one plain-English yes/no question and Claude runs every build and test | ✓ VERIFIED | Question present in 02-10-SUMMARY, harvested as a human item below. |

**Score:** 88/88 truths verified (78 from the earlier rounds, 9 new for 02-10, with the 02-01 backstop truth moved from present-but-behavior-unverified to verified on UAT test 8's observed abort; 0 present-but-behavior-unverified).

### Deferred Items

Items not met inside Phase 2 and addressed (or owned) later. Informational only, they do not affect the status.

| # | Item | Addressed In | Evidence |
|---|------|-------------|----------|
| 1 | CI has no arm64/FMA leg, and CI's `ctest` step runs zero tests (build-root `ctest` finds none; `.github/workflows/ci.yml`) | Phase 6 | ROADMAP Phase 6 success criterion 3: "A green macOS CI run builds against JUCE 9.0.0 and runs the Catch2 suite. (SpatialCore#9)". Clear match for both. |
| 2 | HUTUBS PP2 golden checksum fails in Debug only (`HutubsPP2Tests.cpp:47`) | Phase 3 (per `deferred-items.md` and EXTR-02), not named in ROADMAP text | Phase 3 lists EXTR-02 and "dedicated test files" for `PartitionedConvolver` and `BinauralRenderer`, but no ROADMAP sentence names the golden checksum. Treated as a pre-existing out-of-scope failure, not as matched-and-deferred. It does not touch a Phase 2 truth. |

### Prohibitions (judgment-tier, non-authoritative LLM judgments; human review recommended)

The first 25 are carried from earlier rounds (UAT tests 5 and 9 accepted them). The previously partly-failing 02-07 item holds. Four new items from 02-10 are at the bottom of the table and are not yet accepted:

| Plan | Prohibition (short) | Verifier judgment |
|---|---|---|
| 02-07 | Docs must not describe code differently from what it computes | **Now holds.** The band is EAR-exact and the tie scope is accurate. Residual minor wording items IN-01 and IN-04 are unchanged and not covered by this prohibition's core. |
| 02-08 | No output change above the horizon; #22 stays separate | Holds: `[vbap3d-identity]` |
| 02-08 | No alloc, lock or log in `computeVBAPGains3D`; wedges built on the layout-build thread | Holds: only integer tests and stack floats added; wedges built in `appendLowerHemisphereTriplets` |
| 02-08 | No public member added, removed or retyped; no change to `SpatializationAlgorithm` or the 4-member `LayoutContext` | Holds: header diff is comments only |
| 02-08 | No special case for 5.1.4's rear gap in code | Holds: the builder has no layout-specific branch; the gap gets the same wedge |
| 02-08 | No hand-edited `EarReference.h`, no value derived from SpatialCore code | Holds: byte-identical regeneration from ear 2.1.0 |
| 02-09 | Docs must not describe behaviour the code lacks | Holds, with the "about -59 degrees" figure a stated derivation I re-checked |
| 02-09 | OSD repo not modified; no installing targets built; no GitHub post; CONTEXT unchanged | Holds: OSD status empty; no `gh` write; I did not touch OSD |
| 02-09 | No change outside the named bullets and paragraphs | Holds: guide +24/-12 in two bullets, README one line, skill three lines |
| 02-10 | No change under `src/` or `include/`; no edit to any line of `referenceAmbiDecode` | Holds: `git diff 1ac244a HEAD -- src include` is empty; the function text is byte-identical |
| 02-10 | No `-ffp-contract` or other FP flag in `CMakeLists.txt` or `tests/CMakeLists.txt` | Holds: diff of both is empty |
| 02-10 | Do not tune a tolerance to make a measurement pass; stop and report if a stop rule breaks | Holds on the evidence: the number is derived from measured conditioning and spread, the SUMMARY records the stop rules (Release worst 5.28e-6 vs limit 1.25e-5; worst float-vs-exact 1.68e-5 vs limit 2.0e-5) as not broken, and my mutation shows the bound still discriminates. The limit did move from 1e-6 to 2.5e-5, which is the user's yes/no decision below. |
| 02-10 | Do not commit the mutation or anything from the disposable worktree; do not touch HUTUBS PP2, other tolerances, `ci.yml` or `/tmp/sc-backstop` | Holds: `git log -p 1ac244a..HEAD -- src` is empty, `.github/` diff is empty, the only test file changed is `RenderEngineTests.cpp`, `/tmp/sc-backstop` still exists |

### Required Artifacts

| Artifact | Status | Details |
|---|---|---|
| `src/IO/SpeakerLayout.cpp` | ✓ VERIFIED | Cap and wedge emission, shared `setInverse`, banner rewritten; WIRED via `RenderEngine.cpp:735` |
| `src/Core/SpatialMath.cpp` | ✓ VERIFIED | `selectionTier` and three-pass selection; WIRED (read by VBAP/VBIP/MDAP through `computeVBAPGains3D`) |
| `include/SpatialCore/IO/SpeakerLayout.h` | ✓ VERIFIED | Comment-only change |
| `tests/IO/SpeakerLayoutTests.cpp`, `tests/Core/VBAPTripletSelectionTests.cpp`, `tests/Algorithms/PanningLawTests.cpp` | ✓ VERIFIED | All registered and run; the three new test groups pass |
| `tests/reference/gen_ear_reference.py`, `EarReference.h`, `README.md` | ✓ VERIFIED | Regenerate byte-identically |
| `docs/integration-guide.md`, `README.md`, `.claude/skills/spatial-audio-dsp/SKILL.md` | ✓ VERIFIED | New text present, CRLF intact |
| `02-REVIEW-DISPOSITION.md`, `02-VALIDATION.md` | ✓ VERIFIED | WR-01 fixed; the four new validation rows exist (still marked pending, a bookkeeping item, not a goal gap) |
| `tests/Engine/RenderEngineTests.cpp` (`[ambi-pin]`, `doublePrecisionAmbiDecode`, two derived constants) | ✓ VERIFIED | Substantive (real double-precision solve, not a stub), wired (reads `RenderEngine::getActiveLayout().ambiDecodeMatrix`), registered in `tests/CMakeLists.txt` and run in both builds. See WR-06 for the NaN blind spot. |
| `.gitignore`, `02-VALIDATION.md`, `.planning/codebase/TESTING.md`, `deferred-items.md` | ✓ VERIFIED | `build-release/` ignored; Release gate and ctest-directory caveat present; four 02-10 deferred entries present. The `02-VALIDATION.md` rows for 02-08, 02-09 and 02-10 still read pending (bookkeeping, not a goal gap). |
| Earlier plans' artifacts (02-01 to 02-07) | ✓ VERIFIED | No regressions; tests above pass |

### Key Link Verification

| From | To | Status | Details |
|---|---|---|---|
| `RenderEngine::activateLayout` | `appendLowerHemisphereTriplets` | ✓ WIRED | Called after the guard, before the swap (`RenderEngine.cpp:735`) |
| `computeVBAPGains3D` | wedge and cap fields | ✓ WIRED | `selectionTier` reads `lowerHemisphere`, `nadirVertex`, `nadirMask`; the output mapping reads `nadirGain` |
| Docs | tests | ✓ WIRED | The guide's 0.7071 / 0.7071 example and the PyPI ear 2.1.0 claim are `[g02-2]` and the `[ear][golden][band]` pins |
| `EarReference.h` | `gen_ear_reference.py` | ✓ WIRED | Regenerate-diff empty (generators and reference headers unchanged since the last pass: `git diff a322673 HEAD -- tests/reference` is empty) |
| `[ambi-pin]` | `AmbisonicsCodec::getDecodeMatrix` via `RenderEngine::setOutputFormat` then `activateLayout` | ✓ WIRED | My mutation of the codec's epsilon (and of its output to NaN) changed `[ambi-pin]`'s outcome, which could not happen unless the test reads the library's matrix |
| `[ambi-pin]` derivation comment | `.planning/debug/ambi-pin-release-tolerance.md` | ✓ WIRED | Cites G-02-10 and the path; figures match |
| `02-VALIDATION.md` Release command | `build-release/` | ✓ WIRED | Command runs 193 tests; `build-release/` is gitignored |

### Data-Flow Trace (Level 4)

Not applicable to UI. The equivalent trace is `setOutputFormat` then `activateLayout` then `vbapTriplets` then `computeGains` then output gains. My probe exercises the whole chain with real engine-built triplets, not test-built ones, and the gains are non-trivial (0.7071, 0.9391, 0.8374). Status: ✓ FLOWING.

### Behavioral Spot-Checks

| Behaviour | Command | Result | Status |
|---|---|---|---|
| Suite builds, both trees | `cmake --build build` and `cmake --build build-release` (`--target SpatialCoreTests`) | exit 0, up to date | ✓ PASS |
| Release suite as CI would run it | `ctest --test-dir build-release/tests --output-on-failure` | 100% passed, 193 of 193 | ✓ PASS |
| G-02-10 gap itself | `./build-release/tests/SpatialCoreTests "[ambi-pin]"` and the same in `build/` | 110 assertions pass in each | ✓ PASS |
| The pin can still fail | temporary +0.1% epsilon mutation, both trees, then revert | 11 failed assertions in each; 110 pass after revert | ✓ PASS |
| Debug suite passes except the deferred HUTUBS case | `./build/tests/SpatialCoreTests "~HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)"` | 192 cases / 304,865 assertions pass | ✓ PASS |
| G-02-2 end to end | own probe (7.1.4, 5.1.4) | 0.7071 / 0.7071 at 0, -5, -15, -25, -30; mirrors at -60 | ✓ PASS |
| Reference headers are generated, not typed | three `gen_*_reference.py` vs checked-in headers | 3/3 identical | ✓ PASS |
| Ten Phase 2 tags | per-tag runs in `build` and `build-release` | all 20 runs pass | ✓ PASS |
| Issue #22 still open | `gh issue view 22` | **SKIPPED**: `gh` returns HTTP 401 this session. UAT already recorded the match. | ? SKIP (no decision rides on it) |

### Probe Execution

No `scripts/*/tests/probe-*.sh` exist and no plan declares one. Step 7c: N/A.

### Requirements Coverage

| Requirement | Source Plans | Description | Status | Evidence |
|---|---|---|---|---|
| EXTR-01 | 02-01, 02-02, 02-03, 02-04, 02-05, 02-07, 02-08, 02-09 | Every public algorithm computes correct gains; the 3D triplet fallback is eliminated or fails loudly | ✓ SATISFIED | Algorithm tests pass in both builds; `[panning-law]` covers all 8; the nearest-speaker fallback has no caller in `src/` (only the deprecated public inline definition at `include/SpatialCore/Core/SpatialMath.h:45` remains, kept for the major-version rule, per D-01); below-horizon VBAP pinned to ear 2.1.0 including the band |
| EXTR-03 | 02-02, 02-06, 02-07, 02-10 | Layouts, format registry and Ambisonics codec return real data; 23 formats, 15 layouts, encode/decode to order 6 | ✓ SATISFIED | SC3 and SC4 evidence; the decode pin (`[ambi-pin]`) now holds on the shipping build type as well |
| VERIFY-01 | 02-02, 02-06, 02-07, 02-10 | Ambisonics channel-order and normalisation convention stated (SpatialCore#11) | ✓ SATISFIED | SC1. REQUIREMENTS.md already shows `[x]` and "Complete". #11 itself was never closed on GitHub (only by commit-message reference). |

All three phase requirement IDs (EXTR-01, EXTR-03, VERIFY-01) appear in plan frontmatter and are accounted for. No orphaned requirements: REQUIREMENTS.md maps only EXTR-01, EXTR-03 and VERIFY-01 to Phase 2 (traceability rows 427, 428, 442) and ROADMAP.md lists the same three. 02-08 and 02-09 declare only EXTR-01; 02-10 declares VERIFY-01 and EXTR-03.

**Tracking defect: the 02-10 SUMMARY's "`requirements mark-complete` found no `EXTR-03` entry" is wrong about the cause, and it is the same defect the previous pass found for EXTR-01.** Resolved against REQUIREMENTS.md explicitly:

- The entry exists. Line 148 is `- [ ] **EXTR-03**: Layouts, format registry, and Ambisonics codec return real data`, in the same form as the `[x]` entries (line 138 is the matching `- [ ] **EXTR-01**`). The traceability row exists too: line 428, `| EXTR-03 | REQ-extract-speaker-layouts | 2 | Largely verified by tests |`.
- I read the tool (`~/.claude/gsd-core/bin/lib/milestone.cjs`, the `mark-complete` path, around lines 160-215). It flips the checkbox, then updates the traceability row only if the row's Status cell is `Pending` or `Gaps Found`. `Largely verified by tests` is neither, so the row write is rejected and, by design (#2788), the checkbox flip is rolled back. The tool then reports no hit, which reads as "no entry" but means "entry found, status text not flippable".
- Current state: both EXTR-01 and EXTR-03 are still `[ ]` with status "Largely verified by tests", although the evidence satisfies both. I did not edit REQUIREMENTS.md (the orchestrator owns tracking). Suggested edit: set both checkboxes to `[x]` and both traceability Status cells to `Complete` by hand, since the tool will not do it.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---|---|---|---|
| (files changed by 02-08 and 02-09) | | TBD / FIXME / XXX | none found | Debt-marker gate clear |
| `src/IO/SpeakerLayout.cpp` | 293-375 | WR-05: holes when the ear-level ring has an azimuth gap of 180 degrees or more (hull "away from origin" orientation) | ⚠️ Advisory | See below |
| `src/Core/SpatialMath.cpp`, `include/.../SpeakerLayout.h` | 243-256 / 30-52 | WR-02: wedge kind identified only by the implicit `nadirVertex >= 0 && nadirMask == 0` sentinel | ⚠️ Advisory | Hand-built triplets with `nadirMask == 0` would be reclassified; nothing enforces the encoding |
| `src/Core/SpatialMath.cpp` | 337-355 | WR-03: regular-only triplet list has no below-horizon coverage; Debug assert on the audio thread | ⚠️ Advisory (UAT test 3 accepted) | Reachable only by bypassing `RenderEngine` |
| `src/Algorithms/{VBAP,VBIP,MDAP}Algorithm.cpp` | jassert sites | Release fall-through to 2D pan on a height layout with empty triplets | ⚠️ Advisory | Hand-built contexts only, accepted under D-01 |
| `tests/Engine/RenderEngineTests.cpp` | 817-819 (also 824-832) | WR-06: `std::max` reductions drop NaN, so a NaN decode passes `[ambi-pin]` and the anchor | ⚠️ Warning | Reproduced (see the G-02-10 section). Bounded: `[io][ambisonics]` catches NaN from `getDecodeMatrix`. Not a blocker for any must-have. |
| `tests/Engine/RenderEngineTests.cpp` | 692-696, 715-782, 673-707 | IN-07 (the +0.1% catch is resolved on only 5 of 15 layouts), IN-08 (double helper has no `N <= M` guard and shares the float SH input), IN-09 (comment says 2.5x where the ratio is 2.3x, duplicated path sentence, indentation split unexplained) | ℹ️ Info | None affects a must-have |
| `tests/Algorithms/PanningLawTests.cpp`, `tests/IO/AmbisonicsCodecTests.cpp` | 65, 836 / 170 | Unqualified issue references (bare `#22` twice, `SpatialCore#11` without owner) | ⚠️ Warning | See "Cross-phase regression check". Cosmetic, they name SpatialCore's own tracker |
| (files changed by 02-10: `tests/Engine/RenderEngineTests.cpp`, `.gitignore`) | | TBD / FIXME / XXX | none found | Debt-marker gate clear |

### Code review cross-check

**Earlier review (G-02-2 round: 0 critical, 3 warnings, 2 info).** Unchanged by this pass: `src`, `include` and `tests/reference` have no diff since `a322673`, so WR-05, WR-02, WR-03, IN-05 and IN-06 stand as recorded (none contradicts a must-have; WR-05, WR-02 and WR-03 are follow-ups for hand-built layouts or a future minor version, see the disposition ledger).

**02-10 review (`02-REVIEW.md`, 0 critical, 1 warning, 3 info; findings renumbered WR-06, IN-07, IN-08, IN-09 past existing IDs).** Checked against the must-haves:

- **WR-06** is real and reproduced, see the G-02-10 section. It does not contradict a must-have (the truth is about the Release suite passing, the derivation, and the mutation proof, all verified), but it weakens the pin's own claim to catch a broken decode. Recommend the two-line fix before the phase is closed.
- **IN-07** is true: the +0.1% resolution is shown on 7.0, 7.1, 7.1.2, 7.1.4, 7.1.6, which is what the must-have required ("at least 3 failing per-layout pin checks"). My mutation gave 5 per-layout plus 5 anchor failures.
- **IN-08** is true and harmless today (15 speakers is the maximum, `N <= M`). The anchor tests the solve and its conditioning, not `evalSH`; SH values are pinned separately by `[sn3d]` (49 scipy literals, 153 assertions, passes in both builds).
- **IN-09** is true (comment hygiene).
- The disposition ledger (`02-REVIEW-DISPOSITION.md`) records all four as `open` (14 open of 15).

### Cross-phase regression check: Phase 1 criterion 4

Phase 1's verification used `grep -rhoE '[[:alnum:]/-]*#[0-9]+' include/ src/ tests/ | grep -cv 'OpenSpatialDelay#'` and recorded `0`. I re-ran it: it returns **11**. All 11 predate 02-10 (the 02-10 diff touches neither `include/`, `src/` nor those test files, apart from `RenderEngineTests.cpp`, which contains no `#NNN`).

| File:line | Text | Form |
|---|---|---|
| `include/SpatialCore/Core/SpatialMath.h:78` | `AndrewRahman/SpatialCore#11` | qualified |
| `include/SpatialCore/IO/AmbisonicsCodec.h:11` | `AndrewRahman/SpatialCore#11` | qualified |
| `include/SpatialCore/Algorithms/VBIPAlgorithm.h:19` | `AndrewRahman/SpatialCore#20` | qualified |
| `include/SpatialCore/Algorithms/DBAPAlgorithm.h:21` | `AndrewRahman/SpatialCore#10` | qualified |
| `src/Algorithms/VBIPAlgorithm.cpp:18` | `AndrewRahman/SpatialCore#20` | qualified |
| `tests/Algorithms/PanningLawTests.cpp:243, 553, 644` | `AndrewRahman/SpatialCore#22` | qualified (3) |
| `tests/IO/AmbisonicsCodecTests.cpp:170` | `SpatialCore#11` | owner missing |
| `tests/Algorithms/PanningLawTests.cpp:65, 836` | bare `#22` (`(F5, #22)`) | **bare** |

**Judgment: not a real regression of the criterion's intent, but a real drift in form on three sites, plus an obsolete check command.** Reasoning:

- The criterion is "every `#NNN` resolves in the tracker it names, OSD references written `Spatial-Media-Lab/OpenSpatialDelay#NNN`". Phase 1's purpose was to stop bare numbers that a reader would resolve in the wrong repo (the code had bare OSD issue numbers). Nine of the eleven are in the explicit `Owner/Repo#N` form, which is exactly the form the criterion asks for; they name SpatialCore's own tracker, which did not appear in code comments when Phase 1 ran, so the grep's "anything not OSD is a violation" proxy could not distinguish them. The check command, not the code, is what no longer fits.
- The three non-qualified sites are the real drift. All three refer to SpatialCore's own issues (#22 is the coplanar-tie issue filed in Phase 2 and its text was confirmed identical to the approved draft in UAT test 4; #11 is the Ambisonics convention issue, ROADMAP line 75 and REQUIREMENTS line 326). In context, `#22` is unambiguous: line 243 and 553 of the same file cite it qualified. But a bare `#22` in a repo whose comments also cite OSD issues is the exact pattern Phase 1 banned.
- I could not re-resolve the issues live (`gh` returns HTTP 401 this session). #22 and #11 are evidenced by UAT test 4 and the requirements file. #10 and #20 are cited by qualified references written in Phases 1 and 2 and were not re-resolved this pass.

**Classification: WARNING (advisory), not a blocker.** The drift came in with plans 02-05 and 02-08 (the bare `#22`) and 02-06 or earlier (`SpatialCore#11`), and Phase 2 did not re-run Phase 1's grep. Recommended fix, three one-token comment edits (`#22` to `AndrewRahman/SpatialCore#22` at PanningLawTests.cpp:65 and :836; `SpatialCore#11` to `AndrewRahman/SpatialCore#11` at AmbisonicsCodecTests.cpp:170) and amend the Phase 1 check to `grep -rhoE '[[:alnum:]/-]*#[0-9]+' include/ src/ tests/ | grep -cv 'OpenSpatialDelay#\|AndrewRahman/SpatialCore#'`, which would then return 0. This is not a Phase 2 goal item, so no gap is raised against Phase 2.

### Human Verification Required

#### 1. Accept the 25-parts-per-million bound for the decode check? (yes or no)

**Test:** None to run. 02-10 changed one test's allowed difference, between the library's Ambisonics decoder and a copy of the original decoder, from 1e-6 (one part per million) to 2.5e-5 (25 parts per million).
**Expected:** Answer yes or no. Yes keeps the new limit (the recommendation). The decoder in the library is unchanged, so nothing you hear changes. The old limit came from a measurement on a debug build and was never derived; the new one is worked out from the matrix maths, re-checked against a double-precision solve on every test run, and I reproduced that it still fails when the decoder is deliberately changed by 0.1%, in both Release and Debug. No means keeping one part per million and forcing identical rounding with a compiler setting on the whole library, which slows the audio code and means re-checking OpenSpatialDelay's sound; that needs a new decision before any change.
**Why human:** Loosening a test's limit is a trust judgment. The numbers are checked; the acceptance is yours. Nothing else is blocked on the answer.

#### 2. Judgment-tier prohibitions from 02-10 (4)

**Test:** Review the last four rows of the Prohibitions table.
**Expected:** Accept or reject. All four hold on the evidence.
**Why human:** Judgment-tier items are never silently absorbed into a pass. The 25 earlier prohibitions were already accepted in UAT tests 5 and 9.

**No longer open (closed by the user in UAT since the previous report):** the 5.1.4 rear-gap question (test 6, "yes, leave it as it is"), the 25 earlier prohibitions (tests 5 and 9), and the D-02a abort backstop (test 8, observed). **Deferred follow-up, not a gate:** the optional listening check of the corrected band (test 7, queued by the user for the next round of listening reviews).

### Gaps Summary

There are no gaps and no blockers. G-02-10 was the only open gap and it is closed: the full suite passes in a Release build on Apple Silicon (193/193 under ctest), `[ambi-pin]` passes in Release and Debug, and a deliberate +0.1% decoder change fails it in both, which I reproduced rather than took from the SUMMARY. Nothing regressed: the source diff since the last verification is one test file and one gitignore line, and every Phase 2 tag I ran passes in both build types. The only failing test in either build is the pre-existing Debug-only HUTUBS PP2 checksum, owned by Phase 3.

Status is `human_needed` because the user has one yes/no question open (the 2.5e-5 bound) and four flagged judgment-tier prohibitions to accept.

Follow-ups, none of which blocks the phase:

- **Recommended before closing the phase:** fix WR-06 (two-line `REQUIRE (std::isfinite (...))` per decode entry in `[ambi-pin]`, as written in 02-REVIEW.md). Otherwise the pin's claim to catch a broken decode has a known hole.
- Qualify the three unqualified issue references (PanningLawTests.cpp:65 and :836, AmbisonicsCodecTests.cpp:170) and amend the Phase 1 criterion 4 grep to exclude `AndrewRahman/SpatialCore#`.
- Update `02-UAT.md`: test 10 to pass, G-02-10 to resolved, status to complete.
- Update REQUIREMENTS.md by hand: EXTR-01 and EXTR-03 to `[x]` and `Complete` (the tool cannot, see the tracking note).
- Tick the `02-VALIDATION.md` rows for 02-08, 02-09 and 02-10 (still `pending`).
- Carry the 02-09 OSD follow-ups into the OSD repo.
- Optional hardening: IN-07 to IN-09, WR-05, WR-02/IN-06, WR-03, WR-04, and the Phase 6 CI items (arm64/FMA leg, CI's ctest step running zero tests).

---

_Verified: 2026-10-03T22:30:00Z_
_Verifier: Claude (gsd-verifier)_
