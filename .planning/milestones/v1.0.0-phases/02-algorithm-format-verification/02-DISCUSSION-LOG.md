# Phase 2: Algorithm & Format Verification - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-09-30
**Phase:** 02-algorithm-format-verification
**Areas discussed:** Triplet-fallback disposition; Two divergent SH evaluators; Ambisonics round-trip method; Meaning of "verified against the panning laws"

Every file:line reference was verified by agents during the discussion.

---

## Area 1: Triplet-fallback disposition (success criterion 2, EXTR-01)

### Q1 — What to do with the empty-triplets nearest-speaker branch?

Sites: `VBAPAlgorithm.cpp:19`, `VBIPAlgorithm.cpp:20`, `MDAPAlgorithm.cpp:48` and `:101`.

| Option | Description | Selected |
|--------|-------------|----------|
| A: delete + harden upstream | Remove the branch; harden the layout-prepare path instead | ✓ |
| Patch in place | Keep the branch, fix its behaviour | |
| Keep | Leave as is | |

**User's choice:** A — delete the branch, harden upstream.
**Notes:** `jassertfalse` is Debug-only, so in Release the branch silently dumps all energy into one
speaker. The branch is structurally unreachable with shipped layouts: every height layout builds
25-355 triplets (5.1.2 = 25, 7.1.4 = 118, 9.1.6 = 355). -> CONTEXT D-01.

### Q2 — What should the layout-build guard do when a height layout yields no triplets?

**Reframe.** The question was first asked with the options *reject and return a bool*, *force 2D*,
*crash*, and *log*. The user challenged the premise: **"who can cause this?"** Agents then established
that the condition is unreachable from the public API. `OutputFormat` is a closed enum, `activateLayout`
is private, and there is no custom-layout entry point into the engine, so only a SpatialCore developer
editing the layout table or the triplet builder can cause it. The original four options (each of which
was a runtime or API answer to a developer-only condition) were discarded and replaced by:

| Option | Description | Selected |
|--------|-------------|----------|
| A: test only | Catch2 check only, no Release guard | |
| B: test + Release crash | Existing Catch2 check plus an unconditional Release crash in `RenderEngine::activateLayout`, replacing the Debug-only `jassert` at `RenderEngine.cpp:638` | ✓ |

**User's choice:** B.
**Notes:**
- The crash is on the layout-build path (message/prepare thread, allocates via `push_back`), so DR-1
  does not apply.
- `setOutputFormat` stays `void`; the earlier "return a bool" idea is dropped, so there is no API change.
- The existing check `tests/IO/SpeakerLayoutTests.cpp:221-235` ("height layout ⇔ non-empty triplets") is
  the primary catch.
- Folded in: unify the mismatched height thresholds, `layoutHasHeight` `0.0175f`
  (`SpeakerLayout.cpp:94`) versus `buildVBAPTripletsForLayout` `0.01f` (`SpeakerLayout.cpp:111`, `:117`),
  into one shared constant. The latent mismatch trips `:638` for any speaker elevated 0.57-1.0 degrees.
-> CONTEXT D-02, D-03.

### Q3 — How should a below-horizon source behave on height layouts with no lower speakers?

**Context.** This is a second silent nearest-speaker fallback, inside `computeVBAPGains3D`
(`src/Core/SpatialMath.cpp:272-289`), and it is user-reachable: the OSD elevation parameter (-90..90),
ADM-OSC `/elev`, `/aed` and `/xyz` with no clamp (`ADMOSCReceiver.cpp:62-77`), and the
Bounce/Cross/Helix/Random trajectories (`TrajectoryEngine.cpp:188`, `:222`, `:294`). MDAP also hits it at
el=0, because 3 of its 8 aux directions fall below the horizon. Measured today: a 0.72 gain jump across
the horizon, speaker flip-flopping, and an arbitrary speaker at -90.

**The user asked for research before committing.** Agents researched BS.2127/EAR, MPEG-H, SAF, AllRAD,
Atmos and a plain clamp, and built numerical prototypes. **The options below were presented after that
research**, not before.

| Option | Description | Selected |
|--------|-------------|----------|
| 1: ITU BS.2127 method (recommended) | Virtual -30 degree speakers under the ear-level ring (per `ear/core/point_source.py` `extra_pos_vertical_nominal`) plus a virtual nadir; virtual gains downmixed 1/sqrt(n) to the real speakers, power-renormalized. Holds the horizon pan to -30, then blends to equal gain on all ear-level speakers at -90 | ✓ |
| 2: nadir downmix from 0 degrees | Nadir downmix starting at the horizon | |
| 3: clamp to horizon | Clamp elevation to 0 | |
| 4: custom near-side-weighted spread | Prototype a custom spread first | |

**User's choice:** 1 — the ITU-R BS.2127 (EAR) construction.
**Notes:**
- Example, 4+7+0 at az 30: el -60 gives 0.83 main / 0.228 others; el -90 gives 0.378 on all.
- Precomputed in `LayoutState` at layout-build time; the audio thread stays alloc-free.
- Numeric oracle: PyPI `ear` 2.1.0.
- Applies only to the speaker-layout VBAP family (VBAP/VBIP/MDAP via `computeVBAPGains3D`). Binaural and
  Ambisonics keep the true negative elevation.
- Audible change for OSD sessions with below-horizon sources; accepted as a fix.
-> CONTEXT D-04, D-05.

### Q4 — What happens during playback if a position cannot be placed?

**Context.** Stress test: ~400M points per layout across 8 height layouts x {nadir-only, EAR}, including
10M random, adversarial edge points, the 0.1 degree OSD grid and the ADM-OSC xyz path: 0 fallback hits.
The margin is measured, not proven: observed error ~3.7e-7 against a 1e-6 tolerance, and the a-posteriori
bound reaches 2.6e-6 on 7.1.6/9.1.6. NaN or +-inf az/el **always** reaches the fallback, and nothing
sanitizes it upstream (`RenderEngine::computeObjectGains` passes `degreesToRadians` straight through).

| Option | Description | Selected |
|--------|-------------|----------|
| 1: never crash in playback (recommended) | Sanitize non-finite az/el at engine entry (hold last good position); replace the snap with the best (largest-min-gain) triplet, negatives clamped to 0, renormalized; `jassertfalse` in Debug only | ✓ |
| 2: sanitize input but crash if fallback fires | | |
| 3: crash on garbage input too | | |

**User's choice:** 1 — never crash during playback.
**Notes:**
- The best-triplet replacement is bit-identical wherever a triplet is found today; worst-case deviation
  is ~3e-6 (-110 dB).
- **Widening the tolerance to -1e-5 was rejected:** it flips the triplet choice in 38-111 per 1M
  directions, with gain jumps up to 0.96.
-> CONTEXT D-06, D-07.

---

## Area 2: Two divergent SH evaluators (success criterion 1, VERIFY-01, SpatialCore#11)

**Findings presented:**
- `evalSH` (`src/Core/SpatialMath.cpp:10`, declared `include/SpatialCore/Core/SpatialMath.h:71`) is on the
  shipping path: `AmbisonicsAlgorithm.cpp:30`, `RenderEngine.cpp:450` and `:662`.
- `AmbisonicsCodec::evaluateSH` (`src/IO/AmbisonicsCodec.cpp:13`, declared `AmbisonicsCodec.h:23`) is what
  the 7 codec tests cover.
- The two are bit-identical (10k directions, 0 differences), so the shared bug affects both.
- Convention: ACN, SN3D, no Condon-Shortley phase, radians, az=0 front, +az toward +Y.
- **Confirmed bug**, against an independent scipy reference on 20k directions using the real code: 22 of
  49 channels at orders 4-6 have the wrong constant scale; shape and sign are correct; orders 0-3 are
  exact. Wrong: ACN 16, 24 (x3); 25, 35 (x4); 26, 34 (x3); 27, 33 (x2 sqrt 2); 28, 32 (x sqrt 2); 36, 48
  (x4 sqrt 2); 37, 38, 46, 47 (x4); 39, 45 (x2 sqrt 6); 40, 44 (x sqrt 2); 41, 43 (x2). Correct: ACN
  17-23, 29-31, 42. Affects 4OA-6OA output (`RenderEngine.cpp:450`); the order-3 speaker decode is
  unaffected.

### Q1 — Which evaluator is the source of truth?

| Option | Description | Selected |
|--------|-------------|----------|
| 1: One implementation, two names (recommended) | `evalSH` owns the math and gets the SN3D fix; `AmbisonicsCodec::evaluateSH` becomes a one-line forwarder so both public names survive (DR-3) | ✓ |
| 2: The reverse | The codec owns the math and `evalSH` forwards; the shipping path would then depend on IO | |
| 3: Keep both copies | Leave both implementations and test both against the same reference | |

**User's choice:** 1 — one implementation, `evalSH` fixed, codec forwards to it.
**Notes:** Audible change: 4OA-6OA output becomes correct. -> CONTEXT D-08, D-10.

### Q2 — What to do with the private duplicate `RenderEngine::computeAmbiDecodeForLayout`?

| Option | Description | Selected |
|--------|-------------|----------|
| 1: Delete it (recommended) | Remove `RenderEngine.cpp:649-745` (hard-coded M=16, Tikhonov eps=0.01, never tested). The engine calls `AmbisonicsCodec::getDecodeMatrix` at order 3 and adapts the flat layout into `ambiDecodeMatrix[MAX_SPEAKERS][MAX_SPEAKERS]`, with a test pinning the output | ✓ |
| 2: Keep both plus an agreement test | Retain the private copy and add a test that the two agree | |
| 3: Keep both, no new test | Leave the duplicate as it is | |

**User's choice:** 1 — delete, route through `getDecodeMatrix`.
**Notes:** The user replied "I replied 1", which was recorded as selecting option 1 with an offer to
undo; no undo was requested. Runs off the audio thread. A test pins the new output to the old within
float tolerance. -> CONTEXT D-09.

**Claude's discretion (this area):** where the ACN/SN3D/no-CS statement lands (`evalSH` and
`AmbisonicsCodec.h` docblocks plus docs).

---

## Area 3: Ambisonics round-trip method (success criterion 4, EXTR-03)

**Finding, verified numerically.** A mode-matching encode -> decode -> re-encode round trip (and the
energy-vector direction on that decode) is **blind to per-channel scale bugs**, because
pinv(S*Y) = pinv(Y)*S^-1. It passes on today's buggy code at every order (coefficient error 1.8e-14 with
pinv, 5.06e-4 with Tikhonov, identical buggy versus fixed).

### Q1 — How is criterion 4 satisfied given the round trip cannot see the bug?

| Option | Description | Selected |
|--------|-------------|----------|
| 1: Trio (recommended) | (a) round trip as a smoke test; (b) SN3D addition-theorem invariant, per order l sum over m of Y_lm(dir)^2 = 1 at orders 1-6 (buggy code deviates by up to 19.5); (c) 49 literal scipy reference values at az=64, el=10 (min \|Y\| = 0.072 across all 49 channels), tolerance ~1e-5 | ✓ |
| 2: Round trip only, as written | Literal reading of criterion 4; passes on today's buggy code | |
| 3: Round trip with a projection decoder | A non-inverting (projection) decoder plus an energy-vector direction tolerance. Catches the scale bug (e.g. 11.2 degrees at order 6) but misses sign/swap errors, and needs a threshold between 0.3 and 0.9 degrees | |

**User's choice:** 1 — the trio.
**Notes:** (c) catches the scale bug plus m/-m swap, sign flip,
az reversal, el reversal and Condon-Shortley phase, none of which (a) or (b) can see. CONTEXT records that
the round trip alone is not evidence of correctness. -> CONTEXT D-11, D-12.

**Claude's discretion (this area):** where the scipy generator script lives (checked in, not run in CI),
and exact tolerances.

---

## Area 4: Meaning of "verified against the panning laws" (ROADMAP.md:27 overview line)

**Findings presented.** No success criterion states it. Existing golden vectors are "computed against
pre-move OSD formulas" (`tests/Algorithms/SpatializationAlgorithmTests.cpp:12-21`, `:179-184`); they pin
regressions, not correctness. No tests exist for on-speaker unity, height layouts, sweeps or energy.

### Q1 — What does "verified" mean?

| Option | Description | Selected |
|--------|-------------|----------|
| 1: Behaviour checks + textbook values (recommended) | For all 8 algorithms: on-speaker unity, sum g^2 = 1 across a sweep, L/R mirror symmetry, a 360 degree continuity sweep with no jumps, height-layout coverage including the new below-horizon behaviour. Textbook values from the published formula for VBAP, VBIP, DBAP and MDAP; KNN and ConstantPower (no published law) get property checks only | ✓ |
| 2: Behaviour checks only | Would not have caught the VBIP deviation | |
| 3: Keep the existing OSD golden numbers only | Delete the phrase from the roadmap | |

**User's choice:** 1 — behaviour checks for all 8, textbook values where a published law exists.
**Notes:** Candidate textbook values (the planner must verify
independently before pinning): VBAP Quad az30 -> 0.9659/0.2588, 5.0 az10 -> C 0.8917 / L30 0.4527, 5.0
az50 -> L30 0.9301 / Ls110 0.3673. Standard VBIP Quad az30 -> 0.8881/0.4597, 5.0 az10 -> 0.8144/0.5803, 5.0
az50 -> 0.8467/0.5321, invariant sum g^2 * l points at the source. DBAP at the effective R=12.04 dB, Quad
az30 (speaker order 45, 135, -135, -45) -> 0.9984, 0.0270, 0.0173, 0.0459. MDAP: assert spread=0 equals
VBAP. -> CONTEXT D-13.

### Exploratory question (user) — what is the difference between VBIP and VBAP?

Not a decision point; the user asked to understand the gap before ruling on Q2.
**Answer given:** same speakers, exponent 1/2. VBAP aims the velocity vector rV (the low-frequency
localization cue); VBIP aims the energy vector rE (the high-frequency cue). VBIP gains are the VBAP gains
raised to 1/2 and renormalized.
**Result:** the answer showed VBAP and VBIP are one algorithm family differing by a focus exponent, which
produced **GitHub AndrewRahman/SpatialCore#20** (see Deferred Ideas). It did not change Q2 or Q3.

### Q2 — Should VBIP stay as coded, or become textbook VBIP?

Current code squares the normalized VBAP gains (`VBIPAlgorithm.cpp:30`; header comment "squared gains for
tighter focus"), giving 0.9974/0.0716 at Quad az30, with the intensity vector aimed at 44.7 degrees instead
of 30.

| Option | Description | Selected |
|--------|-------------|----------|
| 1: Keep the sound, rename it honestly (was recommended) | Document the current output as "focused VBAP (squared gains)" and defer textbook VBIP as a 9th algorithm | |
| 2: Fix it to textbook VBIP | Gains g_i = sqrt(G_i / sum G_i) over the same pair/triplet | ✓ |
| 3: Fix VBIP and add the squared version | Add the squared version as a new "Focused VBAP", a 9th algorithm (out of scope) | |

**User's choice:** 2 — fix to textbook VBIP. The user overrode the recommendation. **User's words: "keep
VBIP as the original intended VBIP or we're lying."**
**Notes:** Audible change: OSD sessions using VBIP (a user-selectable menu entry, index 6) become wider.
Cross-repo follow-up: OSD glossary lines 127-128 ("squares the gains, producing an even tighter spatial
image") must be corrected in the OSD repo. Existing VBIP golden vectors are regenerated.
-> CONTEXT D-14.

### Q3 — What is the source, and is VBIP single-band or dual-band?

The primary source was verified: Pernaux, Boussard & Jot, DAFx-98,
https://www.dafx.de/paper-archive/1998/PER15.PS.pdf, Sec. 2.2.2: g_i = sqrt(G_i / sum G_i), with
G = L^-1 p over the same pair/triplet, sum g^2 = 1. The paper is dual-band: VBAP below 700 Hz, VBIP above
700 Hz.

| Option | Description | Selected |
|--------|-------------|----------|
| 1: Single-band VBIP (recommended) | Textbook VBIP gains at all frequencies; docs note the paper's 700 Hz pairing | ✓ |
| 2: The paper's full dual-band scheme | VBAP below 700 Hz and VBIP above 700 Hz, per object; needs crossover filters in `RenderEngine`, and the frozen `computeGains` interface cannot express it | |

**User's choice:** 1 — single-band. **User's words: "We will do #1 for now because it's honest."**
**Notes:** Dual-band is deferred, not rejected, and is tracked in #20. The VBAP+VBIP unification with a
focus exponent was not an option here; it arose from the exploratory question above and is also tracked
in #20. -> CONTEXT D-15.

### Decided without objection

- **DBAP documented as-is:** R about 12.04 dB (a=2, w=1/d^2 used as amplitude), no spatial blur r_s, d^2
  clamped >= 0.001, no hull projection. Tests assert the Lossius et al. ICMC 2009 formula at R=12.04 dB.
  -> CONTEXT D-16.
- **MDAP citation corrected** to Pulkki, "Uniform spreading of amplitude panned virtual sources", IEEE
  WASPAA 1999. The code says 2000 at `MDAPAlgorithm.cpp:8` and `SpatializationAlgorithmTests.cpp:271`.
  -> CONTEXT D-17.

Both items were flagged in the Area 1 checkpoint as "raise in Area 4" (VBIP squaring, MDAP citation) and
are resolved by D-14 and D-17.

---

## Claude's Discretion

- **Tie `kLayoutExpectations` to `NUM_LAYOUT_DEFS`** (`tests/IO/SpeakerLayoutTests.cpp:87`, a hand-written
  15-row golden table). The user said fold it in; do not ask. Success criterion 3 is otherwise already met
  by that table plus five all-layout loops.
- **`nearestSpeaker3DFallback`** (`include/SpatialCore/Core/SpatialMath.h:40-66`, inline in the public
  header). Removing it after its 4 call sites are gone is a major-version bump. Default: keep with a
  deprecation comment, remove at the next major. No consumer calls SpatialCore's copy (OSD main has its own
  local copy; OSD 30391cd defines its own at `PluginProcessor.cpp:2115`).
- **Virtual-speaker storage.** `MAX_SPEAKERS = 16`; EAR on 9.1.6 needs 15 real + 9 virtual + 1 nadir = 25.
  Virtual speakers must not consume real slots; size triplet/scratch/downmix storage separately, preallocated
  in `LayoutState`.
- **EAR performance.** Extras inflate 9.1.6 from 448 to up to 2119 triplets (~2.6 us per call); filtering to
  convex-hull facets gives ~86 triplets and ~0.12 us with no coverage loss below the horizon. Leaving the
  existing upper-hemisphere triplets untouched avoids changing behaviour above the horizon.
- **No EAR virtual zenith.** Testing showed no upper-hemisphere coverage gaps without it.
- **Where "hold last good position" state lives:** per-object, preallocated.
- **Where the ACN/SN3D/no-CS statement lands,** where the scipy generator script lives, and exact
  tolerances.
- **DirectBinaural:** property checks; Woodworth ITD/ILD magnitude checks optional.

## Deferred Ideas

- **SpatialCore#20** (created during this discussion; cross-linked to #10, spread API, SPRD-01, v2):
  per-source focus/spread control; unify VBAP+VBIP into one VBAP with a focus exponent (p=0.5 VBIP, p=1
  VBAP, p=2 old behaviour; count 8 to 7; OSD session migration); Pernaux dual-band VBAP < 700 Hz / VBIP >
  700 Hz.
- Pre-existing min-gain-sum tie-break discontinuity between overlapping triangulations of coplanar quads
  (+-45/+-135 height ring; ear/-30 trapezoids under EAR): 4-13 per 1M directions switch triplet, gain
  jumps up to 0.70. Separate ticket, not yet filed.
- DBAP spatial blur, a user rolloff parameter, and convex-hull projection for sources outside the speaker
  hull (relates to #10).
- VBAP 2D degenerate guard for speaker gaps >= 180 degrees (no shipped layout has one).
- MDAP on 2D layouts: vertical aux points collapse onto the source azimuth (main direction effectively
  weighted ~3/9). Noted, not changed.
- Speaker-layout Ambisonics decode stays fixed at order 3; higher order would be a new capability.
- OSD repo follow-ups: correct glossary lines 127-128; plan session migration for the audible changes.
