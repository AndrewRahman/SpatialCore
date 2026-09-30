# Phase 2: Algorithm & Format Verification - Research

**Researched:** 2026-09-30
**Domain:** Real-time C++17/JUCE 9 spatial-audio DSP — VBAP-family panning laws, ITU-R BS.2127 lower-hemisphere handling, real spherical harmonics (ACN/SN3D), Catch2 verification
**Confidence:** HIGH for everything verified by running the real code or an independent oracle this session; MEDIUM for design proposals (prototyped in scratch code, not yet in the repo)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

### Triplet Fallback Disposition (EXTR-01, criterion 2)

- **D-01:** **Delete the empty-triplets nearest-speaker branch** at `VBAPAlgorithm.cpp:19`,
  `VBIPAlgorithm.cpp:20`, `MDAPAlgorithm.cpp:48` and `:101`. Harden upstream instead of patching in
  place. `jassertfalse` is Debug-only, so today a Release build silently dumps all energy into one
  speaker. The branch is structurally unreachable with shipped layouts: every height layout builds
  25-355 triplets (5.1.2 = 25, 7.1.4 = 118, 9.1.6 = 355).

- **D-02:** **The layout-build guard is a test plus an unconditional Release crash.** The empty-triplets
  condition can only be caused by a SpatialCore developer editing the layout table or the triplet
  builder: `OutputFormat` is a closed enum, `activateLayout` is private, and there is no
  custom-layout entry point into the engine. So:
  (a) `RenderEngine::activateLayout` crashes unconditionally in Release when a height layout yields
  no triplets, replacing the Debug-only `jassert` at `RenderEngine.cpp:638`. This is the
  layout-build path (message/prepare thread, allocates via `push_back`), so **DR-1 does not apply**.
  (b) `setOutputFormat` stays `void`. No API change.
  (c) The existing Catch2 check `tests/IO/SpeakerLayoutTests.cpp:221-235` ("height layout ⇔
  non-empty triplets") is the primary catch; the crash is the backstop.

- **D-03:** **Unify the mismatched height thresholds into one shared constant.**
  `layoutHasHeight` uses `0.0175f` (`SpeakerLayout.cpp:94`); `buildVBAPTripletsForLayout` uses
  `0.01f` (`SpeakerLayout.cpp:111`, `:117`). This is a latent defect that trips the `:638` guard for
  any speaker elevated 0.57-1.0 degrees. Fixing it here is required: D-02(a) would otherwise turn a
  latent mismatch into a Release crash.

### Below-Horizon Sources (EXTR-01, criterion 2)

- **D-04:** **Below-horizon sources on layouts with no lower speakers use the ITU-R BS.2127
  (EBU ADM Renderer / EAR) lower-hemisphere construction.** This is a *second* silent nearest-speaker
  fallback, inside `computeVBAPGains3D` (`src/Core/SpatialMath.cpp:272-289`), and it **is
  user-reachable**: the OSD elevation parameter spans -90..90, ADM-OSC `/elev`, `/aed` and `/xyz`
  arrive with no clamp (`ADMOSCReceiver.cpp:62-77`), and the Bounce/Cross/Helix/Random trajectories
  go below the horizon (`TrajectoryEngine.cpp:188`, `:222`, `:294`). MDAP hits it even at el=0,
  because 3 of its 8 aux directions fall below the horizon. Measured today: a 0.72 gain jump across
  the horizon, speaker flip-flopping, and an arbitrary speaker at -90.
  Construction:
  - Add virtual speakers at -30 degrees under the ear-level ring, per `ear/core/point_source.py`
    `extra_pos_vertical_nominal`.
  - Add a virtual nadir.
  - Downmix the virtual gains 1/sqrt(n) to the real speakers, then power-renormalize.
  Behaviour: the horizon pan holds down to -30 degrees, then blends to equal gain on all ear-level
  speakers at -90. Example, 4+7+0 at az 30: el -60 gives 0.83 main / 0.228 others; el -90 gives
  0.378 on all.
  **Precomputed in `LayoutState` at layout-build time; the audio thread stays alloc-free (DR-1).**
  Numeric oracle: PyPI `ear` 2.1.0.
  User chose this after asking for research first (BS.2127/EAR, MPEG-H, SAF, AllRAD, Atmos, clamp,
  plus numerical prototypes); see the discussion log.
  — **Reversibility:** costly — it changes what OSD sessions with below-horizon sources sound like,
  and any golden vector pinned against the new behaviour would have to be regenerated.

- **D-05:** **The EAR construction applies only to the speaker-layout VBAP family** (VBAP, VBIP and
  MDAP, all via `computeVBAPGains3D`). Binaural and Ambisonics keep the true negative elevation, so
  the HRTF and SH paths are untouched. This is an audible change for OSD sessions with
  below-horizon sources, and it is a fix.

### Playback Safety (EXTR-01)

- **D-06:** **Playback never crashes on an unplaceable position.** The stress test (~400M points per
  layout across 8 height layouts x {nadir-only, EAR}: 10M random, adversarial edge points, the 0.1
  degree OSD grid, and the ADM-OSC xyz path) produced 0 fallback hits for finite input. The margin
  is measured, not proven: observed error ~3.7e-7 against a 1e-6 tolerance, and the a-posteriori
  bound reaches 2.6e-6 on 7.1.6/9.1.6. NaN or +-inf az/el **always** reaches the fallback, and
  nothing sanitizes it upstream (`RenderEngine::computeObjectGains` passes `degreesToRadians`
  straight through, `RenderEngine.cpp:131-135`). Therefore:
  (a) `isfinite`-sanitize az/el at engine entry and hold the last good position.
  (b) Replace the snap with the best (largest-min-gain) triplet, negatives clamped to 0,
  renormalized. Bit-identical wherever a triplet is found today; worst-case deviation ~3e-6 (-110 dB).
  (c) `jassertfalse` in Debug only.

- **D-07:** **Do NOT widen the triplet tolerance to -1e-5.** It flips the triplet choice in 38-111
  per 1M directions, with gain jumps up to 0.96. D-06(b) is the safety net; the tolerance stays
  where it is.

### Spherical Harmonics (VERIFY-01, criterion 1, SpatialCore#11)

- **D-08:** **One SH implementation: `evalSH` is the single source of truth and receives the SN3D
  fix.** `AmbisonicsCodec::evaluateSH` (`src/IO/AmbisonicsCodec.cpp:13`, declared
  `AmbisonicsCodec.h:23`) becomes a one-line forwarder, so both public names survive (DR-3).
  Background: `evalSH` (`src/Core/SpatialMath.cpp:10`, declared
  `include/SpatialCore/Core/SpatialMath.h:71`) is on the shipping path (`AmbisonicsAlgorithm.cpp:30`,
  `RenderEngine.cpp:450` and `:662`), while `AmbisonicsCodec::evaluateSH` is what the 7 codec tests
  cover. The two are bit-identical (10k directions, 0 differences), so **both** carry the same bug.
  **Confirmed bug**, against an independent scipy reference on 20k directions using the real code:
  22 of 49 channels at orders 4-6 have the wrong constant scale (shape and sign are correct; orders
  0-3 are exact).
  - Wrong: ACN 16 and 24 (x3); 25 and 35 (x4); 26 and 34 (x3); 27 and 33 (x2 sqrt 2); 28 and 32
    (x sqrt 2); 36 and 48 (x4 sqrt 2); 37, 38, 46, 47 (x4); 39 and 45 (x2 sqrt 6); 40 and 44
    (x sqrt 2); 41 and 43 (x2).
  - Correct: ACN 17-23, 29-31, 42.
  Affects 4OA-6OA output (`RenderEngine.cpp:450`). The order-3 speaker decode is unaffected.
  Audible change: 4OA-6OA output becomes correct.
  — **Reversibility:** costly — the corrected scale is the AmbiX contract that 4OA-6OA consumers decode against.

- **D-09:** **Delete the private duplicate `RenderEngine::computeAmbiDecodeForLayout`**
  (`RenderEngine.cpp:649-745`: hard-coded M=16, Tikhonov eps=0.01, never tested).
  `activateLayout` calls `AmbisonicsCodec::getDecodeMatrix` at order 3 and adapts the flat layout
  into `ambiDecodeMatrix[MAX_SPEAKERS][MAX_SPEAKERS]`. This runs off the audio thread, so it is
  DR-1-clean. **A test pins the new output to the old within float tolerance**, so the swap is
  provably behaviour-preserving.

- **D-10:** **The stated convention is ACN channel order, SN3D normalisation, no Condon-Shortley
  phase, radians, az=0 at front, +az toward +Y.** This is what the code does and what
  `OutputFormatRegistry.cpp:39-45` already advertises ("AmbiX ACN/SN3D"). Criterion 1 allowed "ACN/SN3D or FuMa"; the
  code is ACN/SN3D. It must be stated in code and docs, not only in this file.
  — **Reversibility:** one-way — this is the published channel layout consumers and hosts decode
  against.

### Ambisonics Round-Trip (EXTR-03, criterion 4)

- **D-11:** **Criterion 4 is satisfied by a trio of tests, not by the round trip alone:**
  (a) The encode -> decode -> re-encode round trip as a **decoder-conditioning smoke test only**.
  (b) The **SN3D addition-theorem invariant**: per order l, sum over m of Y_lm(dir)^2 = 1, at
  orders 1-6. The buggy code deviates by up to 19.5.
  (c) **49 literal reference values at az=64, el=10** (min |Y| = 0.072 across all 49 channels),
  generated by scipy, tolerance ~1e-5. This catches the scale bug plus m/-m swap, sign flip, az
  reversal, el reversal and Condon-Shortley phase, none of which (a) or (b) can see.

- **D-12:** **A mode-matching round trip is not evidence of correctness, and this document records
  it so nobody re-proposes it as sufficient.** Verified numerically: encode -> decode -> re-encode
  (and the energy-vector direction on that decode) is blind to per-channel scale bugs because
  pinv(S*Y) = pinv(Y)*S^-1. It passes on today's buggy code at every order (coefficient error
  1.8e-14 with pinv, 5.06e-4 with Tikhonov, identical buggy versus fixed). Criterion 4 as literally
  worded in ROADMAP would have been green on the broken code.

### Panning-Law Verification (ROADMAP overview line 27)

- **D-13:** **"Verified against the panning laws" means behaviour checks for all 8 algorithms, plus
  textbook values where a published formula exists.** Existing golden vectors are "computed against
  pre-move OSD formulas" (`tests/Algorithms/SpatializationAlgorithmTests.cpp:12-21`, `:179-184`);
  they pin regressions, not correctness. No tests exist for on-speaker unity, height layouts,
  sweeps or energy. Add, for every algorithm: on-speaker unity, sum g^2 = 1 across a sweep, L/R
  mirror symmetry, a 360 degree continuity sweep with no jumps, and height-layout coverage including
  the new below-horizon behaviour (D-04).
  **Textbook values** for VBAP, VBIP, DBAP and MDAP. **KNN and ConstantPower** have no published
  law and get **property checks only**.
  Candidate textbook values (**the planner must verify these independently before pinning them**):
  - VBAP: Quad az30 -> 0.9659 / 0.2588; 5.0 az10 -> C 0.8917 / L30 0.4527; 5.0 az50 -> L30 0.9301 /
    Ls110 0.3673.
  - VBIP (D-14): Quad az30 -> 0.8881 / 0.4597; 5.0 az10 -> 0.8144 / 0.5803; 5.0 az50 -> 0.8467 /
    0.5321; invariant: sum g^2 * l points at the source.
  - DBAP at SpatialCore's effective R=12.04 dB (D-16), Quad az30, speaker order 45, 135, -135, -45
    -> 0.9984, 0.0270, 0.0173, 0.0459.
  - MDAP: assert spread=0 equals VBAP.

- **D-14:** **VBIP is fixed to textbook VBIP.** Current code squares the normalized VBAP gains
  (`VBIPAlgorithm.cpp:30`; header comment "squared gains for tighter focus"), giving 0.9974 / 0.0716
  at Quad az30, with the intensity vector aimed at 44.7 degrees instead of 30. Textbook is
  g_i = sqrt(G_i / sum G_i), with G = L^-1 p over the same pair/triplet and sum g^2 = 1, i.e.
  **VBAP gains raised to exponent 1/2, then renormalized.** Primary source, verified: Pernaux,
  Boussard & Jot, DAFx-98, Sec. 2.2.2. The user's words: "keep VBIP as the original intended VBIP
  or we're lying."
  Audible change: OSD sessions using VBIP (a user-selectable menu entry, index 6) become **wider**.
  **Cross-repo follow-up:** OSD glossary lines 127-128 ("squares the gains, producing an even tighter
  spatial image") must be corrected in the OSD repo.
  — **Reversibility:** costly — existing OSD sessions that chose VBIP change sound, and the
  golden vectors for VBIP are regenerated.

- **D-15:** **VBIP is single-band: the textbook gains at all frequencies, "for now because it's
  honest."** The paper is dual-band (VBAP below 700 Hz, VBIP above 700 Hz). Docs state that pairing
  and state that SpatialCore implements only the VBIP half. Dual-band needs a per-object crossover in
  `RenderEngine` and cannot be expressed through the frozen `computeGains` (DR-2/DR-7); it is
  tracked in SpatialCore#20 (see Deferred).

- **D-16:** **DBAP is documented as-is, not changed.** Effective R is about 12.04 dB (a=2, w=1/d^2
  used as amplitude), no spatial blur r_s, d^2 clamped >= 0.001, no hull projection. Tests assert the
  Lossius et al. ICMC 2009 formula evaluated at R=12.04 dB, so the docs and the test agree with the
  code rather than with an idealized DBAP.

- **D-17:** **The MDAP citation is corrected to Pulkki, "Uniform spreading of amplitude panned
  virtual sources", IEEE WASPAA 1999.** The code says 2000 at `MDAPAlgorithm.cpp:8` and
  `tests/Algorithms/SpatializationAlgorithmTests.cpp:271`. Comment-only.

### Claude's Discretion

- **Tie `kLayoutExpectations` to `NUM_LAYOUT_DEFS`.** `tests/IO/SpeakerLayoutTests.cpp:87` is a
  hand-written 15-row golden table not tied to the layout count. Add a `static_assert` or a size
  check. The user said fold it in; do not ask. Criterion 3 is otherwise already met.
- **`nearestSpeaker3DFallback` in the public header.** It is an `inline` function in
  `include/SpatialCore/Core/SpatialMath.h:40-66`. After its 4 call sites are gone (D-01), removing it
  is a removed public function, which is a major-version bump under the CLAUDE.md versioning rule.
  **Default: keep it in the header with a deprecation comment and remove it at the next major.** No
  consumer calls SpatialCore's copy: OSD main has its own local copy, and OSD 30391cd defines its own
  at `PluginProcessor.cpp:2115`.
- **Virtual-speaker storage.** `MAX_SPEAKERS = 16`, and the EAR construction on 9.1.6 needs 15 real +
  9 virtual (-30 degree ring) + 1 nadir = 25. Virtual speakers **must not consume real speaker
  slots**. Size the triplet, scratch and downmix storage separately, preallocated in `LayoutState`.
- **Performance of the EAR extras.** They inflate the all-triples list (9.1.6: 448 up to 2119
  triplets, ~2.6 us per call). Filtering to convex-hull facets gives ~86 triplets and ~0.12 us with
  no coverage loss below the horizon. Leave the existing upper-hemisphere triplets untouched, so
  behaviour above the horizon does not change.
- **Lower hemisphere only.** Do not add EAR's virtual zenith: testing showed no upper-hemisphere
  coverage gaps without it. Also skip the *upper*-layer branch of `extra_pos_vertical_nominal`. That
  branch adds +45 degree copies of some mid speakers (±135 on 7.1.2, 180 on SML13.1) and would change
  panning above the horizon. D-04 adopts only the -30 degree and nadir additions.
- **Where "hold last good position" state lives** (D-06a): per-object, preallocated.
- **Where the ACN/SN3D/no-CS statement lands** (D-10): `evalSH` and `AmbisonicsCodec.h` docblocks
  plus docs.
- **Where the scipy generator script lives** (D-11c): checked in, not run in CI. Exact tolerances are
  the planner's call.
- **DirectBinaural:** property checks. Woodworth ITD/ILD magnitude checks are optional.

### Deferred Ideas (OUT OF SCOPE)

- **SpatialCore#20** (created during this discussion; cross-linked to #10, the SPRD-01 spread API, v2):
  per-source focus/spread control; option to unify VBAP+VBIP into one VBAP with a focus exponent
  (p=0.5 VBIP, p=1 VBAP, p=2 old behaviour; the count goes 8 to 7 and OSD session migration is
  needed); Pernaux dual-band VBAP < 700 Hz / VBIP > 700 Hz (needs a per-object crossover in
  `RenderEngine`, cannot be expressed via frozen `computeGains`).
- **Tie-break discontinuity between overlapping triangulations of coplanar quads.** Pre-existing
  min-gain-sum tie-break (the +-45/+-135 height ring; ear/-30 trapezoids under EAR): 4-13 per 1M
  directions switch triplet, with gain jumps up to 0.70. Separate ticket, **not yet filed**.
- **DBAP extensions:** spatial blur, a user rolloff parameter, and convex-hull projection for sources
  outside the speaker hull (relates to #10).
- **VBAP 2D degenerate guard** for speaker gaps >= 180 degrees. No shipped layout has one.
- **MDAP on 2D layouts:** the vertical aux points collapse onto the source azimuth, so the main
  direction is effectively weighted ~3/9. Noted, not changed.
- **Higher-order speaker-layout Ambisonics decode.** It stays fixed at order 3; higher order would be a
  new capability.
- **OSD repo follow-ups** (not this repo's work, but caused by it): correct the OSD glossary lines
  127-128 (D-14), and tell OSD users in release notes about the audible VBIP/EAR/4OA-6OA changes.
  Saved sessions need no data migration, because algorithm indices and format enums do not change.
- **Removal of `nearestSpeaker3DFallback`** at the next major version (Discretion).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| EXTR-01 | Every public algorithm computes correct gains; the 3D triplet fallback is eliminated or fails loudly | D-01/D-02/D-03 (delete branches, layout-build guard, one threshold), D-04/D-05 (EAR lower hemisphere — §Architecture Patterns 1-2, oracle vectors in §Reference Data), D-06 (best-triplet + non-finite guards — Pattern 3, F7), D-13..D-17 (panning-law tests — §Reference Data A, F1-F5). Measured coverage: 0 fallback hits in 5.2M points x 8 height layouts. |
| EXTR-03 | Layouts, format registry and Ambisonics codec return real data; Ambisonics encodes/decodes to order 6 | Criterion 3 is already green (cross-table characterisation run: 23/23 formats resolve, 0 mismatches — §Reference Data D). Criterion 4 = D-11 trio; the 22-line evalSH fix is in §Reference Data B (verified against two independent scipy routes, max error 2.1e-6). |
| VERIFY-01 | Ambisonics channel-order / normalisation convention is stated (SpatialCore#11) | D-08/D-10: convention verified = ACN, SN3D, no Condon-Shortley, az=0 front, +az toward +Y (left). Doc/code landing sites enumerated in §Architecture Patterns 6. |
</phase_requirements>

## Summary

The phase is a bounded set of behaviour fixes plus a test-and-documentation layer. Every locked decision is implementable as written, and I independently re-derived the numbers CONTEXT asked me to verify. **Every D-13 VBAP and VBIP candidate value is confirmed** (to 6 digits, numpy, real speaker positions parsed from `SpeakerLayout.cpp`). The DBAP values are confirmed **but CONTEXT lists them in the wrong speaker order** (F2). The D-08 diagnosis is confirmed exactly: 22 of 49 channels, on 22 specific lines of `evalSH`, all with exact algebraic ratios; the corrected constants reproduce scipy to 2.1e-6 in float32.

Running the real code turned up **thirteen findings that CONTEXT did not have** (§Critical Findings). The five that change what the planner must write: (F1) the *current* VBIP output is `0.9330127 / 0.0669873`, not `0.9974 / 0.0716` — the existing code's "normalise to constant power" is a no-op, so VBIP is also up to 3 dB quiet today; (F3) "MDAP spread=0 equals VBAP" cannot be tested through the public API; (F4) copying ear's *full* convex hull faithfully would introduce a 0.71 gain jump at the horizon on 5.1.4 — a lower-hemisphere-only hull avoids it and still matches the ear oracle exactly in the nadir-cap region; (F5) the existing all-triples/min-sum triplet selection has *exact* coplanar ties, so 3D VBAP has 0.45-0.85 gain jumps on ordinary 0.1-degree azimuth sweeps at el=20/40 — the D-13 "360-degree continuity sweep, no jumps" cannot be asserted on height layouts off the horizon; (F7) OpenSpatialDelay computes its own gains through `algo->computeGains`, so the D-06(a) sanitiser at `renderBlock` does **not** protect it, and the 2D VBAP path has an **infinite loop** on `+-inf` and on any azimuth of about 1e9 rad or more.

**Primary recommendation:** Implement D-04 as *(regular triplets, unchanged) + (flagged lower-hemisphere triplets built from the hull of ear-level speakers, their -30 degree copies and a nadir)*, stored in the same `LayoutState::vbapTriplets` vector, selected in a regular-first two-pass loop inside `computeVBAPGains3D` — no `LayoutContext` change, no new allocation, bit-identical above the horizon. Put the non-finite/huge-angle guard in the algorithm layer *and* the engine layer. Scope the D-13 continuity/mirror tests to where the algorithms are actually continuous (§Validation Architecture) and file the tie-break ticket with the measurements below.

## Critical Findings (things CONTEXT did not know — read before planning)

| # | Finding | Evidence | Plan impact |
|---|---------|----------|-------------|
| F1 | **Current VBIP is `0.9330127 / 0.0669873` at Quad az30, not `0.9974 / 0.0716`, and its output power is not 1.** The code squares the (already L2-normalised) VBAP gains, then divides by `sqrt(sum of the squared values)`, which equals `sqrt(sum g^2) = 1` — a no-op. So output power is 0.50 at az0, 0.875 at az30, 1.0 only on a speaker. The 44.7 degree intensity-vector claim is still true (the ratio is the same). `0.9974/0.0716` is what "square, then L2-normalise" *would* give. | `[VERIFIED: src/Algorithms/VBIPAlgorithm.cpp:28-40]` `outputGains[s] = outputGains[s] * outputGains[s]; sum += outputGains[s];` then `float scale = 1.0f / std::sqrt (sum);`. Existing golden passes today: `[VERIFIED: tests/Algorithms/SpatializationAlgorithmTests.cpp:215-216]` `CHECK_THAT (gains[0], WithinAbs (0.9330127f, 1e-5f));` / `0.0669873f`. `./build/tests/SpatialCoreTests "[vbip]"` -> "All tests passed (4 assertions in 1 test case)". Sweep of the real code: VBIP power range `[0.5000,1.0000]` on all 2D layouts, `[0.33,0.98]` off-ring on height layouts. **Provenance:** squaring is verbatim OSD v1.0 (`git log -S`: `d95f17c` "Populate stubs with real implementations from OpenSpatialDelay v1.0"); the golden was added by `d745cad` "computed against pre-move OSD formulas". | D-14's audible change is *larger* than stated: OSD VBIP users also get up to +3 dB (0 dB on-speaker, +3 dB between speakers). The VBIP golden changes to `0.8880738 / 0.4597008`. Release notes must say "level" as well as "width". |
| F2 | **DBAP candidate vector is in the wrong speaker order.** CONTEXT lists Quad speaker order `45, 135, -135, -45`; the code's order is `45, -45, 135, -135`. Values are right, order is not. | `[VERIFIED: src/IO/SpeakerLayout.cpp:21]` `{ 4, -1, 4, {{ 45,0,0}, {-45,0,1}, { 135,0,2}, {-135,0,3}} },`. Recomputed: `[0.9984304, 0.0459007, 0.0270259, 0.0173052]` in code order. | Pin index 1 (`-45`) = 0.0459007, index 2 (`135`) = 0.0270259, index 3 (`-135`) = 0.0173052. Also: the **existing** DBAP golden at dist 0.5 (`0.9390509, 0.2691336, 0.1768006, 0.1203831`) is reproduced to 7 digits by the independent Lossius formula at R=12.04 dB — it is already a textbook check; only its comment needs to say so. |
| F3 | **"MDAP: assert spread=0 equals VBAP" is untestable.** There is no spread parameter: the ring angle is `alphaDegs = 0.9 * (180 / max(1, numSpeakers))` clamped to `[5, 30]` degrees, never 0 (SPRD-01 / #10 is deferred). | `[VERIFIED: src/Algorithms/MDAPAlgorithm.cpp:20-21]` `float alphaDegs = 0.9f * (180.0f / static_cast<float> (std::max (1, numSpeakers)));` `alphaDegs = juce::jlimit (5.0f, 30.0f, alphaDegs);`. Also MDAP's ring start phase moves gains by ~1% (`[0.94781 0.31326 0.0593]` vs `[0.95013 0.30735 0.05276]` for a 22.5 degree phase shift), so there is no phase-independent textbook value. | Replace with: (a) a **numpy port** of the construction (verified equal to the real C++: Quad az30 `0.94781 0.31326 0.05930 0.0`, 5.0 az10 `0.64130 0.29993 0.70519 0.03849 0.0`) pinned at tolerance 5e-4, labelled "cross-check, not independent oracle"; (b) properties: unit power, mirror symmetry (2D layouts), wider than VBAP, **on-speaker source does not give unity** (spread by design). |
| F4 | **A faithful copy of ear's full-sphere hull breaks horizon continuity on sparse-rear layouts.** On 5.1.4 (`ear-level 30,-30,0,110,-110`; uppers at +-45/+-135) ear's hull has a facet joining the +-135 uppers straight down to the -30 degree copies of +-110, skipping the ear-level speakers; a source at az -142.7 el -2.3 is panned 0.21/0.51 onto height speakers. Using the hull of *ear-level speakers + their -30 copies + nadir only* removes it. | Prototype, real layouts, both variants: horizon jump (max over az of `|g(el=+0.05) - g(el=-0.05)|`): lower-only hull 0.0018-0.0028 on all 8 height layouts; full hull **0.7066 on 5.1.4**, 0.0016-0.0028 elsewhere. Region check vs ear (lower-only hull, 8 height layouts x 3000 directions uniform in solid angle over the lower hemisphere): in ear's nadir-cap region (`VirtualNgon`, 1120-1484 directions per layout) max |diff| = 3.3e-16; in ear's `QuadRegion` band <= 0.0551 on 7 layouts; on 5.1.4 the differences are 0.558 (ear `QuadRegion`) and 0.387 (ear `Triplet`) — exactly the rear-gap facets that reach up to the height speakers and that the lower-only hull deliberately does not copy. | Adopt the lower-only hull. D-04's text ("virtual -30 ring, virtual nadir, 1/sqrt(n) downmix, power-renormalise") is unchanged; only the facet source set is specified. |
| F5 | **Existing 3D VBAP is discontinuous in the upper hemisphere and its tie-breaks are decided by float noise.** Coplanar quads (the mid-to-height side quads, e.g. rear `135/-135` ear-level + `135/-135` height on 7.1.4) produce *exactly equal* gain sums for both triangulations (`1.325751162` vs `1.325751162`), so `sum < bestGainSum` is decided by rounding; the winner flips every ~0.1 degree across a wide band while the two candidates' gains differ by up to 0.6. | Real-C++ 0.1-degree azimuth sweep (`.context/research/s_sweep.cpp`): VBAP max step 7.1.4 el=20 **0.607**, el=40 0.751; 5.1.4 el=40 **0.851**; 9.1.6 el=20 0.716 (ear-level el=0: 0.0053 everywhere). Numpy replica: 129 of 3600 steps flip at 7.1.4 el=20; tie printed to 9 decimals. Also KNN jumps 0.03-0.54 (inherent k-nearest switching) and MDAP at el=0 on height layouts jumps 0.16-0.19 (the below-horizon aux directions — this is the D-04 defect and should shrink). | (1) D-13 "360 degree continuity sweep with no jumps" can only be asserted for: 2D layouts at el=0 for every algorithm except KNN; height layouts at el=0 for VBAP/VBIP; ConstantPower, DBAP and Ambisonics anywhere. (2) Golden/mirror tests on height layouts must avoid the tie region (use a self-filtering helper — §Validation Architecture). (3) Cross-platform golden risk: a value pinned inside a tie region can differ between clang and gcc. (4) **File the tie-break ticket now** (CONTEXT says "not yet filed"); it is far larger than "4-13 per 1M directions". (5) Optional, needs user decision: an epsilon in the min-sum comparison would make ties deterministic — but it changes above-horizon bits, so it is *not* recommended inside this phase. |
| F6 | **The D-11(a) round trip cannot go through the shipped `getDecodeMatrix` beyond order 1.** The decoder caps at 16 speakers (`E[MAX_AMBI_CHANNELS][MAX_SPEAKERS]`) and is a rank-limited pseudo-inverse: encode -> decode -> re-encode max coefficient error on 9.1.6 (15 speakers, eps 0.01): order 1 = 7.6e-3, order 2 = **1.13**, order 3 = **1.25**. With more than 16 speakers it writes out of bounds. | `[VERIFIED: src/IO/AmbisonicsCodec.cpp:148-152]` `float E[MAX_AMBI_CHANNELS][MAX_SPEAKERS] = {};` ... `E[c][s] = evaluateSH(c, speakerAzimuths[s], speakerElevations[s]);` with no `numSpeakers` bound. Round trip with a test-local dense decoder (200 Fibonacci points, double, eps 1e-6) using the **real** `evalSH`: max coefficient error 1.5e-8 (order 1) ... 6.2e-8 (order 6) — **identical for the buggy and the fixed code** (D-12 confirmed on the real code). | The round-trip test must be *test-local* (dense speaker set + own solve in double, `AmbisonicsCodec::encode` for the encodes). It proves nothing about the shipped decoder; that is what D-09's old-vs-new pin covers. Add `if (numSpeakers > MAX_SPEAKERS) return;` to `getDecodeMatrix` (message thread, free) — an out-of-bounds write is a public-API defect. |
| F7 | **OSD does not go through the engine's gain path, and the 2D path can hang.** OSD builds `LayoutContext layoutCtx { surLayout, layoutState.vbapTriplets, layoutState.ambiDecodeMatrix, layoutState.ambiNumSpeakers };` from `getActiveLayout()` and calls `algo->computeGains (src, layoutCtx, objChannelGains[t], ...)` itself; `engineComputesGains` is not used. So D-06(a)'s sanitiser inside `RenderEngine` never sees OSD's positions. Measured on the real code: `computeVBAPGains2D` **hangs** (`while (azimuthRad > pi) azimuthRad -= 2pi;`) for `+inf` and for any azimuth at or above ~1e9 rad (`1e8` and `1e6` return); KNN returns NaN gains for NaN/inf azimuth; the ADM-OSC receiver deliberately forwards `NAN` as a "field not set" sentinel and does not clamp `/azim`. | `[VERIFIED: OSD 30391cd Source/PluginProcessor.cpp:2852-2853, :2938]` (quoted above). Scratch harness (`s_nan`) with `alarm(5)`: `VBAP quad az=inf exit=142`, `az=1e9 exit=142`, `az=1e8 exit=0`. `[VERIFIED: src/OSC/ADMOSCReceiver.cpp:56-60]` `l.admPositionReceived (objIdx, v, NAN, NAN);` | The guard must live in **two** places: (i) algorithm layer — `computeVBAPGains2D/3D` return silence for non-finite az/el and wrap large finite az/el with a bounded `std::remainder`/`fmod` (replaces the unbounded `while` loops); (ii) engine layer — D-06(a) hold-last-good copy of `sources.objects` at the top of `renderBlock` (covers HRIR update, SH encode, and `engineComputesGains`). Also sanitise `distance` (a NaN distance poisons `smoothedNfcDistance` forever). |
| F8 | **Test command and baseline.** The binary is `build/tests/SpatialCoreTests` (the orchestrator brief's `./build/SpatialCoreTests` does not exist). Baseline: 149 test cases / 1624 assertions; **1 pre-existing failure** (HUTUBS PP2 golden checksum, deferred by Phase 1 as `01-01`, Phase 3 territory) and one intentional JUCE assertion print from the oversized-block test. Full suite runs in ~8 s (Debug). | `./build/tests/SpatialCoreTests` -> `test cases: 149 | 148 passed | 1 failed`, `assertions: 1624 | 1623 passed | 1 failed`; `[VERIFIED: .planning/phases/01-documentation-truth-contract-freeze/deferred-items.md]`. | Phase gate = "all tests pass except `HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)`". Use `-# "~[hutubs]"`-style exclusion only if the planner prefers; otherwise state the known failure in every verify step. |
| F9 | **OSD compiles its own target with `-ffast-math`.** Header-inline `std::isfinite` compiled in an OSD translation unit is folded to `true`. SpatialCore's own sources are compiled without it (`CMakeLists.txt` comment: removing -ffast-math from SpatialCore restored bit-identical output). | `[VERIFIED: OSD 30391cd CMakeLists.txt:162]` `target_compile_options(OpenSpatialDelay PRIVATE -ffast-math -Wno-nan-infinity-disabled)`; `[VERIFIED: CMakeLists.txt:182]` `target_compile_options(SpatialCore PRIVATE -Wno-nan-infinity-disabled)`. | Put every `isfinite` guard in a **.cpp** of the SpatialCore target (`SpatialMath.cpp`, `RenderEngine.cpp`), never in a header `inline`. Do not add a `[[deprecated]]` attribute to `nearestSpeaker3DFallback` (comment-only deprecation), to avoid new warnings in consumers. |
| F10 | **D-09 is a pure refactor: old and new decode matrices are bit-identical** for all 15 layouts (max diff 0, 0 non-identical entries). `getDecodeMatrix(3, N, az, el, &m[0][0])` writes `decodeMatrix[s * 16 + c]`, which is exactly `float[MAX_SPEAKERS][MAX_SPEAKERS]` because `M == 16 == MAX_SPEAKERS`. No copy/adapter loop needed. | Scratch harness `s_ambi.cpp` (verbatim copy of `RenderEngine.cpp:653-734` vs `AmbisonicsCodec::getDecodeMatrix(3, ...)` over `getLayoutDef(0..14)`): `worst 0`. | Add `static_assert (MAX_SPEAKERS == (3 + 1) * (3 + 1))` next to the call. The pin test can use tolerance 0 (recommend `1e-6f` to survive compiler flag changes). |
| F11 | **Do not append the lower-hemisphere triplets inside `buildVBAPTripletsForLayout`.** The existing check `tests/IO/SpeakerLayoutTests.cpp:221-235` ("height layout <=> non-empty triplets") and the D-02(a) guard both test `triplets.empty()`. If extras were appended by the builder, a height layout whose *regular* triplets failed to build would still be non-empty and the guard/test would go blind. | `[VERIFIED: tests/IO/SpeakerLayoutTests.cpp:221-235]`. | Keep the builder regular-only. Add a second function (Pattern 1) called by `activateLayout` after the guard. |
| F12 | **Documentation drift the fixes must sweep.** MDAP is cited "Pulkki 2000" in **five** places, not two. The auto-loading skill says the 3D VBAP tie-break picks "highest gain sum" (code picks the *minimum*), documents the nearest-speaker below-horizon fallback, and says VBIP "squares ... re-normalizes". | `[VERIFIED: grep]` `README.md:27`, `include/SpatialCore/Algorithms/MDAPAlgorithm.h:7`, `tests/Algorithms/SpatializationAlgorithmTests.cpp:271`, `.claude/skills/spatial-audio-dsp/SKILL.md:108`, `src/Algorithms/MDAPAlgorithm.cpp:8`. `.claude/skills/spatial-audio-dsp/SKILL.md:44,48,56-63,277-279,554`; `README.md:24`; `include/SpatialCore/Algorithms/VBIPAlgorithm.h:7`. Code: `if (sum < bestGainSum)   // Min sum = tightest triangle` (`src/Core/SpatialMath.cpp:251-252`). | Doc task lists all sites (§Architecture Patterns 6). |
| F13 | **CONTEXT's triplet counts for EAR ("448 -> 2119, ~86, 9.1.6") are not reproducible.** Measured (real builder + my lower-hull construction): regular triplets 5.1.2=25, 5.1.4=72, 7.1.2=45, 7.1.4=118, 7.1.6=235, 9.1.4=190, 9.1.6=355, SML13.1=188 (matches D-01's 25/118/355); lower-hemisphere triplets 25, 25, 35, 35, 35, 45, 45, 40. | Scratch prototypes `proto2.py`, `s_ear_proto.cpp`; layout runtime dump `s_fmt.cpp` (`tri=25/72/45/118/235/190/355/188`). | Size expectations for storage: <= 45 extra `VBAPTriplet`s (~64 bytes each). Test assertions on counts should be `> 0` per height layout, not literals, except regular counts (already stable). |
| F14 | **Line-reference corrections.** `kLayoutExpectations` is at `tests/IO/SpeakerLayoutTests.cpp:133` (CONTEXT: `:87`, which is the format-table test). The height-threshold mismatch is demonstrable with a 0.8 degree speaker (below). | `[VERIFIED: grep]`. Scratch `s_thr.cpp`: centre speaker at 0.5 deg -> `layoutHasHeight=0 triplets=0` consistent; **0.8 deg -> `layoutHasHeight=0 triplets=3` MISMATCH**; 1.1 deg -> `1 / 6` consistent. | D-03's regression test = a hand-built 5-speaker layout with one speaker at 0.8 degrees. |
| F15 | **OSD v1.0.0 ships the same SH defect.** OSD's own `static float evalSH` copy has the identical constants (`case 16: return std::sqrt (35.0f) * 0.375f * sin4Az * cosEl4;`, `case 25: ... std::sqrt (63.0f / 8.0f) ...`). SpatialCore's copy is verbatim OSD. | `[VERIFIED: OSD 30391cd Source/PluginProcessor.cpp:2211, :2222, :2232, :2237]`; SpatialCore origin `git log -S`: `d95f17c`. | Cross-repo follow-up: the 4OA-6OA change is a *fix* of a shipped OSD defect; OSD's local `evalSH` (used at `:2262` for its order-3 decode, unaffected) should be deleted at migration. |

## Architectural Responsibility Map

Tiers here are the library's own layers (the phase touches no browser/CDN tiers).

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Regular VBAP triplet build, height-threshold constant (D-03), `kLayoutExpectations` tie | `IO/SpeakerLayout` (message thread) | `tests/IO` | Layout tables and triplet geometry live here; allocation allowed. |
| Lower-hemisphere triplet build (D-04 precompute) | `IO/SpeakerLayout` (new function, message thread) | `Engine/activateLayout` (caller) | Pure geometry of a `SpeakerLayout`; testable without an engine; allocates via `push_back` off the audio thread. |
| Layout-build guard, Release crash (D-02a), ambi decode adapter (D-09), publish via atomic swap | `Engine/RenderEngine::activateLayout` | `IO/AmbisonicsCodec` (callee) | Only place that owns `LayoutState` and the double-buffer swap. |
| Triplet selection, best-triplet fallback, non-finite/huge-angle guard (D-06b) | `Core/SpatialMath::computeVBAPGains2D/3D` (audio thread) | Algorithms (callers) | Single choke point for VBAP/VBIP/MDAP; must stay alloc/lock/log free (DR-1). |
| VBIP transform (D-14) | `Algorithms/VBIPAlgorithm` (audio thread) | — | Post-processes the gains `computeVBAPGains*` returns; interface frozen (DR-2/DR-7), so no new inputs. |
| Hold-last-good position (D-06a) | `Engine/RenderEngine::renderBlock` (audio thread, preallocated members) | Consumer (OSD computes its own gains) | One copy of `sources.objects` covers HRIR update, SH encode and `engineComputesGains`. |
| SN3D constants, convention statement (D-08/D-10) | `Core/SpatialMath::evalSH` (single source of truth) | `IO/AmbisonicsCodec` (forwarder), docs | Shipping path already goes through `evalSH`. |
| Reference generators (scipy/ear/numpy) | Offline tooling in `tests/reference/` | — | Checked in, never run in CI (P1 D-10/D-13, D-11c). |

## Standard Stack

### Core (no new C++ dependencies)
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 9.0.0 | `jassert`, `MathConstants`, DSP modules | Already pinned. `[VERIFIED: JUCE v9.0.0 printed by the test binary]` |
| Catch2 | v3.7.1 | Test framework (FetchContent) | Already used. `[VERIFIED: tests/CMakeLists.txt]` |
| libmysofa | v1.3.2 | SOFA I/O | Untouched this phase. |

### Supporting (offline reference tooling, dev machine only, never shipped, never in CI)
| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| numpy | 2.5.3 | Textbook VBAP/VBIP/DBAP values, MDAP port | `gen_panning_reference.py` |
| scipy | 1.18.1 | Independent SH oracle (`lpmv`, `sph_harm_y`) | `gen_sh_reference.py` |
| ear (EBU ADM Renderer) | 2.1.0 | Numeric oracle for the lower-hemisphere construction | `gen_ear_reference.py` |

**Working install recipe (tested: Python 3.14.7, macOS arm64).** `pip install ear` fails on Python 3.14 (it pins `lxml~=4.4`, which has no wheel and fails to build). Use:
```bash
python3 -m venv .context/venv                       # .context/ is gitignored
.context/venv/bin/pip install --only-binary=:all: lxml
.context/venv/bin/pip install --no-deps ear==2.1.0
.context/venv/bin/pip install numpy scipy attrs multipledispatch six pyyaml ruamel.yaml "setuptools<81"   # pkg_resources
```
Frozen result: `attrs==26.1.0 ear==2.1.0 lxml==6.1.3 multipledispatch==1.0.0 numpy==2.5.3 PyYAML==6.0.3 ruamel.yaml==0.19.1 scipy==1.18.1 setuptools==80.10.2 six==1.17.0`. pip prints "requires numpy~=1.14 ..." warnings; `ear` 2.1.0 nevertheless imports and runs correctly on numpy 2.5.3 (every oracle value below was produced this way, and the hull/downmix logic was cross-checked by reading `ear/core/point_source.py`). Record this recipe in `tests/reference/README.md`. `[VERIFIED: pip freeze + successful runs this session]`

**Version verification:** `pip index versions ear` -> `ear (2.1.0)`, available 1.0.0...2.1.0; `numpy (2.5.3)`; `scipy (1.18.1)`. `[VERIFIED: pip index]`

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Lower-only hull (recommended) | ear's full-sphere hull incl. upper speakers | Exact ear match everywhere, but 0.71 horizon jump on 5.1.4 (F4). |
| Flagged extras in `vbapTriplets` (recommended) | New `LayoutContext` member | Touches the frozen context struct that OSD aggregate-initialises with 4 members (F7); rejected. |
| Test-local dense decoder for the round trip | Raising `MAX_SPEAKERS` | `MAX_SPEAKERS` change is a repo-wide array-size change (`Types.h:9`); out of scope. |

## Package Legitimacy Audit

No C++ packages are added. The only installs are the three Python reference-generator dependencies above. `gsd-tools query package-legitimacy check --ecosystem pypi` returned **SUS** for all three, for registry-metadata reasons only:

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---------|----------|-----|-----------|-------------|---------|-------------|
| numpy | PyPI | first upload 2006-12-02 | unknown (API gap) | github.com/numpy/numpy | SUS (`too-new` latest release, `unknown-downloads`, `no-repository` in seam metadata) | Flagged — planner must add `checkpoint:human-verify` before install |
| scipy | PyPI | first upload 2010-07-27 | unknown (API gap) | github.com/scipy/scipy | SUS (`unknown-downloads`, `no-repository`) | Flagged — same checkpoint |
| ear | PyPI | first upload 2018-03-29 (2.1.0 published 2022-01-26) | unknown (API gap) | github.com/ebu/ebu_adm_renderer (author "EBU", BSD-3-Clause-Clear) | SUS (`unknown-downloads` only) | Flagged — same checkpoint |

**Packages removed due to [SLOP] verdict:** none.
**Packages flagged as suspicious [SUS]:** numpy, scipy, ear — the reasons are missing download/repo metadata from the seam's PyPI lookup, not malicious signals (repo URLs above come from PyPI project metadata). They are dev-machine-only, pinned, installed into a gitignored venv, and their output is committed as literal tables. One `checkpoint:human-verify` covering the venv install is sufficient. The three names were also verified as the well-known upstream projects via `pip show` / PyPI JSON. `[ASSUMED: wheels are what upstream publishes — no hash/signature check was run]`.

## Architecture Patterns

### System Architecture Diagram

```
LAYOUT-BUILD PATH (message / prepare thread — may allocate)         AUDIO THREAD (no alloc / lock / log — DR-1)
────────────────────────────────────────────────────────────        ────────────────────────────────────────────────
consumer: setOutputFormat(fmt)  [stays void, D-02b]                  consumer (OSD today): builds LayoutContext from
   │                                                                     getActiveLayout() -> algo->computeGains(...)
   ▼                                                                 consumer (engineComputesGains): RenderEngine does it
RenderEngine::activateLayout(fmt)                                        │
   ├─ getLayoutDef(id) ─────────────────────────────▶ SpeakerLayout     ▼
   ├─ AmbisonicsCodec::getDecodeMatrix(3,N,az,el,                    renderBlock(sources, blockCtx, out, n)
   │     &buf.ambiDecodeMatrix[0][0])   [D-09, replaces private copy]    ├─ [D-06a] sanitized_ = sources; per object:
   ├─ buildVBAPTripletsForLayout ─▶ REGULAR triplets (unchanged)         │     finite? else hold lastGood{Az,El,Dist}[t]
   ├─ [D-02a] if (layoutHasHeight != !regular.empty())                   ├─ engineComputesGains ─▶ computeObjectGains
   │             { jassertfalse; std::abort(); }   (Release too)         │        └─ VBAPAlgorithm::computeGains
   ├─ [D-04] appendLowerHemisphereTriplets(layout, triplets)             │              └─ computeVBAPGains3D / 2D  ◀── OSD calls the same
   │           (flagged; virtual speakers never take a slot)             └─ 5-branch dispatch (unchanged)
   └─ atomic swap: activeLayoutIndex.store(prepareLayoutIndex)

computeVBAPGains3D(layout, triplets, az, el, out)
   0. non-finite / huge az,el ─▶ wrap or silence            [F7]
   1. pass 1: regular triplets only, min-sum, g >= -1e-6     (bit-identical to today)
   2. pass 2 (only if pass 1 found none): flagged lower triplets, same rule;
        vertex kinds: real speaker | virtual -30 copy (accumulates on its ear-level speaker) | nadir (1/sqrt(n) to all ear-level)
   3. none found (finite input): best triplet by largest min-gain, negatives clamped, renormalised  [D-06b]; Debug jassertfalse
   4. accumulate (+=) into out[], power-normalise
VBIP / MDAP / VBAP all call this ⇒ all inherit EAR; Binaural & Ambisonics never call it (D-05)
```

### Recommended Project Structure (additions only)
```
tests/
├── reference/                      # NEW: checked-in, never run in CI (D-11c)
│   ├── README.md                   # pins + the ear install recipe above
│   ├── gen_sh_reference.py         # 49 literals at az=64, el=10 (+ asserts)
│   ├── gen_ear_reference.py        # ear oracle vectors (nadir-cap region only)
│   ├── gen_panning_reference.py    # VBAP/VBIP/DBAP/MDAP-port values
│   ├── layouts_from_cpp.py         # parses layoutDefs so oracles follow the real table
│   └── *.h (generated literals)    # ShReference.h, EarReference.h, PanningReference.h
├── Algorithms/SpatializationAlgorithmTests.cpp   # extend: D-13 property + textbook sections
├── Core/SpatialMathTests.cpp                     # extend: SN3D invariant, 49 literals, sanitiser
├── IO/AmbisonicsCodecTests.cpp                   # extend: forwarder equality, round trip (test-local)
├── IO/SpeakerLayoutTests.cpp                     # extend: static_assert, D-03, lower-hemisphere builder
├── Engine/RenderEngineTests.cpp                  # extend: 23-format resolve, D-09 pin, D-06a hold, EAR via engine
└── Core/ConsumerSurfaceTests.cpp                 # NEW (optional): compile-only DR-3 surface pins
```
`tests/CMakeLists.txt` lists every test file explicitly (no glob) — new `.cpp` files must be added by hand. `[VERIFIED: tests/CMakeLists.txt:9-27]`

### Pattern 1: Lower-hemisphere triplets (D-04, D-05)

**What:** For layouts with height and *no* speaker below -10 degrees, build the hull of {ear-level speakers, a -30 degree copy under each, a nadir}; keep every supporting-plane triple that has at least one virtual vertex; store as flagged `VBAPTriplet`s. Ear-level = `|elevationRad| <= 10 degrees` (ear's `mid = (-10 <= nominal_el) & (nominal_el <= 10)`). A virtual -30 copy accumulates onto its own ear-level speaker (ear's 1:1 `downmix_row[mid_channel] = 1`); the nadir spreads `1/sqrt(n)` over all `n` ear-level speakers (ear's `np.full(len(real_verts), 1.0 / np.sqrt(len(real_verts)))`), then the result is power-normalised. Because both maps are linear and the final step is a normalisation, ear's chain (triplet-normalise -> ngon downmix -> normalise -> extra->mid downmix -> normalise) equals `normalise(M * (inv * p))` per triplet. `[CITED: ebu/ebu_adm_renderer ear/core/point_source.py — read from the installed ear 2.1.0, _configure_full, VirtualNgon.handle, extra_pos_vertical_nominal]`

**Struct change (source-compatible; OSD does `VBAPTriplet tri; tri.i = ...`):**
```cpp
// include/SpatialCore/IO/SpeakerLayout.h
struct VBAPTriplet
{
    int i = 0, j = 0, k = 0;
    float inv[3][3] = {};
    // --- lower-hemisphere (BS.2127-style) triplets only; ordinary triplets leave these at their defaults ---
    bool  lowerHemisphere = false;   // true => only consulted when no ordinary triplet contains the source
    int   nadirVertex     = -1;      // 0..2 = which vertex is the virtual nadir (its i/j/k slot is unused), -1 = none
    uint16_t nadirMask    = 0;       // bit s set => ear-level speaker s receives nadirGain (MAX_SPEAKERS == 16 fits exactly)
    float nadirGain       = 0.0f;    // 1 / sqrt(n)
};
// i/j/k of a virtual -30 vertex hold the INDEX OF THE EAR-LEVEL SPEAKER IT DOWNMIXES TO; `inv` is built from the
// virtual vertex's real position. Duplicate indices (e.g. {A, B, A}) are legal, hence `+=` accumulation below.
void appendLowerHemisphereTriplets (const SpeakerLayout& layout, std::vector<VBAPTriplet>& triplets);
```
**Builder (double precision, message thread):** vertices = ear-level speakers, their `(az, -30 deg)` copies, `(0,0,-1)`. For every triple with `>= 1` virtual vertex: skip `|det| < 1e-3`; take plane normal `n = (b-a) x (c-a)` oriented so `n . a > 0`; keep iff `max over all vertices of n . (v - a) <= 1e-7` (supporting plane = hull facet; coplanar quads yield all four overlapping triangles, exactly as the regular set already does); invert the column matrix in double, store as float. No scipy, no `ConvexHull` — brute force is `C(25,3) = 2300` triples x 25 tests, negligible off-thread. Counts: 25 / 25 / 35 / 35 / 35 / 45 / 45 / 40 (5.1.2, 5.1.4, 7.1.2, 7.1.4, 7.1.6, 9.1.4, 9.1.6, SML13.1). Guard: build extras only if some speaker has `|el| <= 10 deg`; skip if any speaker is below -10 degrees (no shipped layout; lower-layer speakers would cover the hemisphere natively) — **[ASSUMED] treat as "no extras" rather than an error.**

**Verified behaviour of this construction** (numpy, float64, real layouts; C++ float prototype `s_ear_proto.cpp`, 5,203,819-5,203,827 evaluations per layout: 3M uniform-on-sphere, 0.1-degree az x 0.3-degree el grid, horizon/pole/ring-edge shells, every speaker position): **0 directions fell through to the fallback, 0 unfound, worst `|power - 1|` = 7.2e-7**; C++ output matches ear to 6 digits at `(30,-60)`, `(30,-90)`, `(0,-45)`, `(180,-70)` on 7.1.4. Horizon continuity (`el = +-0.05 deg`): <= 0.0028. Sweep steps at 0.1-degree az: nadir cap (el <= -45 on 7.1.4) <= 0.004; trapezoid band (-30..0) up to ~0.10 — the same coplanar-tie phenomenon as F5, ~20x the smooth slope; MDAP at el=0 with EAR 0.03-0.11 at 0.5-degree steps (vs 0.16-0.19 today).

### Pattern 2: `computeVBAPGains3D` structure (D-04 consumption, D-06b)
```cpp
// src/Core/SpatialMath.cpp -- sketch, arithmetic of pass 1 must stay byte-for-byte what it is today
void computeVBAPGains3D (const SpeakerLayout& layout, const std::vector<VBAPTriplet>& triplets,
                         float azimuthRad, float elevationRad, float* outGains)
{
    const int N = layout.numSpeakers;
    for (int s = 0; s < N; ++s) outGains[s] = 0.0f;
    if (! sanitiseDirection (azimuthRad, elevationRad)) return;          // non-finite -> silence; huge -> wrapped (F7)
    const float px = std::cos (elevationRad) * std::sin (azimuthRad); /* py, pz as today */

    int best = -1; float bestSum = 1e30f; float bg[3] = {};
    int fb = -1; float fbMin = -1e30f; float fbg[3] = {};                 // D-06b: largest-min-gain candidate over ALL triplets
    for (int pass = 0; pass < 2 && best < 0; ++pass)                     // regular first: bit-identical above the horizon
        for (int t = 0; t < (int) triplets.size(); ++t)
        {
            const auto& tri = triplets[(size_t) t];
            if (tri.lowerHemisphere != (pass == 1)) continue;
            /* g0,g1,g2 exactly as today */
            const float mn = std::min (g0, std::min (g1, g2));
            if (mn >= -1e-6f && sum < bestSum) { /* record best, clamp >= 0 */ }
            if (mn > fbMin)                    { /* record fallback candidate */ }
        }
    jassert (best >= 0);                                                  // Debug-only (D-06c); Release keeps going
    if (best < 0) { best = fb; /* use fbg */ }
    if (best < 0) return;                                                 // empty vector: silence
    /* accumulate: outGains[idx] += g   (== assignment for distinct idx, so regular results stay bit-identical) */
    /* nadirVertex >= 0: for s in mask: outGains[s] += g * tri.nadirGain */
    /* power-normalise as today */
}
```
The audio thread touches only stack floats and a `const std::vector&`. D-06(b)'s "bit-identical wherever a triplet is found today" holds because pass 1 is the existing loop over the existing triplets in the existing order with the existing `-1e-6f` test and `sum <` rule, and `0.0f + x == x`.

**Algorithms after D-01:** each `computeGains` becomes `if (! ctx.triplets.empty()) computeVBAPGains3D (...); else computeVBAPGains2D (...);` plus `jassert (ctx.triplets.empty() == ! layoutHasHeight (ctx.layout));` (compiled out in Release) so a direct caller passing empty triplets on a height layout still fails loudly in Debug. Sites: `VBAPAlgorithm.cpp:15-21`, `VBIPAlgorithm.cpp:16-23`, `MDAPAlgorithm.cpp:45-50` and `:98-102`. `nearestSpeaker3DFallback` stays in `SpatialMath.h` with a comment-only deprecation (F9).

### Pattern 3: VBIP (D-14, D-15)
```cpp
// after computeVBAPGains{2D,3D} filled outputGains[0..numSpeakers)
float sum = 0.0f;
for (int s = 0; s < numSpeakers; ++s) { outputGains[s] = std::sqrt (std::max (0.0f, outputGains[s])); sum += outputGains[s] * outputGains[s]; }
if (sum > 1e-12f) { const float scale = 1.0f / std::sqrt (sum); for (...) outputGains[s] *= scale; }
```
= `g_i = sqrt(G_i / sum G_j)` because VBAP gains are proportional to `G`. `[CITED: dafx.de/paper-archive/1998/PER15.PS.pdf s2.2.1-2.2.2]`: VBAP `g_i = G_i / sqrt(sum G_i^2)`, VBIP `g_i = sqrt(G_i / sum G_i)`, "VBAP (< 700 Hz) and VBIP (> 700 Hz)". Applied to the *final real-speaker* gains, so below the horizon it operates on the EAR-downmixed vector (a defined extension; document it). Verified: Quad az30 rE azimuth = 30.000 degrees, 5.0 az10 = 10.000, az50 = 50.000; output power exactly 1 everywhere.

### Pattern 4: Sanitiser (D-06a) — where it lives
`RenderEngine` (`.cpp`, preallocated): `RenderSources sanitized_;`, `float lastGoodAzDeg_[MAX_SOURCES], lastGoodElDeg_[MAX_SOURCES], lastGoodDist_[MAX_SOURCES]` (init 0, 0, 0.5f = `ObjectState` defaults `[VERIFIED: Types.h:45-51]`). At the top of `renderBlock`: `sanitized_ = sources;` (trivially copyable, ~0.8 KB — same idiom as `gainScratch_ = blockCtx;` at `RenderEngine.cpp:75`), then for each live object: if `azimuthDeg`/`elevationDeg` is non-finite, substitute the last good pair (both fields together); else store it as last good after wrapping azimuth into [-180, 180] with `std::remainder` and clamping elevation to [-90, 90]; same for `distance` (non-finite -> last good, else clamp 0..1). Every render path and `computeObjectGains` then reads `sanitized_`. **Do not** put this in a header (F9). It does *not* replace the algorithm-layer guard (F7).

### Pattern 5: D-09 adapter
```cpp
// in activateLayout, replacing computeAmbiDecodeForLayout(...)
static_assert (MAX_SPEAKERS == (3 + 1) * (3 + 1), "ambiDecodeMatrix rows must equal the order-3 channel count");
float az[MAX_SPEAKERS] = {}, el[MAX_SPEAKERS] = {};
for (int s = 0; s < buf.layout.numSpeakers; ++s) { az[s] = buf.layout.speakers[s].azimuthRad; el[s] = buf.layout.speakers[s].elevationRad; }
std::memset (buf.ambiDecodeMatrix, 0, sizeof (buf.ambiDecodeMatrix));         // rows >= N cleared (old code left stale rows)
AmbisonicsCodec::getDecodeMatrix (3, buf.layout.numSpeakers, az, el, &buf.ambiDecodeMatrix[0][0]);
buf.ambiNumSpeakers = buf.layout.numSpeakers;
```
Delete `computeAmbiDecodeForLayout` (`RenderEngine.cpp:645-734`, declaration `RenderEngine.h:293-295`). Pin test: keep a **test-local verbatim copy** of the old function (source: `git show bf10fac:src/Engine/RenderEngine.cpp`, lines 653-734) as `referenceAmbiDecode` and compare with `engine.getActiveLayout().ambiDecodeMatrix` for all 15 layout formats; tolerance 1e-6 (measured diff is exactly 0). Capture must happen **before** the deletion lands, or use the git blob.

### Pattern 6: Convention statement — landing sites (D-10)
Text: *"Real spherical harmonics, ACN channel order (`acn = l*l + l + m`; m>0 -> cos(m*az), m<0 -> sin(|m|*az)), SN3D normalisation (`sum over m of Y_lm^2 = 1` at every order), no Condon-Shortley phase, angles in radians, azimuth 0 = front, +azimuth toward +Y (left), elevation 0 = horizon, +up. This is the AmbiX convention."*
Sites: `include/SpatialCore/Core/SpatialMath.h:71` (docblock on `evalSH`); `src/Core/SpatialMath.cpp:7-13` (header comment currently says "ACN/SN3D" only); `include/SpatialCore/IO/AmbisonicsCodec.h` (class docblock, `evaluateSH` forwarder note); `README.md:17,28`; `docs/integration-guide.md` (new short section — it has none); `.claude/skills/spatial-audio-dsp/SKILL.md` §7.1/§7.4 (state "no Condon-Shortley phase" and the sign of Y); `include/SpatialCore/IO/OutputFormat.h:25` already says "AmbiX ACN/SN3D".
Other doc/comment sweeps in the same phase: VBIP (`VBIPAlgorithm.h:7`, `VBIPAlgorithm.cpp:11`, `README.md:24`, `SKILL.md:56-63,554`), MDAP citation (5 sites, F12), fallback prose (`SKILL.md:48,277-279`, "highest gain sum" `SKILL.md:44`), DBAP effective R=12.04 dB / d^2 clamp (D-16, `DBAPAlgorithm.h`), `nearestSpeaker3DFallback` deprecation comment.

### Anti-Patterns to Avoid
- **Adding a `LayoutContext` member** for the EAR data — OSD aggregate-initialises it with exactly four initialisers (F7).
- **Appending extras from `buildVBAPTripletsForLayout`** — blinds the D-02 guard and its test (F11).
- **`isfinite` in a header** — folded away by OSD's `-ffast-math` (F9).
- **Asserting bit-exact or "no jump" behaviour inside a coplanar-tie region** (F5).
- **Using the shipped `getDecodeMatrix` for the order 4-6 round trip** (F6).
- **Widening the triplet tolerance to -1e-5** (D-07, locked).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Convex hull for the lower triplets | Incremental/quickhull implementation | Brute-force supporting-plane test over all triples (<= 2300) | Off-thread, tiny n, ~25 lines, and it keeps coplanar-quad overlap consistent with the regular set. |
| SH reference values | Hand-typed constants / re-deriving the polynomials | `scipy.special.lpmv` (+ second route `sph_harm_y`) in `gen_sh_reference.py` | The bug *is* hand-typed constants; two independent scipy routes agree to 1e-15. |
| Lower-hemisphere oracle | Own re-implementation of BS.2127 | PyPI `ear` 2.1.0 | The user chose it as oracle (D-04); it is the standard's reference implementation. |
| 2D azimuth wrap | `while (az > pi) az -= 2pi` | `std::remainder (az, 2*pi)` | The loop never terminates for `inf` or |az| >~ 1e9 (F7). |
| Dense decoder in the round-trip test | New linear-algebra library | ~15-line double-precision Gauss-Jordan in the test (same shape as `getDecodeMatrix`) | Test scope only; no dependency. |
| Uniform sphere sample set for the round trip | Spherical-design tables | Fibonacci lattice, N = 200 | Measured error 6e-8 with eps 1e-6; no table needed. |

**Key insight:** every reference number in this phase must come from something independent of the code under test (scipy, ear, the published formula). Three numbers CONTEXT relied on (F1, F2 order, F3) were wrong or unreachable precisely because they were derived by simulation rather than by running the code.

## Reference Data (paste-ready, all generated this session)

### A. D-13 textbook values — every candidate re-derived (numpy, positions parsed from `SpeakerLayout.cpp`)

| Case | CONTEXT value | Recomputed | Verdict |
|------|---------------|------------|---------|
| VBAP Quad az30 | 0.9659 / 0.2588 | 0.9659258 / 0.2588190 (speakers 45, -45) | CONFIRMED |
| VBAP 5.0 az10 | C 0.8917 / L30 0.4527 | L30 (idx 0) 0.4527072, C (idx 2) 0.8916592 | CONFIRMED (index order: `{30,-30,0,110,-110}`) |
| VBAP 5.0 az50 | L30 0.9301 / Ls110 0.3673 | L30 (idx 0) 0.9300936, Ls110 (idx 3) 0.3673226 | CONFIRMED |
| VBIP Quad az30 | 0.8881 / 0.4597 | 0.8880738 / 0.4597008 | CONFIRMED |
| VBIP 5.0 az10 | 0.8144 / 0.5803 | idx0 0.5802964, idx2 0.8144053 | CONFIRMED (order: L30 0.5803, C 0.8144) |
| VBIP 5.0 az50 | 0.8467 / 0.5321 | idx0 0.8466885, idx3 0.5320889 | CONFIRMED |
| VBIP invariant | sum g^2 l points at the source | rE azimuth 30.0000 / 10.0000 / 50.0000 deg; power exactly 1 | CONFIRMED |
| DBAP Quad az30, dist 1, R = 12.04 dB | 0.9984, 0.0270, 0.0173, 0.0459 for order 45,135,-135,-45 | **in code order (45,-45,135,-135): 0.9984304, 0.0459007, 0.0270259, 0.0173052** | VALUES CONFIRMED, **ORDER CORRECTED** (F2) |
| DBAP Quad az30, dist 0.5 | (existing golden) | 0.9390509, 0.2691336, 0.1768006, 0.1203831 | Existing golden already textbook |
| MDAP | spread=0 == VBAP | not reachable (F3); port: Quad az30 `0.9478133 0.3132631 0.0592973 0`, 5.0 az10 `0.6412997 0.2999306 0.7051913 0.0384905 0` | REPLACED |
| Current VBIP Quad az30 | 0.9974 / 0.0716 | **0.9330127 / 0.0669873** (test-pinned) | **CORRECTED** (F1) |

DBAP formula used: `v_i = d_i^-2 / sqrt(sum_j d_j^-4)`, `d_i^2 = |s - x_i|^2` clamped `>= 0.001`, speakers and source on the unit sphere scaled by `distance` — Lossius eq (3)-(5) with `a = 2` (`R = 2 * 6.0206 = 12.04 dB`, eq (4)), `r_s = 0`. `[CITED: jamoma.org/publications/attachments/icmc2009-dbap-rev1.pdf eq (2)-(5): "v_i = k / d_i^a", "a = R / (20 log10 2)", "k = 1 / sqrt(sum 1/d_i^(2a))"]`. On-speaker DBAP is unity only to ~1e-3 (clamp), so test unity at tolerance 2e-3.

Unambiguous 3D VBAP pins on 7.1.4 (single smallest-sum triplet, no tie; current C++ output equals these to 5 digits; speaker order `30,-30,0,90,-90,135,-135,45h,-45h,135h,-135h`):
`(az 0, el 30)` -> idx2 0.7157629, idx7 0.4938033, idx8 0.4938033 · `(60, 20)` -> idx0 0.3733918, idx3 0.6244854, idx7 0.6860004 · `(10, 10)` -> idx0 0.1193867, idx2 0.9451641, idx7 0.3039929 · `(90, 30)` -> idx3 0.7157629, idx7 0.4938033, idx9 0.4938033. Ambiguous (avoid): `(180, 20)`, `(20, 60)`.

### B. D-08 — the 22 wrong lines and the corrected SN3D constants (`src/Core/SpatialMath.cpp`)

Diagnosis, verified by compiling the **actual `evalSH` text** (lines 10-114) standalone and comparing with scipy on 20,000 random directions: exactly 22 channels wrong, exact ratios 3, 4, 2√2, √2, 4√2, 2√6, 2; shape, sign and the polynomial factors are right; ACN 0-15, 17-23, 29-31 and 42 correct. Sum-over-m deviation: buggy 19.54, fixed 3.5e-6. Root cause: per-(l, m) leading constants for those channels are wrong by exact algebraic factors (the code's origin is verbatim OSD v1.0, F15) — *why* the constants were chosen is not recoverable from the source `[ASSUMED]`. The fix is constants only.

| Line | ACN | Old text | New text |
|------|-----|----------|----------|
| 74, 82 | 16, 24 | `std::sqrt (35.0f) * 0.375f` | `std::sqrt (35.0f) * 0.125f` |
| 85, 95 | 25, 35 | `std::sqrt (63.0f / 8.0f)` | `std::sqrt (63.0f / 128.0f)` |
| 86, 94 | 26, 34 | `std::sqrt (315.0f) * 0.375f` | `std::sqrt (315.0f) * 0.125f` |
| 87, 93 | 27, 33 | `std::sqrt (35.0f / 16.0f)` | `std::sqrt (35.0f / 128.0f)` |
| 88, 92 | 28, 32 | `std::sqrt (105.0f / 8.0f)` | `std::sqrt (105.0f / 16.0f)` |
| 98, 110 | 36, 48 | `std::sqrt (231.0f / 16.0f)` | `std::sqrt (231.0f / 512.0f)` |
| 99, 109 | 37, 47 | `std::sqrt (693.0f / 8.0f)` | `std::sqrt (693.0f / 128.0f)` |
| 100, 108 | 38, 46 | `std::sqrt (63.0f / 16.0f)` | `std::sqrt (63.0f / 256.0f)` |
| 101, 107 | 39, 45 | `std::sqrt (315.0f / 16.0f)` | `std::sqrt (105.0f / 128.0f)` |
| 102, 106 | 40, 44 | `std::sqrt (105.0f / 16.0f)` (line keeps its trailing `* 0.25f`) | `std::sqrt (105.0f / 32.0f)` |
| 103, 105 | 41, 43 | `std::sqrt (21.0f / 16.0f)` | `std::sqrt (21.0f / 64.0f)` |

Applying exactly these 22 substitutions to the extracted function and comparing with scipy over 20,000 random directions: **max abs error 2.1e-6, no channel above 1e-4, sum-over-m deviation 3.5e-6**. After D-08, delete the duplicate body in `AmbisonicsCodec.cpp:13-113` and make `evaluateSH` return `spatialcore::evalSH (acn, az, el)` (add `#include <SpatialCore/Core/SpatialMath.h>`; no include cycle: `SpatialMath.h` includes `IO/SpeakerLayout.h`, not `AmbisonicsCodec.h`).

**The 49 reference values, az = 64 degrees, el = 10 degrees** (`gen_sh_reference.py`, scipy 1.18.1; route 1 `lpmv` with Condon-Shortley removed, route 2 `sph_harm_y`; agreement 3.3e-16; min |Y| = 0.0721261 at ACN 17). Tolerance 1e-5 (float32 evaluation error measured <= 2.1e-6):
```cpp
static constexpr float kShRef_az64_el10[49] = {
     1.000000000f, 0.885139345f, 0.173648178f, 0.431711304f, 0.661859328f, 0.266221118f, -0.454769466f, 0.129844715f, -0.517101179f,   // ACN 0-8
    -0.156990472f, 0.256992782f, -0.460313171f, -0.247381933f, -0.224509734f, -0.200784767f, -0.738582103f,                             // ACN 9-15
    -0.674922629f, -0.072126116f, -0.337050780f, -0.338889873f, 0.265901611f, -0.165287635f, 0.263332930f, -0.339326697f, -0.168277111f, // ACN 16-24
    -0.417724629f, -0.351597254f, 0.075659158f, -0.309215491f, 0.255799553f, 0.281017541f, 0.124761778f, 0.241585619f, 0.355948351f,     // ACN 25-33
    -0.087663041f, 0.497824827f,                                                                                                         // ACN 34-35
     0.249225837f, -0.240578412f, 0.302578333f, 0.083335370f, 0.168630638f, 0.363217706f, -0.132121339f, 0.177153112f, -0.131748693f,    // ACN 36-44
     0.392062093f, 0.075441251f, 0.286710187f, 0.559770395f };                                                                           // ACN 45-48
```
(The generator prints the same table with one line per ACN; regenerate rather than trusting this transcription.) This table catches: scale bug, m/-m swap, sign flip, az reversal, el reversal, Condon-Shortley (values have both signs at every order).

**Old code's behaviour on the trio (measured with the real code):** (a) round trip passes identically for buggy and fixed (1.5e-8..6.2e-8); (b) invariant deviates 19.54 (buggy) vs 3.5e-6 (fixed) — use tolerance 2e-5; (c) 22 literal mismatches on the buggy code.

### C. D-04 oracle vectors (PyPI `ear` 2.1.0; `gen_ear_reference.py`, layouts parsed from `layoutDefs`; gains in `layoutDefs` speaker order)

Exactly reproduced by the lower-hemisphere construction (all in ear's `VirtualNgon` region, `|ear - construction| <= 1.1e-16` in float64; C++ float prototype matches to 6 digits). Tolerance for tests: 1e-5.
```
S7_1_4 (11 spk: 30,-30,0,90,-90,135,-135 | 45h,-45h,135h,-135h)      [CONTEXT check: az30 el-60 -> 0.83 / 0.228; el-90 -> 0.378 — CONFIRMED]
 (az  30, el -60): 0.8300495 0.2276759 0.2276759 0.2276759 0.2276759 0.2276759 0.2276759 0 0 0 0
 (az   0, el -45): 0.1164798 0.1164798 0.9584335 0.1164798 0.1164798 0.1164798 0.1164798 0 0 0 0
 (az 180, el -70): 0.2674711 0.2674711 0.2674711 0.2674711 0.2674711 0.5666992 0.5666992 0 0 0 0
 (az-100, el -75): 0.3201575 0.3201575 0.3201575 0.3201575 0.5742917 0.3201575 0.3970957 0 0 0 0
 (az  30, el -90): 0.3779645 x7, then 0 x4                                            (= 1/sqrt(7))
S5_1_2 (7 spk: 30,-30,0,110,-110 | 90h,-90h)
 (az  30, el -60): 0.8506508 0.2628656 0.2628656 0.2628656 0.2628656 0 0
 (az 180, el -60): 0.0081453 0.0081453 0.0081453 0.7070364 0.7070364 0 0
 (az-100, el -75): 0.3637489 0.4175284 0.3637489 0.3637489 0.6547749 0 0
 (az  30, el -90): 0.4472136 x5, 0 0                                                  (= 1/sqrt(5))
S9_1_6 (15 spk: 30,-30,0,90,-90,135,-135,60,-60 | 6 heights)
 (az  45, el -75): 0.4549079 0.2893633 0.2893633 0.2893633 0.2893633 0.2893633 0.2893633 0.4549079 0.2893633 0 x6
 (az -90, el -60): 0.2041241 0.2041241 0.2041241 0.2041241 0.8164966 0.2041241 0.2041241 0.2041241 0.2041241 0 x6
 (az 120, el -50): 0.1486895 0.1486895 0.1486895 0.4781848 0.1486895 0.7852256 0.1486895 0.1486895 0.1486895 0 x6
 (az  30, el -90): 0.3333333 x9, 0 x6                                                 (= 1/sqrt(9))
Meridian (directly under an ear-level speaker, -30 < el <= 0): exactly 1 on that speaker, 0 elsewhere — ear and construction agree (e.g. 7.1.4 az30 el-15).
```
**Not exact vs ear:** in the -30..0 band ear uses bilinear `QuadRegion`s (max |diff| 0.055 vs triplets on the seven layouts other than 5.1.4; on 5.1.4 up to 0.56, because ear's rear facets reach the height speakers and the lower-only hull deliberately does not copy them) — pin these by properties, not by ear.

### D. Criterion 3 characterisation (already green; run this session)
`RenderEngine::setOutputFormat (f)` for all 23 `OutputFormat`s: `layout.totalChannels == requiredChannels`, `(lfeChannelIndex >= 0) == hasLFE`, `layoutHasHeight == hasHeight` — **23/23, 0 mismatches**, triplet counts 25/72/45/118/235/190/355/188 for the eight height layouts, 0 for the rest. This cross-table test (registry x layout table x engine switch) is the strongest new criterion-3 check and needs no source change.

## Common Pitfalls

### Pitfall 1: Allocation / lock / log on the audio path (DR-1)
**What goes wrong:** the new selection code or sanitiser allocates (`std::vector` growth, `std::string`, `Logger`).
**Why it happens:** the lower-hemisphere data is "just another vector"; `sanitized_ = sources` looks like it might allocate.
**How to avoid:** all triplet data built in `activateLayout` (message thread); `computeVBAPGains3D` only reads the `const std::vector&`; `RenderSources` is trivially copyable; `jassertfalse` is Debug-only and the only diagnostic allowed (D-06c). The Release `std::abort()` lives in `activateLayout`, never in a function reachable from `renderBlock`.
**Warning signs:** any `push_back`/`resize`/`String` in `SpatialMath.cpp` or in `renderBlock`'s call tree.

### Pitfall 2: Frozen `SpatializationAlgorithm` / `LayoutContext` (DR-2/DR-7)
**What goes wrong:** adding a virtual method or a context member to carry EAR data or a "spread".
**How to avoid:** everything rides in `VBAPTriplet` (default-initialised fields) and free functions. `nearestSpeaker3DFallback` stays (public inline). Dual-band VBIP stays deferred (#20).
**Warning signs:** an edit to `SpatializationAlgorithm.h` or `Types.h:77-83`.

### Pitfall 3: OpenSpatialDelay must keep building (DR-3)
**What goes wrong:** a removed/renamed public symbol or a changed aggregate.
**Public surface OSD 30391cd uses (verified):** `spatialcore::evalSH` (declaration), `computeVBAPGains2D/3D` (declarations), `layoutHasHeight`, `buildVBAPTripletsForLayout`, `LayoutContext` 4-member aggregate init, `VBAPTriplet tri; tri.i/j/k/inv`, `RenderEngine::{setOutputFormat, prepare, getActiveLayout, getBinauralRenderer, renderBlock, ...}`. OSD defines its own *global-namespace* `static evalSH`, `computeVBAPGains2D/3D`, `nearestSpeaker3DFallback` and qualifies SpatialCore's with `::`. `[VERIFIED: OSD 30391cd Source/PluginProcessor.cpp:55-57, :1710, :1831, :2115, :2147, :2852]`
**How to avoid:** all new `VBAPTriplet` members have default initialisers; `setOutputFormat` stays `void`; keep `AmbisonicsCodec::evaluateSH` and `evalSH` both public. Add compile-only pins (`static_assert (std::is_same_v<decltype (&RenderEngine::setOutputFormat), void (RenderEngine::*) (OutputFormat)>)`, a `LayoutContext { l, t, m, n }` construction, `VBAPTriplet tri; tri.i = 1;`) in a new `ConsumerSurfaceTests.cpp`. As a manual phase gate, build OSD against the modified SpatialCore: `[ASSUMED]` `git worktree add /tmp/osd-check 30391cd`, replace the `SpatialCore` submodule directory with a symlink to this worktree, `cmake -S . -B build && cmake --build build --target OpenSpatialDelayTests` — not run here (heavy JUCE plugin build); the planner should schedule it as a human-run checkpoint.

### Pitfall 4: Golden vectors that will change
- `tests/Algorithms/SpatializationAlgorithmTests.cpp:203-219` VBIP golden `0.9330127 / 0.0669873` -> `0.8880738 / 0.4597008` (test title "squared VBAP, renormalized" -> "textbook VBIP").
- `:271` MDAP title citation 2000 -> 1999.
- MDAP behaviour on **height layouts** changes (below-horizon aux directions) — no existing test pins that; MDAP on 2D layouts is unchanged (its two existing tests are properties).
- No RenderEngine test pins VBIP or ambisonics beyond order 0 (`RenderEngineTests.cpp:250-281`).
- Regular-triplet counts and all above-horizon VBAP values are unchanged (bit-identical by construction) — a characterisation snapshot of ~50 above-horizon VBAP vectors taken **before** the edit is the cheapest proof.

### Pitfall 5: Dependency ordering (land in this order)
1. D-03 (one height predicate) **before** D-02(a) (Release crash) — else a 0.57-1.0 degree speaker turns a latent mismatch into an abort (F14).
2. Characterisation snapshots (old ambi decode copy, above-horizon VBAP vectors) **before** D-09 deletion and D-04.
3. D-08 fix -> then SN3D/round-trip/literal tests (they must fail red on the old constants first if the plan uses TDD; the table proves they do).
4. D-01 (delete branches) and D-04/D-06 (`computeVBAPGains3D`) touch the same three algorithm files and `SpatialMath.cpp`: one plan, or strictly sequential.
5. D-14 (VBIP) after D-04/D-06b (VBIP calls the shared function).
6. `RenderEngine.cpp` is touched by D-02a, D-04 (call), D-06a and D-09 — sequential plans or one plan; never parallel waves.
7. D-13 tests last (they assert the post-change behaviour).

### Pitfall 6: Asserting things that are not true
Mirror-symmetry and golden pins on height layouts inside tie regions (F5); `spread=0` (F3); unity for DBAP tighter than ~1e-3; sweeps "with no jumps" for KNN or height layouts off the ring; equality with ear inside the trapezoid band; DirectBinaural elevation/front-back cues (BUG-01, Phase 3, will change them) — DirectBinaural property checks are limited to L/R symmetry, ILD/ITD sign and monotonic magnitude vs |az|, distance-gain value, and `computeGains` == zeros.

### Pitfall 7: Saved sessions
Algorithm indices and `OutputFormat` enums do not change, so no data migration; only sound changes. (Not a rename/refactor phase in the Runtime-State-Inventory sense: **Stored data / live service config / OS-registered state / secrets / build artifacts — None: no string or identifier is renamed, verified by the decision list.**)

## Code Examples

### D-02(a) guard (replaces `RenderEngine.cpp:638`)
```cpp
// After buildVBAPTripletsForLayout (regular triplets only) and BEFORE appendLowerHemisphereTriplets.
// Layout-build path: message/prepare thread, allocation allowed, DR-1 does not apply (D-02).
if (buf.vbapTriplets.empty() == layoutHasHeight (buf.layout))   // height layout with no triplets, or 2D layout with triplets
{
    jassertfalse;     // Debug: stop at the cause
    std::abort();     // Release: the only way to reach here is a SpatialCore developer editing the table/builder
}
appendLowerHemisphereTriplets (buf.layout, buf.vbapTriplets);
```
(`#include <cstdlib>`.) `layoutHasHeight` and the builder must share one threshold (D-03): `inline constexpr float kHeightElevationThresholdRad = 0.0175f;` in `SpeakerLayout.h`, used by both `SpeakerLayout.cpp:94` and `:111`/`:117`. `0.0175f` is the value the public docblock already promises ("elevation > 1 degree", `SpeakerLayout.h`), and no shipped speaker sits in (0.01, 0.0175], so behaviour is unchanged for shipped layouts.

### D-11(b)/(c) tests
```cpp
TEST_CASE ("SH: SN3D addition theorem, sum over m of Y_lm^2 == 1, orders 1-6", "[ambisonics][sn3d]")
{ /* 2000 fixed-seed directions incl. poles; for l in 1..6: |sum_{acn=l*l}^{(l+1)^2-1} evalSH(acn,az,el)^2 - 1| <= 2e-5 */ }
TEST_CASE ("SH: evalSH matches 49 scipy reference values at az=64 el=10", "[ambisonics][sn3d][golden]")
{ for (int c = 0; c < 49; ++c) CHECK_THAT (evalSH (c, degToRad (64.f), degToRad (10.f)), WithinAbs (kShRef_az64_el10[c], 1e-5f)); }
TEST_CASE ("SH: AmbisonicsCodec::evaluateSH forwards to evalSH bit-for-bit", "[ambisonics]")
{ /* 49 channels x 500 directions: == */ }
```

### D-06 robustness sweep (every algorithm, both a 2D and a height layout)
```cpp
for each algo in instantiateAll (AllAlgorithmTypes{})   // pattern from tests/Core/CountsTests.cpp:14-21 — auto-covers a 9th algorithm
  for az in { NaN, +-inf, 1e30f, 1e9f, -1e9f }  for el in { 0, NaN, +-inf, 1e30f }:
      computeGains (...);   // must RETURN (test run has a watchdog: the 2D loop would hang today)
      CHECK all gains finite; CHECK power == 1 or all-zero
```
Use a `std::async` + `wait_for` watchdog (or a bounded-iteration proof by construction) so a regression fails instead of hanging CI. `[ASSUMED]` — not prototyped.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Nearest-speaker snap below the horizon | ITU-R BS.2127 / EAR: -30 degree virtual ring + nadir, 1/sqrt(n) downmix | ADM renderer (EBU) — `[CITED: ITU-R BS.2127-1 s6.1.2.2, s6.1.3.1 via CONTEXT]` | D-04 |
| VBIP as "squared VBAP gains" | `g_i = sqrt(G_i / sum G_i)` | Pernaux, Boussard & Jot, DAFx-98 `[CITED: dafx.de PER15]` | D-14 |
| Two SH implementations | One (`evalSH`) | this phase | D-08 |

**Deprecated/outdated:** `nearestSpeaker3DFallback` (kept, comment-deprecated, removal at next major); `RenderEngine::computeAmbiDecodeForLayout` (deleted).

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Layouts with a speaker below -10 degrees should get "no extras" rather than an error (none ship) | Pattern 1 | Low: no shipped layout; a future lower-layer layout would silently lose EAR |
| A2 | Root cause of the wrong SN3D constants (why those 22) is not recoverable from source; fix is constants-only | Reference Data B | None for the fix — verified numerically against scipy |
| A3 | `-ffast-math`-folded `isfinite` in header code would break OSD guards (reasoned from compiler flags, not built) | F9 | Medium: if wrong, header guards would also work; keeping guards in `.cpp` is safe either way |
| A4 | The OSD build check (worktree + symlinked submodule) works as sketched; not run | Pitfall 3 | Medium: planner should treat as a human-run checkpoint |
| A5 | numpy/scipy/ear wheels are the upstream publishers' (no hash check) | Package Legitimacy | Low: offline dev tooling, output committed as literals |
| A6 | A `std::async` watchdog is portable/acceptable in the Catch2 target | Code Examples | Low: alternative is a bounded-loop-by-construction proof |
| A7 | Optional epsilon in the min-sum comparison would make ties deterministic (not tried; changes above-horizon bits) | F5 | Only relevant if the user later wants it |
| A8 | `[CITED]` ITU-R BS.2127 section numbers and Zotter & Frank chapter are taken from CONTEXT's reference list; only `ear` source and the DAFx/ICMC papers were read this session | State of the Art | Low |

## Open Questions

1. **D-13 "360-degree continuity sweep, no jumps" on height layouts (F5).**
   - What we know: existing all-triples/min-sum gives 0.45-0.85 steps at el=20/40; exact ties; band jumps of ~0.10 also exist in the new lower band; KNN jumps by design.
   - What's unclear: whether the user wants the phase to *fix* the triangulation (out of the four approved audible changes) or scope the test.
   - Recommendation: scope the tests as in §Validation Architecture, file the ticket with these numbers, and record the exceptions in test comments.
2. **Sanitiser scope beyond the letter of D-06(a) (F7).** Adding the algorithm-layer guard, wrap of huge finite azimuths and `distance` hold-last-good goes slightly past D-06's text but is squarely inside its stated stance ("playback never crashes"). Recommend including; needs a one-line confirmation.
3. **`getDecodeMatrix` guard for `numSpeakers > MAX_SPEAKERS` (F6).** Recommended free hardening (no shipped caller can trigger it); confirm it is welcome in this phase.
4. **Release-notes wording for VBIP (F1):** "wider and up to 3 dB louder between speakers".
5. **Optional: deterministic tie epsilon** (A7) — not recommended in this phase.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | build | yes | 4.4.3 | — |
| Apple clang | build | yes | 21.0.0 | — |
| Existing `build/` dir (Debug, configured for this worktree) | tests | yes | `CMAKE_HOME_DIRECTORY` = this worktree | fresh configure fetches Catch2/JUCE/libmysofa (network) |
| Git LFS + 5 real SOFA files | binaural tests | yes | git-lfs 3.8.0; files 1.1-36.6 MB | — |
| python3 | reference generators | yes | 3.14.7 | — |
| numpy / scipy / ear in `.context/venv` | generators | yes (installed this session) | 2.5.3 / 1.18.1 / 2.1.0 | recipe above |
| `gh` | filing the tie-break ticket | yes | 2.101.0 | — |
| OpenSpatialDelay checkout for DR-3 | manual gate | yes | `~/conductor/repos/openspatialdelay-v1`, commit 30391cd | — |

**Commands (verified this session; runtime):**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug            # 1.5 s when configured
cmake --build build --target SpatialCoreTests -j8        # ~2 s incremental
./build/tests/SpatialCoreTests                           # ~8 s Debug; NOTE the tests/ subdir
./build/tests/SpatialCoreTests "[algorithms]"            # tag filter; also: "[io]" "[ambisonics]" "[engine]" "[spatialmath]" "[counts]" "[vbip]" ...
ctest --test-dir build --output-on-failure               # one process per TEST_CASE
```

## Validation Architecture

(`workflow.nyquist_validation` is absent from `.planning/config.json` => enabled.)

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.7.1 (FetchContent), CTest via `catch_discover_tests` |
| Config file | `tests/CMakeLists.txt` (explicit file list — add new `.cpp` by hand) |
| Quick run command | `./build/tests/SpatialCoreTests "[algorithms],[io],[ambisonics],[spatialmath],[counts]"` |
| Full suite command | `./build/tests/SpatialCoreTests` (expected: only the known HUTUBS golden-checksum failure) |
| Compile flags note | test target has JUCE_UNIT_TESTS=1, no fast-math; NaN/inf tests are valid here |

### Phase Requirements -> Test Map
| Req / Decision | Behavior | Test Type | Automated Command | File Exists? |
|----------------|----------|-----------|-------------------|-------------|
| D-01 | No nearest-speaker branch; empty triplets on a height layout asserts in Debug | code review + grep gate (`! grep -n nearestSpeaker3DFallback src/`) | `grep -rn "nearestSpeaker3DFallback" src/` returns nothing | no (add as plan verify step) |
| D-02(c) | height <=> non-empty **regular** triplets for all 15 layouts | unit | `./build/tests/SpatialCoreTests "[io][layout]"` | yes (`SpeakerLayoutTests.cpp:221`); strengthen to count regular only |
| D-02(a) | Release abort on mismatch | not unit-testable (abort); verified by inspection + the D-03 test | manual: code review of `activateLayout` | — |
| D-03 | one predicate: 0.8-degree centre speaker layout has `layoutHasHeight == !triplets.empty()` | unit `[io][layout][d03]` | `... "[d03]"` | no — Wave 0 |
| D-04 | (a) oracle vectors, tol 1e-5, incl. nadir 1/sqrt(n); (b) meridian == 1; (c) unit power below horizon; (d) horizon continuity `\|g(+0.05)-g(-0.05)\| <= 0.01`; (e) coverage: 200k fixed-seed directions incl. grid, every one finds a triplet (test-local finder over the vector) and the result power is 1 +- 2e-6; (f) VBAP/VBIP/MDAP all reach it via `RenderEngine` + `getActiveLayout()`; (g) above-horizon bit-identity against a pre-change snapshot | unit `[ear]` | `... "[ear]"` | no — Wave 0 (data via `gen_ear_reference.py`) |
| D-05 | Binaural/Ambisonics untouched | existing `[binaural]`, `[ambisonics]`, plus evalSH at el<0 keeps sign | full suite | yes |
| D-06(a) | NaN/inf/huge az,el hold last good; first-ever bad value -> (0,0) | unit `[engine][sanitize]` | `... "[sanitize]"` | no |
| D-06(b) | algorithm robustness sweep (finite result or zero, returns) for all algos x 2D/3D layout | unit `[algorithms][robust]` with watchdog | `... "[robust]"` | no |
| D-06(c) | Debug-only jassert | inspection | — | — |
| D-08 | evalSH == 49 scipy values (1e-5) | unit `[ambisonics][sn3d][golden]` | `... "[sn3d]"` | no |
| D-09 | new decode == old (test-local verbatim copy), all 15 layouts, tol 1e-6 | unit `[engine][ambi-pin]` | `... "[ambi-pin]"` | no — capture before deletion |
| D-10 | convention statement present in the 6 sites | grep gate | `grep -c "Condon-Shortley" include/SpatialCore/Core/SpatialMath.h README.md docs/integration-guide.md ...` | no |
| D-11(a) | round trip, orders 1-6, test-local dense decoder, tol 1e-5 (measured 6e-8) — smoke only | unit `[ambisonics][roundtrip]` | `... "[roundtrip]"` | no |
| D-11(b) | sum_m Y^2 = 1, orders 1-6, tol 2e-5 (measured 3.5e-6; buggy 19.5) | unit `[sn3d]` | as above | no |
| D-11(c) | 49 literals, tol 1e-5 | unit `[sn3d][golden]` | as above | no |
| D-13 | per algorithm (auto-enumerated via `AllAlgorithmTypes`): on-speaker unity (DBAP 2e-3; MDAP/KNN/ConstantPower per their laws), power 1 over sweeps, L/R mirror on 2D layouts (and unambiguous points on 3D), continuity (scoped, see below), textbook values (VBAP, VBIP, DBAP tol 1e-5; MDAP port tol 5e-4) | unit `[algorithms][panning-law]` | `... "[panning-law]"` | no |
| D-14 | VBIP golden regenerated; `sum g^2 = 1`; rE points at source | unit `[vbip]` | `... "[vbip]"` | edit existing |
| D-16/D-17 | docs + citation | grep gate | `grep -rn "Pulkki 2000" .` returns nothing outside `.planning` | — |
| Criterion 3 | 23 formats x engine switch: channels/LFE/height agree; `kLayoutExpectations` tied to `NUM_LAYOUT_DEFS` | unit `[io][layout][engine]` + `static_assert (sizeof (kLayoutExpectations) / sizeof (kLayoutExpectations[0]) == NUM_LAYOUT_DEFS)` | `... "[layout]"` | partly (add the static_assert at `SpeakerLayoutTests.cpp:133`) |
| DR-3 | consumer surface compiles | compile-only unit `[consumer-surface]` | build | no (optional) |

### Continuity / symmetry scoping (from the measured sweeps; thresholds are per 0.1-degree step)
| Algorithm | Layouts / elevation where "no jump" may be asserted | Measured max step today (real code) | Assert |
|-----------|------------------------------------------------------|-------------------------------------|--------|
| ConstantPower, Ambisonics, DBAP | all layouts, el 0/20/40 | <= 0.0102 | <= 0.02 |
| VBAP | all 2D layouts el=0; height layouts el=0 | 0.0053 | <= 0.01 |
| VBIP (after D-14) | same as VBAP | 0.0065 today; recompute after the fix | <= 0.02 (calibrate) |
| MDAP | 2D layouts el=0 (height layouts el=0: today 0.16-0.19, after D-04 expected <= 0.11 at 0.5-degree steps — assert only that it improved, e.g. `<= 0.12`) | 0.0015-0.0032 (2D) | <= 0.01 (2D) |
| KNN | none (inherent switching) — assert unit power and finite only | 0.03-0.54 | — |
| Lower hemisphere, VBAP | el <= -45 on layouts whose ear-level ring is dense (7.1.x, 9.1.x, SML): nadir cap | <= 0.004 (proto) | <= 0.01; the -30..0 band: assert <= 0.12 and reference the tie-break ticket |
Mirror symmetry (`az` <-> `-az` permutes gains by the layout's mirror map): assert on all 2D layouts and, for height layouts, only on points where `ambiguityGap > 1e-3` (helper: among triplets with all `g >= -1e-6`, no second triplet has `|sum_a - sum_b| < 1e-4` with gain vectors differing by `> 1e-3`).

### Sampling Rate
- **Per task commit:** `./build/tests/SpatialCoreTests "[<tag of the area touched>]"` (< 3 s).
- **Per wave merge:** full suite (~8 s Debug); expect exactly one failure, the deferred HUTUBS checksum.
- **Phase gate:** full suite green except the deferred item, plus the DR-3 OSD build checkpoint, before `/gsd-verify-work` (DR-4).
- Coverage/property tests use fixed RNG seeds (`std::mt19937_64 rng (42)`) and <= 200k points per layout (the 5.2M-point stress in this research took 110 s Debug and is a one-off).

### Wave 0 Gaps
- [ ] `tests/reference/` generators + generated headers (`ShReference.h`, `EarReference.h`, `PanningReference.h`) + README with the install recipe
- [ ] Characterisation snapshot of ~50 above-horizon VBAP vectors on 7.1.4 / 9.1.6 / 5.1.2 (taken pre-change) and the test-local `referenceAmbiDecode` (verbatim copy from `git show bf10fac:src/Engine/RenderEngine.cpp`, lines 653-734)
- [ ] Tags registered by use: `[ear]`, `[sn3d]`, `[roundtrip]`, `[sanitize]`, `[robust]`, `[panning-law]`, `[ambi-pin]`, `[d03]`, `[consumer-surface]`
- [ ] New test files (if any) added to `tests/CMakeLists.txt`
- [ ] File the tie-break GitHub issue (F5) and reference its number in the scoped-test comments

## Security Domain

`security_enforcement` is absent from config => enabled. This phase touches no auth/session/crypto; the relevant surface is untrusted numeric input reaching real-time code.

### Applicable ASVS Categories
| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — |
| V3 Session Management | no | — |
| V4 Access Control | no | — |
| V5 Input Validation | **yes** | `isfinite` + bounded wrap/clamp at engine entry and in `computeVBAPGains2D/3D`; sanitise `distance` too. Network-supplied `/azim`,`/elev`,`/aed`,`/xyz` are unclamped (`ADMOSCReceiver.cpp:56-77`). |
| V6 Cryptography | no | — |
| V11 Business Logic / resource limits | yes (availability) | Bounded loops only on the audio thread (replace `while (az > pi)`); no unbounded allocation |

### Known Threat Patterns for this stack
| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Non-finite / huge OSC value hangs the audio thread (`while` wrap loop never terminates) | Denial of service | `std::remainder`-based wrap + non-finite -> silence in the algorithm layer (F7); hold-last-good in the engine |
| NaN propagates into filter state (`smoothedNfcDistance`) and stays until reset | Denial of service / integrity | Sanitise `distance` as well as az/el |
| Out-of-bounds write in `getDecodeMatrix` for `numSpeakers > 16` | Tampering / DoS | Early return guard (not reachable from shipped callers) |
| Layout table edited to a state with no triplets | Integrity | D-02 test + Release abort (developer-only, D-02 rationale) |

## Sources

### Primary (HIGH confidence — run or read this session)
- Real repository code and tests at `/Users/andrewrahman/conductor/workspaces/SpatialCore/kelowna` (branch `gsd-remap`, HEAD `bf10fac`): `src/Core/SpatialMath.cpp`, `src/IO/SpeakerLayout.cpp`, `src/IO/AmbisonicsCodec.cpp`, `src/Engine/RenderEngine.cpp`, `include/SpatialCore/Engine/RenderEngine.h`, `include/SpatialCore/Core/Types.h`, `include/SpatialCore/IO/SpeakerLayout.h`, the 8 algorithm sources, `tests/**`, `CMakeLists.txt`, `.github/workflows/*.yml`.
- OpenSpatialDelay consumer `~/conductor/repos/openspatialdelay-v1` at 30391cd (`Source/PluginProcessor.cpp`, `CMakeLists.txt`, `.gitmodules`) — read-only (DR-3).
- PyPI `ear` 2.1.0 source (`ear/core/point_source.py`: `_configure_full`, `VirtualNgon.handle`, `extra_pos_vertical_nominal`, `QuadRegion`) and executed oracle values.
- scipy 1.18.1 `lpmv`, `sph_harm_y`; numpy 2.5.3.
- Lossius et al., ICMC 2009, DBAP — https://jamoma.org/publications/attachments/icmc2009-dbap-rev1.pdf (eq 2-5 read via pdftext).
- Pernaux, Boussard & Jot, DAFx-98 — https://www.dafx.de/paper-archive/1998/PER15.PS.pdf (s2.2.1-2.2.2 read via pdftext).
- Scratch harnesses and numerics: `.context/research/` (gitignored, this worktree only): `s_sweep.cpp`, `s_nan.cpp`, `s_ambi.cpp`, `s_ear_proto.cpp`, `s_thr.cpp`, `s_fmt.cpp`, `proto2.py`, `proto3.py`, `roundtrip*.py`, `cmp_sh.py`, `ref/gen_*.py`.

### Secondary (MEDIUM)
- ITU-R BS.2127-1 s6.1.2.2 / s6.1.3.1, Pulkki WASPAA 1999, Zotter & Frank 2019 ch. 3 — cited from CONTEXT's canonical list, not re-read.

### Tertiary (LOW)
- None used as authority.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new C++ dependencies; Python tooling pinned and executed.
- Architecture: MEDIUM-HIGH — the D-04 construction, `computeVBAPGains3D` structure and coverage were prototyped in scratch C++ (float) and numpy, not in the repo; DR-1/DR-2/DR-3 constraints checked against real OSD code.
- Pitfalls: HIGH — F1-F15 each reproduced with the real binary or an independent oracle.

**Research date:** 2026-09-30
**Valid until:** 2026-10-30 (code is under active change on this branch; re-verify F1/F5 line numbers after any merge into `gsd-remap`)

---

## Appendix: reference generators (copy to `tests/reference/`; regenerate the headers, do not trust transcriptions above)

### `gen_sh_reference.py`
```python
#!/usr/bin/env python3
"""Reference values for SpatialCore evalSH: real SH, ACN order, SN3D, NO Condon-Shortley phase.
Offline tool. NOT run in CI. Requires numpy + scipy (see README pins).
Convention: az=0 front, +az toward +Y (left), radians; el=0 horizon, +el up.
  ACN = l*l + l + m ; m>0 -> cos(m*az) ; m<0 -> sin(|m|*az)."""
import math, sys
import numpy as np
from scipy.special import lpmv, sph_harm_y, factorial

def sn3d(acn, az, el):                       # route 1: associated Legendre (CS phase removed)
    l = math.isqrt(acn); m = acn - l*l - l; am = abs(m)
    P = ((-1)**am) * lpmv(am, l, np.sin(el))
    N = math.sqrt((2 - (1 if m == 0 else 0)) * factorial(l-am) / factorial(l+am))
    return N * P * (np.cos(am*az) if m >= 0 else np.sin(am*az))

def sn3d_route2(acn, az, el):                # route 2: scipy complex SH -> real SH -> SN3D scale
    l = math.isqrt(acn); m = acn - l*l - l
    Y = sph_harm_y(l, abs(m), np.pi/2 - el, az)
    r = Y.real if m == 0 else (math.sqrt(2)*Y.real*((-1)**m) if m > 0 else math.sqrt(2)*Y.imag*((-1)**(-m)))
    return r * math.sqrt(4*np.pi/(2*l+1))

if __name__ == "__main__":
    az_deg, el_deg = 64.0, 10.0
    az, el = math.radians(az_deg), math.radians(el_deg)
    ref = [float(sn3d(c, az, el)) for c in range(49)]
    alt = [float(sn3d_route2(c, az, el)) for c in range(49)]
    assert max(abs(a-b) for a, b in zip(ref, alt)) < 1e-12, "two scipy routes disagree"
    rng = np.random.default_rng(1); A = rng.uniform(-np.pi, np.pi, 4000); E = np.arcsin(rng.uniform(-1, 1, 4000))
    for l in range(7):                        # addition theorem: sum_m Y_lm^2 == 1 (SN3D)
        s = sum(sn3d(c, A, E)**2 for c in range(l*l, (l+1)**2)); assert abs(s-1).max() < 1e-12
    print(f"// az={az_deg} deg, el={el_deg} deg ; generated by gen_sh_reference.py (scipy {__import__('scipy').__version__})")
    print("static constexpr float kShRef_az64_el10[49] = {")
    for c in range(49): print(f"    {ref[c]:.9f}f,  // ACN {c}")
    print("};")
    print(f"// min |Y| = {min(abs(v) for v in ref):.6f} at ACN {int(np.argmin(np.abs(ref)))}", file=sys.stderr)
```

### `gen_ear_reference.py`
```python
#!/usr/bin/env python3
"""Oracle gains for the D-04 lower-hemisphere construction, from the EBU ADM Renderer (PyPI `ear` 2.1.0).

Only directions that ear handles with its VirtualNgon (nadir cap) region are emitted as exact oracle values.
Where ear uses a bilinear QuadRegion (the -30..0 band) SpatialCore uses triplets, and the two differ by up to ~0.055,
so those directions are NOT pinned against ear.  Also asserts the 'meridian' case (directly under an ear-level speaker)
where both agree exactly: gain 1 on that speaker.

Offline tool, NOT run in CI.  Layouts are parsed from src/IO/SpeakerLayout.cpp so the oracle follows the real table.
"""
import warnings
warnings.filterwarnings("ignore")
import numpy as np
from ear.core import point_source
from ear.core.layout import Layout, Channel
from ear.core.geom import PolarPosition
from ear.core.point_source import VirtualNgon
from layouts_from_cpp import load


def ear_panner(spk):
    ch = [Channel(name=("T+000" if el == 90 else f"S{i:02d}"), polar_position=PolarPosition(az, el, 1.0))
          for i, (az, el, _c) in enumerate(spk)]
    return point_source.configure(Layout(name="custom", channels=ch))


def ear_gains(panner, az, el):
    """Returns (gains in layoutDefs speaker order, handled_by_VirtualNgon).
    ear: +az = left, x = right (mirror of SpatialCore's x axis) -- gains are unaffected by the mirror."""
    a, e = np.radians(az), np.radians(el)
    pos = np.array([np.sin(-a) * np.cos(e), np.cos(a) * np.cos(e), np.sin(e)])
    for r in panner.psp.regions:
        pv = r.handle_remap(pos, panner.psp.num_channels)
        if pv is not None:
            g = np.dot(panner.downmix, pv)
            return g / np.linalg.norm(g), isinstance(r, VirtualNgon)
    raise RuntimeError("ear found no region")


CASES = {
    "S5_1_2": [(30, -60), (180, -60), (-100, -75), (30, -90)],
    "S7_1_4": [(30, -60), (0, -45), (180, -70), (-100, -75), (30, -90)],
    "S9_1_6": [(45, -75), (-90, -60), (120, -50), (30, -90)],
}

if __name__ == "__main__":
    L = load()
    for name, pts in CASES.items():
        spk = L[name]["spk"]
        p = ear_panner(spk)
        print(f"// {name}: gains are in layoutDefs speaker order ({len(spk)} speakers)")
        for az, el in pts:
            g, ngon = ear_gains(p, az, el)
            assert ngon, f"{name} ({az},{el}) is not in ear's VirtualNgon region"
            print(f"{{ {az:g}.f, {el:g}.f, {{ " + ", ".join(f"{v:.7f}f" for v in g) + " } },")
        g, _ = ear_gains(p, spk[0][0], -15.0)
        assert abs(g[0] - 1.0) < 1e-9, "meridian case must be exactly 1 on the speaker above"
```

### `gen_panning_reference.py`
```python
#!/usr/bin/env python3
"""Textbook reference values for the panning-law tests (D-13).  Offline tool, NOT run in CI.  Needs numpy only.

  VBAP  Pulkki 1997 / Pernaux-Boussard-Jot DAFx-98 s2.2.1 :  g = L^-1 p, then g / ||g||_2
  VBIP  Pernaux-Boussard-Jot DAFx-98 s2.2.2 (single band)  :  g_i = sqrt(G_i / sum G_i), G = L^-1 p
                                                             == sqrt(VBAP gains) renormalised to sum g^2 = 1
  DBAP  Lossius et al. ICMC 2009 eq (2)-(5)                :  v_i = k / d_i^a, a = R/(20 log10 2), k = 1/sqrt(sum d_i^-2a)
        SpatialCore effective R = 12.04 dB (a = 2), spatial blur r_s = 0, d^2 clamped >= 0.001, speaker radius 1.
  MDAP  Pulkki, WASPAA 1999: NOT a closed-form law.  The value is a PORT of the code's construction
        (main + 8-point ring at alpha = clamp(0.9*180/N, 5, 30) deg, amplitude-summed VBAP, L2-normalised) -- it is a
        cross-check of the ring geometry, not an independent oracle.  Depends on ring start phase (~1% of gain).
Conventions match SpeakerLayout.cpp: +azimuth = left, x = cos(el) sin(az), y = cos(el) cos(az), z = sin(el).
"""
import numpy as np

def vec(az, el=0.0):
    az, el = np.radians(az), np.radians(el)
    return np.array([np.cos(el)*np.sin(az), np.cos(el)*np.cos(az), np.sin(el)])

def vbap_2d(spk, az):
    """Adjacent-pair VBAP; returns gains in `spk` order."""
    order = np.argsort(spk)
    for a in range(len(spk)):
        i, j = order[a], order[(a+1) % len(spk)]
        L = np.array([vec(spk[i])[:2], vec(spk[j])[:2]])
        g = np.linalg.solve(L.T, vec(az)[:2])
        if (g >= -1e-12).all():
            out = np.zeros(len(spk)); out[i], out[j] = g
            return out / np.linalg.norm(out)

def vbip_2d(spk, az):
    h = np.sqrt(vbap_2d(spk, az)); return h / np.linalg.norm(h)

def vbap_3d_triplet(spk_el_az, tri, az, el):
    """Textbook VBAP for a GIVEN triplet of (az, el) speakers (caller picks an unambiguous triplet)."""
    L = np.array([vec(*spk_el_az[k]) for k in tri]).T
    g = np.linalg.solve(L, vec(az, el)); assert (g >= 0).all()
    return g / np.linalg.norm(g)

def dbap(spk, az, el=0.0, dist=1.0, eps=1e-3):
    s = dist * vec(az, el)
    w = np.array([1.0 / max(eps, np.sum((s - vec(a))**2)) for a in spk])   # d^-2 used as AMPLITUDE (a = 2)
    return w / np.linalg.norm(w)

def mdap_2d(spk, az):
    N = len(spk); alpha = np.radians(np.clip(0.9*180/N, 5, 30))
    p = vec(az); A = np.array([p[1], -p[0], 0.0]); A /= np.linalg.norm(A)
    tot = vbap_2d(spk, az).copy()
    for i in range(8):
        phi = 2*np.pi*i/8
        rot = A*np.cos(phi) + np.cross(p, A)*np.sin(phi) + p*(p@A)*(1-np.cos(phi))
        q = p*np.cos(alpha) + np.cross(rot, p)*np.sin(alpha) + rot*(rot@p)*(1-np.cos(alpha))
        tot += vbap_2d(spk, np.degrees(np.arctan2(q[0], q[1])))
    return tot / np.linalg.norm(tot)

if __name__ == "__main__":
    np.set_printoptions(precision=7, suppress=True)
    quad = [45, -45, 135, -135]; l50 = [30, -30, 0, 110, -110]            # SpeakerLayout.cpp layoutDefs order
    print("VBAP quad az30 ", vbap_2d(quad, 30));  print("VBIP quad az30 ", vbip_2d(quad, 30))
    print("VBAP 5.0 az10  ", vbap_2d(l50, 10));   print("VBIP 5.0 az10  ", vbip_2d(l50, 10))
    print("VBAP 5.0 az50  ", vbap_2d(l50, 50));   print("VBIP 5.0 az50  ", vbip_2d(l50, 50))
    print("DBAP quad az30 dist1   (code order 45,-45,135,-135)", dbap(quad, 30, dist=1.0))
    print("DBAP quad az30 dist0.5 (existing golden 0.9390509 0.2691336 0.1768006 0.1203831)", dbap(quad, 30, dist=0.5))
    print("MDAP quad az30 ", mdap_2d(quad, 30));  print("MDAP 5.0 az10  ", mdap_2d(l50, 10))
    s714 = [(30,0),(-30,0),(0,0),(90,0),(-90,0),(135,0),(-135,0),(45,45),(-45,45),(135,45),(-135,45)]
    for (az, el, tri) in ((0, 30, (2, 7, 8)), (60, 20, (0, 3, 7)), (10, 10, (0, 2, 7)), (90, 30, (3, 7, 9))):
        print(f"VBAP 7.1.4 az{az} el{el} triplet{tri}", vbap_3d_triplet(s714, tri, az, el))
```

### `layouts_from_cpp.py`
```python
import re, numpy as np
NAMES=["Quad","S5_0","S5_1","S7_0","S7_1","S9_1","S5_1_2","S5_1_4","S7_1_2","S7_1_4","S7_1_6","S9_1_4","S9_1_6","Octaphonic","SML13_1"]
def load():
    src=open(__import__('os').path.join(__import__('os').path.dirname(__file__),'..','..','..','src','IO','SpeakerLayout.cpp')).read()
    body=src[src.index('layoutDefs[NUM_LAYOUT_DEFS] = {'):src.index('static SpeakerLayout makeLayoutFromDef')]
    body=re.sub(r'//[^\n]*','',body)
    out=[]
    # each layout: { n, lfe, total, { {az,el,ch}, ... } }
    for m in re.finditer(r'\{\s*(\d+),\s*(-?\d+),\s*(\d+),\s*\{(.*?\}),?\s*\}\s*\}',body,flags=re.S):
        n,lfe,tot,spk=int(m.group(1)),int(m.group(2)),int(m.group(3)),m.group(4)
        s=[tuple(int(x) for x in t) for t in re.findall(r'\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(\d+)\s*\}',spk)]
        assert len(s)==n,(n,len(s))
        out.append(dict(n=n,lfe=lfe,total=tot,spk=s))
    assert len(out)==15
    return dict(zip(NAMES,out))
def vec(az,el):
    az,el=np.radians(az),np.radians(el)
    return np.array([np.cos(el)*np.sin(az),np.cos(el)*np.cos(az),np.sin(el)])
if __name__=="__main__":
    L=load()
    for k,v in L.items(): print(k,v['n'],v['lfe'],v['total'],[(a,e) for a,e,c in v['spk']])
```
