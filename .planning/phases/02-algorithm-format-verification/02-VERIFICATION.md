---
phase: 02-algorithm-format-verification
verified: 2026-10-01T07:52:54Z
status: human_needed
score: 57/60 must-haves verified
covered_files:
  - .claude/skills/spatial-audio-dsp/SKILL.md
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
covered_digest: "v2:sha256:89613a2bfb65c9c45248db0f9bfdbd530410ab77b6c3e6dd3addc305fd6bec82"
behavior_unverified: 1
overrides_applied: 0
behavior_unverified_items:
  - truth: "RenderEngine::activateLayout aborts in every build type when a height layout yields no regular triplets or a flat layout yields some; only a SpatialCore developer editing layoutDefs or the builder can reach it, and the abort is on the message/prepare thread, never on a path reachable from renderBlock (02-01, D-02a; verification: backstop)"
    test: "Temporarily edit a layoutDefs height entry so it builds no regular triplets (or lower a flat speaker to 0.8 degrees with the threshold reverted), build Release, call setOutputFormat for that format"
    expected: "Process aborts inside activateLayout on the calling (message/prepare) thread; renderBlock never reaches it"
    why_human: "Tagged verification: backstop (non-inferable). The abort kills the test process, so no Catch2 test exercises it. The only evidence is placement: one std::abort at RenderEngine.cpp:729, and none in renderBlock. Presence and wiring do not qualify as evidence for a backstop truth."
coincidental_reliance_items:
  - truth: "Finite inputs are untouched by every guard and every std::isfinite check lives in a .cpp, so OpenSpatialDelay's -ffast-math build cannot fold it away (02-04)"
    reason: undeclared-precondition
    harden: "The guards hold only because the SpatialCore target itself is compiled without -ffast-math / -ffinite-math-only. Today that is true only because OSD applies the flag PRIVATE to its own targets. Turn it into a compile-time #error on __FAST_MATH__ / __FINITE_MATH_ONLY__ in the guarded TUs (review WR-04)."
human_verification:
  - test: "DR-3 cross-repo build (harvested from 02-07 Task 3 <human-check>). Read the 'DR-3' and 'Cross-repo follow-ups' sections of 02-07-SUMMARY.md."
    expected: "You accept that OSD 30391cd compiled and linked against this branch (verifier confirmed /tmp/osd-dr3-check/build.log ends 'BUILD EXIT 0' with 0 'error:' lines, SpatialCore symlinked to this worktree, and no src/include change since). You accept deferring the five ADMOSCReceiver.h -Wunused-parameter warnings. The four follow-ups are the ones to carry into the OSD repo."
    why_human: "Cross-repo decision, listed as manual-only in 02-VALIDATION.md; this phase must not make OSD-repo decisions."
  - test: "Below-horizon 0 to -30 degree band versus EAR. Verifier probe (built against this branch, then deleted): 7.1.4, az 60. SpatialCore gives L/Lss = 0.7210/0.6929 at el -5, 0.7472/0.6646 at el -15, 0.6768/0.7361 at el -25. PyPI ear 2.1.0 gives 0.7071/0.7071 at all three. Max deviation from the horizon pan over az and el in [-29,-1] is 0.057."
    expected: "Decide one of two outcomes. (a) Accept the deviation as the #22 triangulation defect and fix the docs: integration guide, README and skill should stop presenting the 0 to -30 band as exact BS.2127 behaviour, and should widen the 'above the horizon' tie scope (review WR-01). (b) Treat it as a gap for a gap-closure plan."
    why_human: "The [ear] oracle test covers only the nadir-cap region, so no test sees this band. The 02-07 prohibition 'MUST NOT describe an algorithm differently from what the code computes' is partly breached: the guide calls the band EAR, and CONTEXT D-04 promised 'the horizon pan holds down to -30 degrees'. The deviation is small (about 0.5 dB, with the lean side set by float rounding), so whether it matters is a product call."
  - test: "Residual silent degradation on hand-built LayoutContexts, versus criterion 2's 'fails loudly'. Review the two misuse paths. (1) A height layout with EMPTY triplets: Debug jassert, but in Release the code falls through to computeVBAPGains2D, a pairwise pan over all speakers, height speakers included. (2) A context built from buildVBAPTripletsForLayout alone, without appendLowerHemisphereTriplets: every below-horizon source silently takes the D-06b largest-min-gain fallback in Release and fires a Debug jassert on the audio thread (review WR-03)."
    expected: "Confirm that D-01/D-02 ('harden upstream') covers both. Every engine-built context is protected: the activateLayout abort plus the [ear][coverage] test show 0 uncovered finite directions on all 8 height layouts. The builder docblock fix from WR-03 is a follow-up, not a blocker."
    why_human: "The nearest-speaker fallback criterion 2 names is gone. But on a public, consumer-reachable API path the Release behaviour is still a silent degradation. That path is outside the SC-13 facade, and the user accepted it in D-01, so this needs a human decision, not a verifier verdict."
  - test: "Process gates recorded only in SUMMARY narrative. (a) 02-02 Task 1: the package-legitimacy blocking-human gate was answered 'approved-recreate', with the user delegating the decision to the orchestrator. (b) 02-03 Task 1: the exact title and body of AndrewRahman/SpatialCore#22 were approved before filing."
    expected: "Confirm that the delegation discharges the blocking-human gate, and that you approved the #22 text. The verifier confirmed #22 exists with the F5 measurements and that 02-03-ISSUE-BODY.md is on disk."
    why_human: "Past human approvals cannot be verified from the codebase. 02-02-SUMMARY itself marks this human_judgment: true."
  - test: "Judgment-tier prohibitions (17 across 7 plans). The verifier recorded a NON-AUTHORITATIVE judgment for each; see the 'Prohibitions' table. 16 hold on the evidence. One, 02-07 'docs must not describe code differently', partly fails (see the EAR-band item above)."
    expected: "Review the table and accept or reject each judgment."
    why_human: "unverified-prohibition: human review recommended. Judgment-tier items are never silently absorbed into a pass."
---

# Phase 2: Algorithm & Format Verification Verification Report

**Phase Goal:** Every algorithm and every advertised format is confirmed correct by a test, and the Ambisonics convention is written down.
**Verified:** 2026-10-01T07:52:54Z
**Status:** human_needed
**Re-verification:** No (initial verification)

## Summary

All four ROADMAP success criteria hold in the codebase, and each is backed by a test I ran. Status is `human_needed`, not `passed`, for five reasons:

- a harvested end-of-phase human check (DR-3)
- one backstop truth that no test can exercise
- a new finding: the 0 to -30 degree band does not match EAR, and the docs say it does
- a design-acceptance question on hand-built contexts
- the flagged judgment-tier prohibitions

There are no blockers.

**Test run (my own, not the SUMMARY's):** `cmake --build build --target SpatialCoreTests` exit 0. `./build/tests/SpatialCoreTests`: **190 test cases, 189 passed, 1 failed; 306,087 assertions, 1 failed**. The one failure is `HUTUBS PP2 — golden HRIR checksum at az=90deg (own control)` (`tests/Binaural/HutubsPP2Tests.cpp:47`). It is pre-existing: Phase 1 `deferred-items.md` item 01-01. No Phase 2 commit touches `tests/Binaural/HutubsPP2Tests.cpp`, `src/Binaural/` or `HRTF/` (`git log b338fb0^..HEAD` on those paths is empty). It is Phase 3 scope.

## Goal Achievement

### ROADMAP Success Criteria

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | AmbisonicsCodec's channel order and normalisation is confirmed ACN/SN3D or FuMa, stated in code and docs (SpatialCore#11) | ✓ VERIFIED | **Code:** convention docblock on `evalSH` (`include/SpatialCore/Core/SpatialMath.h`), on the `AmbisonicsCodec` class (`AmbisonicsCodec.h`), and in the `SpatialMath.cpp` header. **Docs:** README rows 17 and 28; integration-guide "Ambisonics channel convention" (lines 185-195); skill §1.5 and §7.1-7.4. **Confirmation by test:** the 49 scipy literals at az 64 / el 10 within 1e-5, plus the SN3D addition theorem at orders 1-6, plus negative-elevation parity (`[sn3d]`, all pass). Those three would catch ACN vs FuMa ordering, N3D vs SN3D, and a Condon-Shortley sign. **Orientation:** "+az = left" agrees with the layout table (`SpeakerLayout.cpp`: L = +30, ch 0). |
| 2 | The 3D triplet fallback in VBAP/VBIP/MDAP either no longer exists or fails loudly instead of silently degrading to nearest-speaker | ✓ VERIFIED | **Both nearest-speaker paths are gone.** I compared against `d43cb15`: the algorithm-level `else if (layoutHasHeight) nearestSpeaker3DFallback` branches in VBAP, VBIP and MDAP (x2) are deleted, and so is the in-function "Fallback: nearest speaker" loop in `computeVBAPGains3D`. `grep -rn nearestSpeaker3DFallback src/` is empty. **Engine path:** `activateLayout` runs `jassertfalse; std::abort();` when triplets are absent (`RenderEngine.cpp:726-730`), and renderBlock contains no abort. **No-enclosing-triplet case:** Debug `jassertfalse` plus a largest-min-gain triplet. `[ear][coverage]` shows 0 uncovered finite directions on all 8 height layouts (about 56k directions each), so the path is not reached for finite input. The only nearest-speaker code left is in the 2D pair path (`SpatialMath.cpp:190`), which is not the 3D triplet fallback. See the human item on hand-built contexts. |
| 3 | All 23 OutputFormat entries resolve to correct info; all 15 layouts return populated channel indices and LFE placement | ✓ VERIFIED | **Formats:** `[io][golden]` 23-row format table, with a REQUIRE that the row count equals `NUM_OUTPUT_FORMATS`. `[format-resolve]` drives all 23 formats through `RenderEngine::setOutputFormat` and checks them against the registry for channels, LFE, height, distinct channel indices that skip LFE, and the triplet invariant. It counts 23 formats and 15 speaker formats. **Layouts:** `kLayoutExpectations` has 15 rows, `static_assert`-tied to `NUM_LAYOUT_DEFS` (`SpeakerLayoutTests.cpp:153`). The `[io][layout]` loops check speaker count, total channels, LFE index, and channel indices in range, distinct and skipping LFE. All pass. |
| 4 | Ambisonics encode/decode round-trips a source position within tolerance at every order up to 6 | ✓ VERIFIED | `[roundtrip]`: orders 1-6, 50 seeded directions, `AmbisonicsCodec::encode`, then a test-local dense decode on 200 Fibonacci points, then re-encode, with tolerance 1e-5. It passes. The test labels itself a smoke test, per D-12. The user chose the D-11 trio for this criterion (DISCUSSION-LOG Area 3, Q1 option 1): the round trip, the addition theorem, and the 49 literals. That choice reflects RESEARCH F6: the shipped `getDecodeMatrix` is capped at 16 speakers and cannot round-trip above order 1. Note: the decode half of this round trip is not SpatialCore's shipped decoder. |

### Plan Must-Have Truths

Truths that restate a ROADMAP criterion are folded into it: 02-01 #6 into SC2; 02-01 #10 and 02-06 #9 into SC3; 02-06 #5 into SC4; 02-06 #6 and 02-07 #1 into SC1. That leaves 56 plan truths plus 4 criteria, 60 in total.

**02-01: EAR lower hemisphere and fallback removal**

| Truth | Status | Evidence |
|---|---|---|
| Nadir on 7.1.4 via engine + VBAPAlgorithm gives 1/sqrt(7) on ear level, 0 on height | ✓ VERIFIED | `[tracer]` passes |
| Under an ear-level speaker gives unity | ✓ VERIFIED | `[tracer]` meridian case; `[ear]` meridian on all height layouts |
| Above-horizon output bit-identical to d43cb15 | ✓ VERIFIED | `[vbap3d-identity]` passes (262,104 probes per SUMMARY; the test passed in my run) |
| Lower-hemisphere triplets built by appendLowerHemisphereTriplets, outside the builder, flagged | ✓ VERIFIED | `SpeakerLayout.cpp`; called at `RenderEngine.cpp:735` after the guard; builder test asserts `! lowerHemisphere` |
| One 0.0175 rad threshold | ✓ VERIFIED | `kHeightThresholdRad` (`SpeakerLayout.cpp:95`); the builder calls `layoutHasHeight`; `[d03]` passes |
| D-02a abort, every build type, not reachable from renderBlock | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED (backstop) | Placement verified (`:729`, none in renderBlock). Tagged `verification: backstop`, so it goes to human review |
| setOutputFormat void; OSD public surface compiles | ✓ VERIFIED | `[consumer-surface]` passes; DR-3 OSD build exit 0 |
| Tolerance stays -1e-6f | ✓ VERIFIED | 3 occurrences of `-1e-6f`, 0 of `-1e-5f` |
| Binaural/Ambisonics algorithms never call computeVBAPGains | ✓ VERIFIED | grep over both sources is empty |

**02-02: Independent reference oracles**

| Truth | Status | Evidence |
|---|---|---|
| Every pinned reference comes from an independent checked-in generator | ✓ VERIFIED | I regenerated all three headers with `.context/venv` and each is **byte-identical** to the checked-in file |
| Headers compile standalone and regenerate byte-identically | ✓ VERIFIED | Regeneration above; headers are included by 4 test TUs that build |
| gen_sh / gen_ear / gen_panning self-checks | ✓ VERIFIED | The generators ran to completion (they refuse to emit on failure) |
| MDAP labelled a port, not an oracle | ✓ VERIFIED | `PanningLawTests.cpp` test title "cross-check, not an oracle" |
| Nothing in tests/reference runs in CI | ✓ VERIFIED | No workflow references tests/reference |
| numpy/scipy/ear used only after human verification | ? UNCERTAIN | SUMMARY records the gate answered by delegation to the orchestrator. Human item |

**02-03: Tie-break issue**

| Truth | Status | Evidence |
|---|---|---|
| Defect filed with F5 measurements, number recorded | ✓ VERIFIED | `gh issue view 22`: the body contains 1.325751162 and the sweep numbers; cited in the skill, the guide and PanningLawTests |
| Issue states Phase 2 does not fix it, lists asserted regions, records tolerance rejection | ✓ VERIFIED | Issue body / `02-03-ISSUE-BODY.md` |
| Nothing filed until a human approved the exact text | ? UNCERTAIN | SUMMARY narrative only. Human item |

**02-04: Robustness**

| Truth | Status | Evidence |
|---|---|---|
| No position value hangs or yields non-finite gains (8 algorithms, watchdog) | ✓ VERIFIED | `[robust]` passes |
| 2D/3D VBAP: non-finite gives silence; bounded `std::remainder` | ✓ VERIFIED | `SpatialMath.cpp:136-151, 252-259`; `[robust]` |
| KNN and DirectBinaural silent for non-finite | ✓ VERIFIED | `isfinite` guards in both `.cpp` files; `[robust]` / `[sanitize]` |
| No-triplet case: largest-min-gain; empty list silent; Debug assert | ✓ VERIFIED | `SpatialMath.cpp:316-336`; D-06b test passes |
| Engine hold-last-good per field; first-ever non-finite gives 0, 0, 0.5 | ✓ VERIFIED | `sanitizeSources` (`RenderEngine.cpp:90`); 4 `[sanitize]` behavioural tests pass |
| NaN distance never reaches the NFC-HOA state | ✓ VERIFIED | `[sanitize]` NFC-HOA test passes |
| Finite inputs untouched; isfinite only in .cpp | ✓ VERIFIED (coincidental-reliance) | No new header `isfinite` (the two in `SpatialMath.h` are the pre-existing softClip/outputLimiter). Depends on SpatialCore itself not being built with fast-math (WR-04) |
| Below-horizon VBAP matches ear 2.1.0 within 1e-5 in the nadir cap | ✓ VERIFIED (as scoped) | `[ear]` oracle passes. **The 0 to -30 band is not covered, and it deviates (see Gaps Summary)** |
| Horizon continuity at most 0.01 | ✓ VERIFIED | `[ear]` horizon test passes |
| Every finite direction covered, unit power, no elevated leak below the horizon | ✓ VERIFIED | `[ear][coverage]` passes |
| Tolerance stays -1e-6f | ✓ VERIFIED | As above |

**02-05: Panning laws**

| Truth | Status | Evidence |
|---|---|---|
| VBIP textbook (0.8880738 / 0.4597008, rE aimed) | ✓ VERIFIED | `VBIPAlgorithm.cpp` sqrt then renormalise; `kVbip_Quad_az30`; `[vbip]` passes |
| VBIP docs say single-band, #20 | ✓ VERIFIED | `VBIPAlgorithm.cpp` header comment; README and skill |
| All 8 algorithms checked, enumerated from AllAlgorithmTypes | ✓ VERIFIED | `instantiateAll (AllAlgorithmTypes{})`; `[panning-law]` passes |
| On-speaker behaviour per algorithm | ✓ VERIFIED | Law table (`PanningLawTests.cpp:~95-110`); passes |
| Continuity only where continuous, exclusions cite #22 | ✓ VERIFIED | `ContinuityScope` and comments citing #22 |
| Textbook values pinned from PanningReference.h | ✓ VERIFIED | include and tests pass |
| Height coverage, unit power, no elevated gain below the horizon | ✓ VERIFIED | `[panning-law]` height coverage passes |
| DBAP documented as implemented (12.04 dB) | ✓ VERIFIED | `DBAPAlgorithm.h:15`, README:26, skill:109 |
| MDAP cited WASPAA 1999 | ✓ VERIFIED | `.h`, `.cpp`, README, skill; `git grep 'Pulkki 2000'` outside .planning is empty |
| DirectBinaural property checks only | ✓ VERIFIED | "DirectBinaural: property checks at the horizon" passes |

**02-06: SN3D, one evaluator, one decoder**

| Truth | Status | Evidence |
|---|---|---|
| evalSH is the single implementation; evaluateSH forwards | ✓ VERIFIED | `AmbisonicsCodec.cpp:17-20` `return spatialcore::evalSH`; forwarder test passes |
| 49 scipy literals within 1e-5 | ✓ VERIFIED | `[sn3d]` passes; the reference regenerates byte-identically |
| SN3D addition theorem at orders 1-6 (22 constants fixed) | ✓ VERIFIED | `[sn3d]` passes; spot check: ACN 16 = sqrt(35)/8 matches SN3D |
| Negative-elevation parity | ✓ VERIFIED | `[sn3d]` passes |
| No private decoder; decode within 1e-6 of pre-change on 15 layouts | ✓ VERIFIED | `getDecodeMatrix (3, ...)` at `RenderEngine.cpp:713`; `computeAmbiDecodeForLayout` is absent; `[ambi-pin]` passes |
| getDecodeMatrix writes nothing for >16 or 0 speakers | ✓ VERIFIED | Guard at `AmbisonicsCodec.cpp:52`; `[decode-guard]` passes |
| Decode rows follow speaker order, SH channels ACN order | ✓ VERIFIED | `[ambi-pin]` plus the literal test; docblock |

**02-07: Docs, DR-3, phase gate**

| Truth | Status | Evidence |
|---|---|---|
| Docs describe VBIP as textbook, wider, single-band | ✓ VERIFIED | No "squared gains" / "tighter" text in README, docs or skill |
| Docs describe below-horizon as EAR, the no-triplet case, non-finite handling; no nearest-speaker | ✓ VERIFIED (as worded) | Guide lines 203-225. **Accuracy caveat:** the 0 to -30 band is not EAR-exact (see Gaps Summary) |
| nearestSpeaker3DFallback keeps body and signature, comment-only deprecation | ✓ VERIFIED | `SpatialMath.h:37-71`, no attribute |
| Skill states min-sum tie-break, cites #22 | ✓ VERIFIED | `SKILL.md:45` |
| DBAP 12.04 / MDAP WASPAA 1999 across docs | ✓ VERIFIED | As above |
| OSD 30391cd compiles and links against this branch | ✓ VERIFIED | `/tmp/osd-dr3-check/build.log` ends "BUILD EXIT 0" with 0 `error:` lines; `SpatialCore` symlinked to this worktree; no src/include change since `2b7653b`; the OSD repo's `git status --porcelain` is empty |
| Full suite passes except HUTUBS PP2; every Phase 2 tag runs a test | ✓ VERIFIED | My run: 190/189, only HUTUBS fails; all 15 tags list at least 1 case |
| Cross-repo follow-ups recorded, not performed | ✓ VERIFIED | 02-07-SUMMARY "Cross-repo follow-ups"; OSD repo untouched |

**Score:** 57/60 truths verified. One is present but behaviour-unverified (the backstop abort), and two are uncertain (process approvals). All three are routed to human verification.

### Prohibitions (judgment-tier, non-authoritative LLM judgments; human review recommended)

| Plan | Prohibition (short) | Verifier judgment |
|---|---|---|
| 02-01 | No sound change beyond the 4 approved changes; above-horizon VBAP bit-identical | Holds: `[vbap3d-identity]` |
| 02-01 | No alloc/lock/log on the audio path | Holds: lower-hemisphere data built in `activateLayout`; 3D selection uses stack only |
| 02-01 | No abort on a user-reachable path | Holds: the abort is only in `activateLayout` (closed enum, private) |
| 02-01 | No change to the frozen interface or LayoutContext; no public symbol removed | Holds as worded: `[consumer-surface]` and the OSD build. Caveat WR-02: `vbapTriplets` keeps its name but its semantics changed |
| 02-02 | No reference derived from the code under test | Holds: byte-identical regeneration from scipy / ear / formulas |
| 02-03 | No outward action without approval; only AndrewRahman/SpatialCore | Unverifiable from code: #22 is in the right repo; approval needs human confirmation |
| 02-04 | No hang or non-finite output on the audio thread | Holds: `[robust]` and `[sanitize]`. Conditional on no fast-math in SpatialCore (WR-04) |
| 02-04 | No alloc on the audio path (sanitiser) | Holds: `static_assert` that the copy is trivially copyable; preallocated member |
| 02-04 | No change for finite positions | Holds: guards act only on non-finite values; 2D `remainder` is exact |
| 02-05 | No textbook value derived from the code | Holds |
| 02-05 | No VBIP wording that differs from the code | Holds |
| 02-05 | No sound change beyond VBIP | Holds |
| 02-06 | Convention unchanged; order-3 decode within 1e-6 | Holds: `[ambi-pin]` max diff 0 per SUMMARY, within 1e-6 in my run |
| 02-06 | Round trip not presented as correctness evidence | Holds: test title and comment say "smoke test only" |
| 02-06 | SH references only from ShReference.h | Holds |
| 02-07 | Docs do not describe code differently from what it computes | **Partly fails.** The 0 to -30 band is described as BS.2127 (EAR) but deviates from EAR by up to 0.057. The tie-jump scope says "above the horizon" only (WR-01). Minor gaps: IN-01 (silence list incomplete) and IN-04 ("per object" really means per slot) |
| 02-07 | OSD repo not modified; no plugin install | Holds: `git status --porcelain` empty; only non-installing targets were built |

### Required Artifacts

| Artifact | Status | Details |
|---|---|---|
| `tests/Core/VBAPTripletSelectionTests.cpp` | ✓ VERIFIED | 1026 lines; registered in tests/CMakeLists.txt; includes `EarReference.h` |
| `tests/Algorithms/PanningLawTests.cpp` | ✓ VERIFIED | 973 lines; registered; includes `PanningReference.h` |
| `src/IO/SpeakerLayout.cpp` | ✓ VERIFIED | `appendLowerHemisphereTriplets`; shared threshold |
| `include/SpatialCore/IO/SpeakerLayout.h` | ✓ VERIFIED | `lowerHemisphere`, nadir fields with defaults |
| `src/Core/SpatialMath.cpp` | ✓ VERIFIED | Two-pass selection, `nadirMask` downmix, `std::remainder`, corrected SN3D constants |
| `src/Engine/RenderEngine.cpp` | ✓ VERIFIED | `sanitizeSources`, `getDecodeMatrix (3`, the abort guard, the append call |
| `tests/reference/*` (3 generators, 3 headers, README) | ✓ VERIFIED | Regenerate byte-identically |
| `tests/IO/AmbisonicsCodecTests.cpp` | ✓ VERIFIED | `[sn3d]`, `[roundtrip]`, `[decode-guard]` |
| `tests/Engine/RenderEngineTests.cpp` | ✓ VERIFIED | `referenceAmbiDecode`, `[ambi-pin]`, `[format-resolve]` |
| `include/SpatialCore/Algorithms/DBAPAlgorithm.h` | ✓ VERIFIED | Contains 12.04 |
| `docs/integration-guide.md`, `README.md`, `SKILL.md` | ✓ VERIFIED (with WR-01 accuracy caveat) | Convention, VBIP, EAR, DBAP, MDAP text present |
| `02-03-ISSUE-BODY.md` | ✓ VERIFIED | Matches issue #22 |

### Key Link Verification

| From | To | Via | Status |
|---|---|---|---|
| RenderEngine.cpp | SpeakerLayout.cpp | `activateLayout` calls `appendLowerHemisphereTriplets` after the guard, before the swap (`:735`) | ✓ WIRED |
| SpatialMath.cpp | SpeakerLayout.h | `computeVBAPGains3D` reads `lowerHemisphere` / `nadirVertex` / `nadirMask` / `nadirGain` | ✓ WIRED |
| RenderEngine.cpp | RenderEngine.h | `renderBlock` calls `sanitizeSources (sources)` and passes `src` to all 5 paths and to `computeObjectGains` | ✓ WIRED |
| AmbisonicsCodec.cpp | SpatialMath.cpp | `return spatialcore::evalSH` | ✓ WIRED |
| RenderEngine.cpp | AmbisonicsCodec.cpp | `getDecodeMatrix (3, ...)` into `ambiDecodeMatrix` | ✓ WIRED |
| Tests | reference headers | `#include "../reference/{Sh,Ear,Panning}Reference.h"` | ✓ WIRED |
| PanningLawTests.cpp | AllAlgorithms.h | `instantiateAll (AllAlgorithmTypes{})` | ✓ WIRED |
| tests/CMakeLists.txt | the 2 new test files | Source list | ✓ WIRED (both compiled into SpatialCoreTests) |
| SKILL.md | issue tracker | `AndrewRahman/SpatialCore#22`, `#20` | ✓ WIRED (#22 exists) |

### Behavioural Spot-Checks

| Behaviour | Command | Result | Status |
|---|---|---|---|
| Suite builds and passes except the known failure | `cmake --build build --target SpatialCoreTests && ./build/tests/SpatialCoreTests` | 190 cases, 189 pass; only HUTUBS PP2 fails | ✓ PASS |
| Reference headers are generated, not hand-typed | `.context/venv/bin/python tests/reference/gen_*_reference.py \| diff - tests/reference/*Reference.h` | 3/3 byte-identical | ✓ PASS |
| Every Phase 2 tag has tests | `SpatialCoreTests "[tag]" --list-tests` for 16 tags | All non-empty | ✓ PASS |
| EAR fidelity in the 0 to -30 band (not covered by any test) | Throwaway /tmp probe linked against `build/libSpatialCore.a` vs `ear.core.point_source` 4+7+0 at az 60 | SpatialCore 0.7472/0.6646 at el -15 vs ear 0.7071/0.7071; max deviation from the horizon pan 0.057 | ✗ deviates (routed to human; not a criterion failure) |
| Issue #22 exists | `gh issue view 22 -R AndrewRahman/SpatialCore` (read-only) | Open, body carries the F5 measurements | ✓ PASS |

### Probe Execution

No `scripts/*/tests/probe-*.sh` exist, and no plan declares one. Step 7c: N/A.

### Requirements Coverage

| Requirement | Source Plans | Description | Status | Evidence |
|---|---|---|---|---|
| EXTR-01 | 02-01, 02-02, 02-03, 02-04, 02-05, 02-07 | Every public algorithm computes correct gains; the 3D triplet fallback is eliminated or fails loudly | ✓ SATISFIED | All existing algorithm tests still pass. The `[panning-law]` suite covers all 8 algorithms. Both nearest-speaker fallbacks are deleted (SC2). **Tracking:** the REQUIREMENTS.md checkbox is still `[ ]` and the traceability row says "Largely verified by tests". The evidence supports marking it complete; the orchestrator should update the row (I did not edit it). |
| EXTR-03 | 02-02, 02-06, 02-07 | Layouts, format registry and Ambisonics codec return real data; 23 formats, 15 layouts, encode/decode to order 6 | ✓ SATISFIED | SC3 and SC4 evidence. Encoding to order 6 is proven against scipy. The order-6 decode in the round trip is test-local, as the user chose in D-11; the shipped `getDecodeMatrix` is order-3 / 16-speaker capped (RESEARCH F6). **Tracking:** same quirk as EXTR-01; the row needs updating. |
| VERIFY-01 | 02-02, 02-06, 02-07 | Ambisonics channel order and normalisation convention stated (SpatialCore#11) | ✓ SATISFIED | SC1. Already marked `[x]` / Complete in REQUIREMENTS.md. Note: #11 is closed only by commit-message reference; no `gh` action was taken on the GitHub issue. |

No orphaned requirements: REQUIREMENTS.md maps only EXTR-01, EXTR-03 and VERIFY-01 to Phase 2, and all three are claimed by plans.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|---|---|---|---|---|
| (35 files changed in Phase 2) | — | TBD / FIXME / XXX / TODO / placeholder | — | None found, in added lines or in whole files |
| `src/Core/SpatialMath.cpp` | 190-202 | 2D "Fallback: nearest speaker" | ℹ️ Info | Pre-existing 2D pair path, effectively unreachable for in-range azimuths. Not the 3D triplet fallback SC2 names |
| `src/Algorithms/{VBAP,VBIP,MDAP}Algorithm.cpp` | jassert sites | Release fall-through to `computeVBAPGains2D` on a height layout with empty triplets | ⚠️ Warning | Only via a hand-built LayoutContext (outside SC-13). This is the pan onto height speakers that the v1.0 comment warned about. Accepted under D-01; see human item |

### Code Review Cross-Check (02-REVIEW.md, all open)

- **WR-01** (guide scopes coplanar-tie jumps to "above the horizon"): **confirmed, and it is wider than reported.** Beyond the ties, the whole 0 to -30 band differs from EAR's quad panning (my probe). This affects doc accuracy for the panning section, not SC1: the Ambisonics convention text is accurate. Routed to human.
- **WR-02** (mixed semantics in `vbapTriplets`): real. No SC is affected today; OSD's legacy walker is dead code that is scheduled for deletion.
- **WR-03** (regular-only builder gives incomplete coverage): real. It does not undermine SC2 on the engine path. It is a residual silent-degradation path for hand-built contexts. Routed to human.
- **WR-04** (fast-math invariant enforced only by comments): real. Recorded as a coincidental-reliance item on the 02-04 truth.
- **IN-01 to IN-04**: doc precision and API ergonomics. None undermines a success criterion.

### Human Verification Required

#### 1. DR-3 OSD build and cross-repo follow-ups (harvested from 02-07 Task 3)
**Test:** Read the DR-3 and "Cross-repo follow-ups" sections of `02-07-SUMMARY.md`.
**Expected:**
- You accept the build result. I confirmed it: `BUILD EXIT 0`, 0 errors, the build ran against this worktree, and no source has changed since.
- You accept deferring the 5 `ADMOSCReceiver.h` warnings.
- You approve the 4 OSD follow-ups.

**Why human:** It is a cross-repo decision.

#### 2. The 0 to -30 degree band versus EAR
**Test:** Review the probe numbers. On 7.1.4 at az 60, SpatialCore gives L/Lss = 0.7472/0.6646 at el -15, against EAR's 0.7071/0.7071, and the lean side flips between -15 and -25. Max deviation from the horizon pan is 0.057.
**Expected:** Choose one:
- (a) Accept it as the #22 triangulation defect and correct the guide, README and skill wording (WR-01, widened).
- (b) Open a gap-closure plan.

**Why human:** No test covers the band, the docs and D-04 claim EAR behaviour there, and the magnitude (about 0.5 dB) is a product call.

#### 3. Silent degradation on hand-built LayoutContexts
**Test:** Review the two misuse paths:
- empty triplets on a height layout: in Release this falls through to a 2D pan that includes height speakers;
- regular-only triplets from `buildVBAPTripletsForLayout`: below-horizon sources take the D-06b fallback, with a Debug jassert on the audio thread.

**Expected:** Confirm that D-01 ("harden upstream") accepts both, with WR-03's docblock or entry-point fix as a follow-up.
**Why human:** SC2's "fails loudly" holds on the engine path but not on this public-API path.

#### 4. Backstop truth: the D-02a abort
**Test:** Optionally, break a `layoutDefs` height entry in a scratch branch, build Release, and call `setOutputFormat`.
**Expected:** The process aborts in `activateLayout`, never from `renderBlock`.
**Why human:** The truth is tagged `verification: backstop`, and no runtime test can exercise an abort.

#### 5. Process gates
**Test:** Two approvals to confirm:
- the 02-02 package-legitimacy gate, which was discharged by delegation to the orchestrator;
- the 02-03 approval of the exact text of #22 before filing.

**Expected:** Both were acceptable.
**Why human:** Past approvals are not visible in the codebase.

#### 6. Judgment-tier prohibitions (17)
**Test:** Review the Prohibitions table.
**Expected:** Accept or reject each judgment. 16 hold; 02-07's doc-accuracy prohibition partly fails (item 2).
**Why human:** These are unverified prohibitions, and human review is recommended.

### Gaps Summary

There are no blocking gaps. Every ROADMAP success criterion is met in the code and backed by a passing test that I ran myself.

The most important finding is **new and not covered by any test**. Below the horizon on height layouts, SpatialCore's lower-hemisphere construction matches the EAR oracle only in the nadir cap, which is all the `[ear]` test checks. In the 0 to -30 degree band, the planar trapezoids between the ear-level ring and the virtual -30 degree ring are triangulated. EAR pans them as quads. So a descending source leans up to 0.057 toward one speaker, and float rounding picks which one (same root cause as #22).

The integration guide, README and skill call this band "ITU-R BS.2127 (EAR)", and CONTEXT D-04 promised that "the horizon pan holds down to -30 degrees". Neither statement is exact. The audible effect is small, and a doc fix is enough unless the user wants the band panned as EAR quads. The decision is routed to the human.

Secondary concerns, all accepted designs or follow-ups, not criterion failures:
- the Release behaviour on hand-built LayoutContexts (WR-03 and the empty-triplet fall-through to 2D);
- the comment-only fast-math invariant (WR-04);
- the changed semantics of `vbapTriplets` (WR-02).

Tracking note: the evidence satisfies EXTR-01 and EXTR-03. Their REQUIREMENTS.md rows still read "Largely verified by tests" with unchecked boxes, and the orchestrator should update them. VERIFY-01 is already marked complete.

---

_Verified: 2026-10-01T07:52:54Z_
_Verifier: Claude (gsd-verifier)_
