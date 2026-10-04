# Phase 2: Algorithm & Format Verification - Context

**Gathered:** 2026-09-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Confirm by test that every algorithm and every advertised format is correct, and write the
Ambisonics convention down. **This phase closes named gaps; it does not rebuild the modules.**
Algorithms, IO and Ambisonics already have passing tests, and the counts stay as frozen by
P1 D-04: 8 algorithms, 23 output formats, 15 speaker layouts, 5 SOFA HRTF profiles.

Unlike Phase 1, **this phase changes behavior.** Verification found real defects, and the user chose
to fix them rather than pin them. Four changes are audible to OpenSpatialDelay (OSD) sessions and
each was explicitly approved as a fix, not a regression:
1. Below-horizon sources on height layouts get a smooth EAR lower-hemisphere pan instead of a
   silent snap to one speaker (D-04).
2. VBIP becomes textbook VBIP, which is **wider** than today's output (D-14).
3. 4OA-6OA Ambisonics output becomes correct; 22 of 49 channels currently carry the wrong
   constant scale (D-08).
4. Non-finite az/el no longer reach the panner (D-06).

Everything else is comments, docblocks, layout-build hardening and tests.

Requirements and success criteria served:
- **EXTR-01** — success criterion 2 (triplet fallback fails loudly or no longer exists). D-01 to D-07.
- **VERIFY-01** — success criterion 1 (Ambisonics convention stated in code and docs, SpatialCore#11).
  D-08 to D-10.
- **EXTR-03** — success criterion 4 (round-trip within tolerance at every order up to 6). D-11, D-12.
- ROADMAP overview line 27 ("verified against the panning laws") has no success criterion of its
  own. D-13 to D-17 give it a meaning.
- Success criterion 3 (23 formats, 15 layouts resolve to correct info) is **already met** by the
  golden table in `tests/IO/SpeakerLayoutTests.cpp:87` plus five all-layout loops. The only residual
  is tying that table to `NUM_LAYOUT_DEFS` (see Claude's Discretion).

</domain>

<decisions>
## Implementation Decisions

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
  **Amended 2026-10-04 (user accepted code-review fixes WR-03 / IN-14; see 02-VERIFICATION.md
  overrides):** (b) still picks the largest-min-gain triplet, but if that candidate clamps to all
  zeros the nearest speaker now gets unity instead of silence. (c) is withdrawn: JUCE's assert path
  logs and allocates, so no assert runs in `computeGains` (VBAP/VBIP/MDAP or `computeVBAPGains3D`).
  Both cases are reachable only with a hand-built partial triplet list, never through RenderEngine,
  whose layout build enforces coverage with the D-02(a) abort on the message thread.

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

### Planning-Time Amendments (2026-09-30, after 02-RESEARCH.md)

Decided by the user at plan-phase on the research's Open Questions 1-3. Corrections to numbers
above come from the research findings named in brackets; the research is the source of truth for them.

- **D-18:** **Continuity tests are scoped, not universal; the triangulation is not fixed in this
  phase.** Existing 3D VBAP has exact coplanar ties that flip on float rounding (0.45-0.85 gain jumps
  at el=20/40, RESEARCH F5). D-13's "360 degree sweep, no jumps" is asserted only where the algorithm
  is continuous (RESEARCH §Validation Architecture, continuity scoping table). The tie-break GitHub
  issue is filed in this phase with the F5 measurements and referenced from the scoped tests.
- **D-19:** **The non-finite guard lives in both layers** (extends D-06(a), RESEARCH F7). OSD calls
  `algo->computeGains` directly, so the engine sanitiser alone does not protect it, and
  `computeVBAPGains2D` hangs on `+inf` or azimuth >= ~1e9 rad. (i) Algorithm layer:
  `computeVBAPGains2D/3D` return silence for non-finite az/el and wrap large finite angles with a
  bounded `std::remainder`/`fmod` instead of the unbounded `while` loops. (ii) Engine layer: D-06(a)
  hold-last-good covers az, el **and distance**. Every `isfinite` guard goes in a SpatialCore `.cpp`,
  never a header inline (OSD compiles with `-ffast-math`, RESEARCH F9).
- **D-20:** **`AmbisonicsCodec::getDecodeMatrix` gets an early return when
  `numSpeakers > MAX_SPEAKERS`** (out-of-bounds write on public API, RESEARCH F6). No behaviour change
  for valid input.
- **Corrections carried from research (not new decisions):** DBAP D-13 values are right but in code
  speaker order `45, -45, 135, -135` they are `0.9984304, 0.0459007, 0.0270259, 0.0173052` (F2).
  Today's VBIP output at Quad az30 is `0.9330127 / 0.0669873` and its power is not 1, so D-14 also
  raises VBIP level by up to 3 dB between speakers (F1). "MDAP spread=0 equals VBAP" is untestable
  (no spread parameter, F3); replaced by a numpy-port cross-check plus properties. D-04 uses the
  lower-hemisphere-only hull (ear-level speakers + their -30 degree copies + nadir), stored as
  flagged extra triplets built outside `buildVBAPTripletsForLayout` (F4, F11).

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

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Governing plan documents
- `.planning/ROADMAP.md` §"Phase 2: Algorithm & Format Verification" (lines 65-80) — the four
  success criteria; overview line 27 is the only source of "verified against the panning laws".
- `.planning/REQUIREMENTS.md` — EXTR-01 (138-141), EXTR-03 (148-150), VERIFY-01 (325-327).
- `.planning/PROJECT.md` — DR-1 through DR-7 locked rules (176-182).
- `.planning/phases/01-documentation-truth-contract-freeze/01-CONTEXT.md` — P1 D-04 (frozen
  counts), P1 D-10 and P1 D-13 (`static_assert` + Catch2, no new CI machinery).
- `.planning/codebase/TESTING.md` — existing test layout and conventions.
- `.planning/codebase/CONCERNS.md` (line 111) — the SH-evaluator concern, SpatialCore#11.

### Standards and primary sources
- ITU-R BS.2127-1, https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.2127-1-202311-I!!PDF-E.pdf
  §6.1.2.2, §6.1.3.1 — lower-hemisphere handling (D-04).
- EBU ADM Renderer `ear/core/point_source.py`,
  https://github.com/ebu/ebu_adm_renderer/blob/master/ear/core/point_source.py
  (`_configure_full`, `VirtualNgon.handle`, `extra_pos_vertical_nominal`). PyPI `ear` 2.1.0 is the
  numeric oracle.
- Pernaux, Boussard & Jot, DAFx-98, https://www.dafx.de/paper-archive/1998/PER15.PS.pdf, Sec. 2.2.2 —
  the VBIP definition (D-14, D-15).
- Lossius et al., ICMC 2009, DBAP, https://jamoma.org/publications/attachments/icmc2009-dbap-rev1.pdf
  (D-16).
- Pulkki, "Uniform spreading of amplitude panned virtual sources", IEEE WASPAA 1999,
  https://research.aalto.fi/en/publications/uniform-spreading-of-amplitude-panned-virtual-sources/
  (D-17).
- Politis VBAP library, https://github.com/polarch/Vector-Base-Amplitude-Panning (`vbip.m`) —
  independent reference implementation used to cross-check the VBIP formula (D-14).
- Zotter & Frank 2019, ch. 3, https://link.springer.com/content/pdf/10.1007/978-3-030-17207-7_3 —
  imaginary-loudspeaker practice for VBAP/VBIP/MDAP (discard vs. 1/sqrt(M) downmix), background
  for D-04.

### Issues
- GitHub `AndrewRahman/SpatialCore#11` — two SH evaluators / convention (criterion 1).
- GitHub `AndrewRahman/SpatialCore#10` — spread API, SPRD-01, v2.
- GitHub `AndrewRahman/SpatialCore#20` — focus/spread, VBAP+VBIP unification, dual-band VBIP.
  Created during this discussion; cross-linked to #10.

### Code — the change sites
- `src/Algorithms/VBAPAlgorithm.cpp:19`, `VBIPAlgorithm.cpp:20,30`, `MDAPAlgorithm.cpp:8,48,101` —
  fallback branches (D-01), VBIP squaring (D-14), citation (D-17).
- `src/Core/SpatialMath.cpp:10` (`evalSH`), `:~197-225` (`computeVBAPGains2D`), `:223-291`
  (`computeVBAPGains3D`, fallback at `:272-289`).
- `include/SpatialCore/Core/SpatialMath.h:40-66` (`nearestSpeaker3DFallback`), `:71` (`evalSH`
  declaration).
- `src/IO/AmbisonicsCodec.cpp:13` (`evaluateSH`), `:139-222` (`getDecodeMatrix`);
  `include/SpatialCore/IO/AmbisonicsCodec.h:12-13` (`MAX_AMBI_ORDER=6`, `MAX_AMBI_CHANNELS=49`),
  `:23`.
- `src/Engine/RenderEngine.cpp:131-135` (`computeObjectGains`), `:450`, `:560-563`
  (`setOutputFormat`), `:575-644` (`activateLayout`, guard at `:638`), `:649-745`
  (`computeAmbiDecodeForLayout`, to be deleted), `:662`.
- `src/IO/SpeakerLayout.cpp:19-66` (`layoutDefs`), `:94`, `:111`, `:117` (thresholds).
- `src/IO/OutputFormatRegistry.cpp:39-45` — FOA..6OA entries, "AmbiX ACN/SN3D".
- `src/OSC/ADMOSCReceiver.cpp:62-77`, `src/Trajectory/TrajectoryEngine.cpp:188,222,294` — the
  user-reachable below-horizon inputs.

### Tests to extend
- `tests/IO/AmbisonicsCodecTests.cpp`, `tests/Algorithms/SpatializationAlgorithmTests.cpp`,
  `tests/Engine/RenderEngineTests.cpp`, `tests/IO/SpeakerLayoutTests.cpp`,
  `tests/Core/CountsTests.cpp`.

### Consumer (read-only, DR-3)
- OpenSpatialDelay migration branch, commit `30391cd`, in `~/conductor/repos/openspatialdelay-v1`:
  `PluginProcessor.cpp:1703-1712` (`requestOutputFormatChange`, message thread), `:1829-1831`
  (`prepareToPlay`), `:2115` (its own `nearestSpeaker3DFallback`).

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **`RenderEngine` double buffer** (`RenderEngine.h:241-248` `LayoutState`, `:378-380`
  `layoutBuffers` / `activeLayoutIndex` / `prepareLayoutIndex`; publish at `RenderEngine.cpp:613-614`,
  `627-628`, `641-642`). The EAR virtual speakers and triplets (D-04) are precomputed into
  `LayoutState` and published by the existing atomic swap. No new synchronisation.
- **`AmbisonicsCodec::getDecodeMatrix`** (`AmbisonicsCodec.cpp:139-222`) — already computes the
  decode; D-09 makes `activateLayout` its consumer instead of a private copy.
- **`SpeakerLayout` `layoutDefs`** (`SpeakerLayout.cpp:19-66`) — the layout table the guard (D-02) and
  threshold constant (D-03) protect.
- **`tests/IO/SpeakerLayoutTests.cpp:221-235`** — "height layout ⇔ non-empty triplets" already exists;
  D-02's primary catch needs no new test.
- **`tests/Core/CountsTests.cpp`** and the five all-layout loops — already carry the P1 count
  contract into this phase.

### Established Patterns
- **Stateless algorithms, context structs** (CLAUDE.md). D-04 state lives in `LayoutState`; D-06's
  last-good-position lives per-object. Algorithms stay pure.
- **Facade boundary (SC-13).** Consumers drive `RenderEngine` only. D-06(a) sanitizes at engine
  entry, which is the one place every consumer path passes through.
- **Golden vectors pin regressions.** The existing ones are pre-move OSD formulas. D-13 adds
  correctness pins beside them, and the VBIP golden vectors change with D-14.
- **Off-audio-thread allocation is allowed.** `activateLayout` allocates via
  `push_back` today; D-02(a) and D-09 rely on that.

### Integration Points
- `RenderEngine::activateLayout` (`RenderEngine.cpp:575-644`) — the crash guard (D-02), the shared
  threshold (D-03), the EAR precompute (D-04), and the `getDecodeMatrix` adapter (D-09) all land here.
- `RenderEngine::computeObjectGains` (`RenderEngine.cpp:131-135`) — az/el sanitization (D-06a).
- `computeVBAPGains3D` (`SpatialMath.cpp:223-291`) — the below-horizon path and best-triplet
  replacement (D-04, D-06b).
- `evalSH` / `AmbisonicsCodec::evaluateSH` — the SN3D fix and forwarder (D-08).
- OSD call sites (`PluginProcessor.cpp:1703-1712`, `:1829-1831`) — unchanged; `setOutputFormat` stays
  `void` (D-02b).

### Constraints Carried In
- **P1 D-04:** 8 algorithms / 23 formats / 15 layouts / 5 HRTF profiles. Nothing here changes a count;
  the D-04 virtual speakers are internal and are not layouts.
- **DR-1:** no alloc, lock or log on the audio path. D-02(a)'s crash is on the message thread and D-04's
  storage is preallocated; D-06(c)'s `jassertfalse` is Debug-only and is the one diagnostic permitted. *(Amended 2026-10-04: D-06(c) withdrawn, so no audio-path diagnostic remains; see D-06.)*
- **DR-2 / DR-7:** the `SpatializationAlgorithm` interface is frozen. Dual-band VBIP is deferred
  precisely because it cannot be expressed through `computeGains`.
- **DR-3:** OSD must keep building. Both `evalSH` and `AmbisonicsCodec::evaluateSH` survive as public
  names (D-08); `nearestSpeaker3DFallback` stays (Discretion); `setOutputFormat` stays `void`.
- **P1 D-10 / P1 D-13:** `static_assert` + Catch2, no new CI machinery. The scipy generator (D-11c) is
  checked in and not run in CI.
- **DR-4:** full suite before tagging — the enforcement mechanism for every test added here.

</code_context>

<specifics>
## Specific Ideas

- **"Keep VBIP as the original intended VBIP or we're lying."** The user's stated reason for D-14.
  Honesty of the label is the criterion; the docs and the code must describe the same algorithm.
- **"For now because it's honest."** The user's reason for single-band VBIP (D-15). Ship the half of
  the paper that fits the frozen interface, and say so.
- **"Who can cause this?"** The user's challenge that reframed D-02 from an API problem into a
  developer-error guard.
- **Playback never crashes.** The stance behind D-06: a layout-build failure may crash because only a
  developer can cause it; a position can never crash because a user or a network peer can cause it.
- **VBIP versus VBAP** (the user's exploratory question): same speakers, exponent 1/2. VBAP aims the
  velocity vector rV (low frequencies); VBIP aims the energy vector rE (high frequencies). This is what
  produced SpatialCore#20.
- **Oracle discipline.** Every new reference value comes from something independent of the code under
  test: scipy for SH, PyPI `ear` 2.1.0 for the lower hemisphere, the published formula for VBAP/VBIP/DBAP.

</specifics>

<deferred>
## Deferred Ideas

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

</deferred>

---

*Phase: 2-algorithm-format-verification*
*Context gathered: 2026-09-30*
