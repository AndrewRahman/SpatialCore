---
status: testing
phase: 02-algorithm-format-verification
source: [02-VERIFICATION.md]
started: 2026-10-01T07:55:50Z
updated: 2026-10-03T22:10:09Z
---

## Current Test

number: 11
name: Accept the 2.5e-5 bound for the [ambi-pin] decode check?
expected: |
  Answer yes or no. Yes (recommended): accept the 25-parts-per-million limit. The library decoder is unchanged, so nothing you hear changes. No: keep 1e-6 and force identical rounding with a library-wide compiler flag, which slows the audio code and means re-checking OpenSpatialDelay's sound.
awaiting: user response

## Tests

### 1. DR-3 cross-repo build and OSD follow-ups
expected: OSD 30391cd build accepted; ADMOSCReceiver.h warnings deferred; four follow-ups confirmed.
result: pass
source: claude-verified
evidence: |
  Re-ran 2026-10-01 against HEAD f6aac62: `cmake --build /tmp/osd-dr3-check/build --target OpenSpatialDelay OpenSpatialDelayTests`
  -> OSD BUILD EXIT 0; `git -C ~/conductor/repos/openspatialdelay-v1 status --porcelain` empty.
  ADMOSCReceiver.h untouched in Phase 2 (`git log d43cb15..HEAD` on it is empty) -> warnings pre-existing, deferred.
  Follow-ups 2-4 line refs confirmed at OSD 30391cd (PluginProcessor.cpp :2115 / :2147 / :2262 / :2327 / :2431).
amendment: |
  Follow-up 1 is incomplete. Besides docs/wiki/glossary.md:127-128, three more live OSD surfaces describe VBIP
  as squared gains: SPECIFICATION.md:491, docs/wiki/output-formats.md:88, .claude/skills/spatial-audio-dsp/SKILL.md:554
  (Archive/ and docs/VERSION_HISTORY.md are historical, leave as-is). Follow-up 1 covers all four.
  Follow-up 2 also gains a fifth audible change if the test 2 gap closes by changing the panning (user chose B).

### 2. Below-horizon 0 to -30 degree band versus EAR
expected: Decide (a) accept as the #22 triangulation defect and correct the docs (guide, README, skill; widen WR-01 tie scope), or (b) treat as a gap for a gap-closure plan. Probe: 7.1.4 az 60 — SpatialCore 0.7210/0.6929 (-5), 0.7472/0.6646 (-15), 0.6768/0.7361 (-25) vs EAR 0.7071/0.7071; max deviation 0.057 (~0.5 dB).
result: pass
reported: "b"
severity: major
source: claude-verified
evidence: |
  Re-probed 2026-10-01 (temporary Catch2 case, removed after): 7.1.4, az 60, VBAPAlgorithm via RenderEngine context.
  el 0: M+030 0.7071 / M+090 0.7071; -5: 0.7210/0.6929; -10: 0.7343/0.6788; -15: 0.7472/0.6646;
  -20: 0.7513/0.6599; -25: 0.6768/0.7361; -30: 0.6941/0.7199.
  PyPI ear 2.1.0 (.context/venv, bs2051 4+7+0 without LFE, point_source): 0.7071/0.7071 at every elevation 0 to -30.
resolution: |
  Fixed by gap-closure plans 02-08 (a0595dc, 582fa5a) and 02-09 (b4f0046, a322673), per the user's answer "b".
  Re-verified 2026-10-03 (02-VERIFICATION.md): 7.1.4 az 60 gives 0.7071 / 0.7071 on M+030 / M+090 at el 0, -5, -15, -25, -30
  via RenderEngine::setOutputFormat + VBAPAlgorithm::computeGains; az -60 mirrors on speakers 1/4. [g02-2], [band],
  [vbap3d-identity], [consumer-surface] pass; band matches ear 2.1.0 within 3e-7 on all 8 height layouts except 5.1.4
  behind the listener (test 6).

### 3. Residual silent degradation on hand-built LayoutContexts (WR-03)
expected: Confirm D-01/D-02 ("harden upstream") covers (1) empty triplets on a height layout falling to 2D pairwise pan in Release, and (2) builder-only contexts sending below-horizon sources to the largest-min-gain fallback; builder docblock fix is a follow-up, not a blocker.
result: pass
source: claude-recommended
reason: Only reachable by bypassing RenderEngine (outside SC-13); the engine path is covered. User accepted the recommendation with the test 2 answer; WR-03 docblock/entry-point fix stays a follow-up.

### 4. Process gates recorded only in SUMMARY narrative
expected: Confirm the delegated 'approved-recreate' (02-02) discharges the package gate, and that #22's text was approved before filing (02-03).
result: pass
source: claude-verified
evidence: |
  02-02: user's own delegation quoted in 02-02-SUMMARY ("You decide about 1 and 2 ... Pick your recommendations.").
  02-03: `gh issue view 22 -R AndrewRahman/SpatialCore` body is identical (after strip) to 02-03-ISSUE-BODY.md, the approved draft.

### 5. Judgment-tier prohibitions (17)
expected: Accept the verifier's non-binding judgments in 02-VERIFICATION.md "Prohibitions" (16 hold; 02-07 docs-match-code partly fails via test 2).
result: pass
source: claude-verified
evidence: Full suite re-run 2026-10-01 — 190 cases, 189 pass; only the pre-existing HUTUBS PP2 checksum (Phase 3) fails. The one partly-failing judgment is tracked as the test 2 gap.

### 6. Keep 5.1.4's rear-gap difference from EAR?
expected: |
  On a 5.1.4 setup, for a sound directly behind you (beyond 110 degrees either side), EAR also sends part of it to the
  two rear ceiling speakers, even at ear height. SpatialCore keeps it on the two rear ear-level speakers, so sounds at or
  above ear height are unchanged from today. The difference is written down in the integration guide, README and skill.
  Yes = keep it (recommended: matching EAR there would change the sound above ear height on 5.1.4 and reintroduce the
  0.71 horizon jump from RESEARCH F4). No = needs a new decision before any code changes.
result: pass
reported: "Then yes, leave it as it is"
evidence: |
  User first asked whether the change made horizon sounds reach height speakers on other layouts. Probed 2026-10-03
  (scratch Release build, HEAD 4df579c): 8 height layouts x 7,201 azimuths at el 0, max height-speaker gain
  VBAP <= 4.3e-8 (~-147 dB), VBIP <= 2.1e-4 (~-74 dB, sqrt of the same float noise). [vbap3d-identity] passes
  (bit-identical to pre-Phase-2 code at and above the horizon). Answered: no; the 5.1.4 rear-gap case is EAR's behaviour, not SpatialCore's.

### 7. Optional listening check of the corrected 0 to -30 degree band
expected: In OpenSpatialDelay on 7.1.4, slowly lower a sound from ear height to 30 degrees below at about 60 degrees left. It stays put, with no drift toward one speaker and no side flip. The numbers already prove it; this is only whether you like how it sounds. Skippable.
result: skipped
reason: "Deferred follow-up: I will check it in the next round of listening reviews. Just add it as a task for the next round."

### 8. Backstop: Release layout-build abort (D-02a)
expected: A broken height layout aborts in activateLayout (RenderEngine.cpp:729) during setOutputFormat, never from renderBlock. No Catch2 test can survive an abort; placement evidence only (unchanged by 02-08/02-09).
result: pass
source: claude-verified
evidence: |
  Exercised 2026-10-03 in a scratch worktree (/tmp/sc-backstop, HEAD 4df579c, never committed), Release build.
  Sabotaged buildVBAPTripletsForLayout to return no triplets for height layouts, then a scratch Catch2 case called
  engine.setOutputFormat(Surround7_1_4) on the test thread: SIGABRT, exit 134, the "returned" line never printed.
  Control (sabotage reverted, same probe): setOutputFormat returns, exit 0.
  The only std::abort in src/ is RenderEngine.cpp:729 (inside activateLayout); RenderEngine::renderBlock contains no
  activateLayout or abort call.

### 9. Judgment-tier prohibitions (25)
expected: Accept the verifier's non-binding judgments in 02-VERIFICATION.md "Prohibitions" (17 from before plus 8 from 02-08/02-09); all 25 hold on the evidence.
result: pass
source: claude-verified
evidence: |
  Re-checked the 8 new items 2026-10-03 (the first 17 were accepted in test 5):
  no change above the horizon: [vbap3d-identity] passes (262,104 assertions, Release build);
  no alloc/lock/log: `git diff 306b025..HEAD -- src/Core/SpatialMath.cpp` adds no new/malloc/vector/push_back/resize/mutex/lock/DBG/printf;
  public API: `git diff 306b025..HEAD -- include/` adds one blank line and comments only;
  no 5.1.4 special case: builder diff has no layout-specific branch;
  EarReference.h: regenerated from ear 2.1.0 (.context/venv) and byte-identical (cmp);
  OSD untouched: `git status --porcelain` empty; no GitHub post: issue #22 last updated 2026-10-01T06:35Z, 0 comments;
  02-09 scope: 1237af4..a322673 touches only SKILL.md, README (1 line), integration-guide, REVIEW-DISPOSITION, VALIDATION.

### 10. Full test suite passes in a Release build (the build type CI uses)
expected: All tests pass with CMAKE_BUILD_TYPE=Release, as .github/workflows/ci.yml builds and runs them (ubuntu-latest, Release, ctest).
result: pass
source: claude-verified
reported: "Found while running test 8: Release build of HEAD 4df579c, 193 cases, 192 pass, 1 fails: [ambi-pin] (RenderEngineTests.cpp:700, added in 317cc51 / 02-06). The order-3 decode matrix differs from the test's reference decoder by up to 5.3e-6 on 10 of 15 layouts (7.0/7.1 worst), tolerance is 1e-6. Debug passes. Phase gates ran Debug only; the branch has not been pushed (ahead 59), so CI has never run it."
severity: major
resolved_by: [02-10]
reverified: "02-10 re-verification (claude-verified): Release ctest --test-dir build-release/tests 193/193; [ambi-pin] 110 assertions pass in Release and Debug; a +0.1% Tikhonov-epsilon mutation still fails the pin in both builds. Debug 192/193 (HUTUBS PP2, pre-existing, Phase 3)."

### 11. Accept the 2.5e-5 bound for the [ambi-pin] decode check?
expected: Answer yes or no. Yes (recommended): accept the 25-parts-per-million limit in place of 1e-6. The library decoder is unchanged, so nothing you hear changes; the limit is derived from the matrix conditioning, re-checked against a double-precision solve every run, and still fails for a deliberate +0.1% decoder change. No: keep 1e-6 and force identical rounding with a library-wide compiler flag (slows the audio code; OpenSpatialDelay's sound needs re-checking).
result: [pending]
source: 02-VERIFICATION.md human_verification (02-10 SUMMARY question)

### 12. Judgment-tier prohibitions from plan 02-10 (4)
expected: Accept or reject the verifier's non-authoritative judgment that all 4 hold — (1) no change under src/ or include/ and referenceAmbiDecode untouched; (2) no -ffp-contract or other FP flag in either CMakeLists.txt; (3) the tolerance was derived, not tuned to pass; (4) nothing from the mutation worktree committed, and HUTUBS PP2, other tolerances, ci.yml and /tmp/sc-backstop untouched. Evidence: 02-VERIFICATION.md Prohibitions table.
result: [pending]
source: 02-VERIFICATION.md human_verification

## Summary

total: 12
passed: 9
issues: 0
pending: 2
skipped: 1
blocked: 0

## Deferred Follow-Ups

- test: 7
  idea: "Listening check in OpenSpatialDelay on 7.1.4: lower a sound from ear height to 30 degrees below at about 60 degrees left; confirm it stays put (no drift, no side flip). User: 'I will check it in the next round of listening reviews. Just add it as a task for the next round.'"
  deferred_at: 2026-10-03

## Gaps

- gap_id: G-02-2
  truth: "On height layouts, a source between 0 and -30 degrees elevation pans as ITU-R BS.2127 (EAR) does: the horizon pan holds (7.1.4 az 60 stays 0.7071/0.7071 on M+030/M+090), with no lean and no side flip — as the integration guide, README, skill and CONTEXT D-04 already claim"
  status: resolved
  resolved_by: [02-08, 02-09]
  reason: "User reported: b (treat as a gap and change the panning to match EAR rather than correct the docs)"
  severity: major
  test: 2
  root_cause: "appendLowerHemisphereTriplets emits each ear-level/-30deg trapezoid as all 4 overlapping triangles of a planar quad; computeVBAPGains3D's strict min-gain-sum selection then faces an exact (or 1-2 ULP) tie between two mirror-image triangles, so enumeration order and float noise pick the lean side. EAR pans each trapezoid as one QuadRegion, which collapses to the elevation-independent 2D pair pan at the source azimuth. No triangulation (fixed diagonal 0.055, blended 0.015-0.025) reproduces it; a pair-pan region per adjacent ear-level pair is exact (prototype residual <= 4.4e-7, above-horizon bit-identical on 259,920 points)."
  scope_note: "Applies to every lower-ring trapezoid on all 8 height layouts (max deviation today 0.055 / 1.23 dB; also below -30deg where wide-chord facets dip under it). Carve-out: 5.1.4's rear gap (|az| >= 111deg) — EAR there reaches the U+-135 height speakers even at el 0; matching it would break above-horizon bit-identity and reintroduce the RESEARCH F4 0.71 horizon jump. Orchestrator decision (per diagnosis recommendation, consistent with F4 lower-only hull and the 'no sound change above the horizon' constraint): keep the rear gap as is and document it as a deliberate departure. Issue #22 (above-horizon coplanar ties) stays separate and unfixed."
  artifacts:
    - path: "src/IO/SpeakerLayout.cpp"
      issue: "lines 257-339: appendLowerHemisphereTriplets emits 4 overlapping triangles per trapezoid (facet filter :298-311 keeps coplanar quads whole)"
    - path: "src/Core/SpatialMath.cpp"
      issue: "lines 279-316: single min-sum pass over a mixed lower list; :348-382 lower-hemisphere output mapping"
    - path: "include/SpatialCore/IO/SpeakerLayout.h"
      issue: "lines 20-43: VBAPTriplet — add a defaulted field only if the explicit-flag route is chosen (must stay default-constructible)"
    - path: "tests/IO/SpeakerLayoutTests.cpp"
      issue: "lines 360-364 pin nadirMask == earMask and nadirGain == 1/sqrt(earCount) for every lower triplet with nadirVertex >= 0; a zero-share wedge violates it"
    - path: "tests/Algorithms/PanningLawTests.cpp"
      issue: "band continuity bound loosened to 0.12 (3e97ad4); can tighten to the 0.01 cap bound"
    - path: "tests/reference/gen_ear_reference.py"
      issue: "band explicitly 'NOT pinned against ear'; EarReference.h can gain band cases (excluding 5.1.4 rear gap)"
    - path: "docs/integration-guide.md"
      issue: "lines 203-211 describe the band as plain EAR; plus README.md:23, .claude/skills/spatial-audio-dsp/SKILL.md:48 and :287"
  missing:
    - "Replace the 4 overlapping band triangles per adjacent ear-level pair with one pair-pan region (zero-nadir-share wedge triplet or explicit defaulted VBAPTriplet flag) whose gains equal the 2D pair pan at the source azimuth"
    - "Order the lower tiers in computeVBAPGains3D: regular triplets (unchanged, pass 0) -> nadir cap -> pair-pan wedges, with no min-sum tie among pair pans; no alloc/lock on the audio path"
    - "Keep [vbap3d-identity], DR-3 consumer-surface (signature, 4-member LayoutContext, default-constructible VBAPTriplet) and [ear][coverage]/[ear][continuity] green; update SpeakerLayoutTests:360-364 to the new representation"
    - "Pin band cases against ear 2.1.0 in EarReference.h (regenerated by gen_ear_reference.py, byte-identical) and tighten the band continuity bound; exclude and document 5.1.4's rear gap"
    - "Docs: guide/README/skill state the band now matches EAR exactly except 5.1.4's rear gap; widen WR-01 note that #22 remains above the horizon"
    - "Record OSD follow-up updates: follow-up 1 covers 4 OSD files (glossary.md:127-128, SPECIFICATION.md:491, docs/wiki/output-formats.md:88, .claude/skills/spatial-audio-dsp/SKILL.md:554); follow-up 2 gains a fifth audible change (below-horizon band now EAR-exact)"
  debug_session: .planning/debug/below-horizon-band-vs-ear.md

- gap_id: G-02-10
  truth: "The full test suite passes in a Release build, the build type CI uses (.github/workflows/ci.yml)"
  status: resolved
  resolved_by: [02-10]
  reason: "Claude-verified: [ambi-pin] fails in Release only; decode matrix vs reference decoder max |diff| 5.3e-6 > 1e-6 tolerance on 10 of 15 layouts. Debug passes."
  severity: major
  test: 10
  root_cause: "Not a decoder defect. [ambi-pin] asserts near bit-exactness (1e-6) between two separately compiled float copies of the same decoder. At -O3 on arm64, clang's default -ffp-contract=on fuses the E*E^T accumulation (AmbisonicsCodec.cpp:69-70 and the same loop in referenceAmbiDecode) into fmuladd; the vectorizer keeps only the scalar tail fused, and the two TUs vectorize differently (runtime M / [49] arrays vs constant M=16 / [16][16]), so the fused/unfused term mix depends on speaker count N (equal only for N=8,9,11). -O0 is bit-identical. E*E^T is singular (rank <= 15 of 16); with the 0.01 Tikhonov term cond is about 500-1800, so the honest float noise floor is about 5e-6 to 2e-5 — above the 1e-6 bound, which came from RESEARCH F10's -O0 measurement ('worst 0'). Proven: -ffp-contract=off on both TUs, or no vectorization, gives 0 difference; per-loop pragma isolates the E*E^T loop."
  scope_note: "CI (ubuntu-latest x86-64, no FMA) likely passes — reasoned plus clang x86_64 proxy, GCC not run; fails on Apple Silicon Release (developer machine, arm64 slice of shipped builds). Fix the test bound, not the library or the verbatim reference body. Separate, not this gap: HUTUBS PP2 golden hash fails in Debug only, passes in Release (same FP class, Phase 3); other tight test-local-reference tolerances listed in the debug session as at-risk candidates (none fail today)."
  artifacts:
    - path: "tests/Engine/RenderEngineTests.cpp"
      issue: "~700 and ~706: CHECK(worstHere <= 1e-6f) and the all-layout bound sit below the float noise floor of a Tikhonov-regularised Gauss-Jordan solve; referenceAmbiDecode (574-658) is a verbatim transplant and must stay verbatim"
    - path: "src/IO/AmbisonicsCodec.cpp"
      issue: "65-72: origin of TU-dependent rounding under FP contraction + vectorization; no defect, no change required"
    - path: "CMakeLists.txt"
      issue: "164-180 documents the per-TU FP hazard class (plan 08-02) but sets no -ffp-contract policy; informational"
    - path: ".github/workflows/ci.yml"
      issue: "Linux x86-64 Release only; no arm64/FMA leg, and phase gates ran Debug only"
  missing:
    - "Replace the 1e-6 bounds in [ambi-pin] with an honest, derived float bound (about 5e-5 fixed, or per-layout k*2^-24*cond*max|D|, or assert each decoder against a double-precision decode); record the derivation beside the number and fix the test comment that cites RESEARCH F10"
    - "Keep referenceAmbiDecode byte-identical and the library decoder unchanged (pure-refactor pin)"
    - "Prove it: [ambi-pin] passes in both Debug and Release (macOS arm64), and the bound still fails for a deliberate +0.1% Tikhonov-epsilon change"
    - "Recurrence guard: the phase gate runs the suite in a Release build too (document in VALIDATION / gate notes); an arm64 CI leg is optional and out of scope unless cheap"
  debug_session: .planning/debug/ambi-pin-release-tolerance.md
