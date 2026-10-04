# Roadmap: SpatialCore

## Overview

Milestone **v1** ends when OpenSpatialDelay ships on SpatialCore as a submodule with zero
regressions.

**This roadmap was rewritten 2026-08-10.** The original assumed the extraction was unwritten and
opened with a large audit phase to prove it. That audit has now been run: the suite builds and
passes 144 test cases / 1585 assertions, the HRTF data is real, and the modules are substantially
complete. So the shape changed. The roadmap now opens by correcting the documentation that misled
the first planning pass, then fixes the specific defects that execution found, then closes the
realtime-safety breaches, then proves consumability against the real consumer.

**Scope:** v1 only. Milestones v2 (OpenSpatialPanner) and v3 (public org release) are recorded in
PROJECT.md and REQUIREMENTS.md, not decomposed here.

**Consumer ruling:** OpenSpatialDelay is the v1 consumer — it is the tree SpatialCore was
extracted from and the only one that can prove zero regressions. Verified 2026-08-10: OSD does not
yet consume SpatialCore; the migration is unstarted.

**Granularity:** standard. **Phase ID convention:** sequential.

## Phases

- [x] **Phase 1: Documentation Truth & Contract Freeze** - Every public number matches the tree, and CLAUDE.md stops mis-steering future sessions (completed 2026-08-15)
- [x] **Phase 2: Algorithm & Format Verification** - Ambisonics convention stated; gain paths verified against the panning laws (completed 2026-10-04)
- [ ] **Phase 3: Binaural Defects & HRTF Packaging** - The two inherited binaural bugs are fixed and a consumer gets HRTF data by linking
- [ ] **Phase 4: Control Surface & UI** - OSC, trajectories, and the spatial map verified by interaction
- [ ] **Phase 5: Realtime Safety Hardening** - Nothing on the audio path allocates, locks, or races, in Release builds
- [ ] **Phase 6: Consumer Readiness & CI** - OSD can submodule, link, and build — proven in CI

## Phase Details

### Phase 1: Documentation Truth & Contract Freeze

**Goal**: Every ambiguous public contract has one answer that matches the tree, and the project's
own context file stops describing a library that doesn't exist.
**Depends on**: Nothing (first phase)
**Requirements**: API-01, API-02, API-03, API-04, API-05, BUG-03
**Success Criteria** (what must be TRUE):

  1. Headers and docs state 8 algorithms, 5 HRTF profiles, 23 output formats, and 15 speaker layouts — the counts read directly from `OutputFormat.h:9-28` and `SpeakerLayout.h:42-45` on 2026-08-11.
  2. `outputLimiter()` has exactly one governing contract, and it describes the shipped tanh soft ceiling. Per D-06/D-07 (2026-08-11) that contract is the header docblock at `Core/SpatialMath.h:98-99`, which already states the curve correctly; the March scaffold plan's contradicting hard-clamp-at-1.2589f language is stamped historical rather than amended, because a historical doc must not serve as the governing contract (D-01). The implementation is unchanged. A test pins the curve at, below, and above the ceiling, and for non-finite input.
  3. CLAUDE.md's architecture table includes the `Engine/` module, states JUCE 9.0.0, and describes HRTF data the way it actually loads.
  4. Every `#NNN` in a code comment resolves in the tracker it names — OSD references are written `Spatial-Media-Lab/OpenSpatialDelay#NNN`.

**Open questions: none.** All four originals are closed. OQ-1 and OQ-3 were doc-counting errors
closed by evidence; **OQ-2 was resolved by the user 2026-08-10 — keep the shipped tanh soft
ceiling and amend the SPEC to match.** Rationale: OSD ships publicly at v1.0.0 with that curve, so
it is the known sound, and changing it during a zero-regressions migration would alter output for
existing users. A selectable hard-clamp mode is deferred to v2 as LIMIT-01.
**Why first**: A wrong CLAUDE.md is loaded into every session in this repo, and is the direct cause
of the 2026-08-09 planning pass being built on false premises. Fix the map before using it.
**Plans**: 5/5 plans executed

Plans:

- [x] 01-01-PLAN.md — Freeze the count contract in code: compile-time assertions for 23 formats / 15 layouts / 8 algorithms, plus a `[counts]` Catch2 backstop *(wave 1)*
- [x] 01-02-PLAN.md — Qualify all 39 cross-repo issue citations across 17 files as `Spatial-Media-Lab/OpenSpatialDelay#N` *(wave 2)*
- [x] 01-03-PLAN.md — Correct all five Tier A doc surfaces, including both auto-loading skill files *(wave 2)*
- [x] 01-04-PLAN.md — Give `outputLimiter()` one contract: named at/below/above-ceiling tests, and stamp the contradicting scaffold plan historical *(wave 3)*
- [x] 01-05-PLAN.md — Stamp the remaining Tier B documents and correct PROJECT.md / REQUIREMENTS.md's own factual errors *(wave 3)*

### Phase 2: Algorithm & Format Verification

**Goal**: Every algorithm and every advertised format is confirmed correct by a test, and the
Ambisonics convention is written down.
**Depends on**: Phase 1
**Requirements**: EXTR-01, EXTR-03, VERIFY-01
**Starting position**: Algorithms, IO, and Ambisonics already have passing tests. This phase closes
named gaps, it does not rebuild the modules.
**Success Criteria** (what must be TRUE):

  1. `AmbisonicsCodec`'s channel order and normalisation is confirmed ACN/SN3D or FuMa, stated in code and docs. *(SpatialCore#11)*
  2. The 3D triplet fallback in VBAP/VBIP/MDAP either no longer exists or fails loudly instead of silently degrading to nearest-speaker.
     *Amended 2026-10-04 (user decision, Phase 2 re-verification): holds for every layout RenderEngine builds: the shipped layouts are fully covered ([ear][coverage]), and the layout build aborts if a height layout yields no triplets (D-02a). A hand-built partial triplet list may snap to the nearest speaker without a diagnostic, because an audio-thread assert allocates.*
  3. All 23 `OutputFormat` entries resolve to correct info; all 15 layouts return populated channel indices and LFE placement.
  4. Ambisonics encode/decode round-trips a source position within tolerance at every order up to 6.

**Plans**: 10/10 plans complete (02-08 and 02-09 close UAT gap G-02-2; 02-10 closes G-02-10)

Plans:
**Wave 1**
- [x] 02-01-PLAN.md — Tracer: EAR lower-hemisphere panning end to end on 7.1.4, one height threshold, the Release layout-build abort, and deletion of the empty-triplet nearest-speaker branches *(wave 1)*
- [x] 02-02-PLAN.md — Independent reference oracles (scipy SH, PyPI ear 2.1.0, textbook panning) checked in under `tests/reference/`, behind a blocking package-legitimacy gate *(wave 1)*
- [x] 02-03-PLAN.md — File the coplanar-quad tie-break issue with the measured jumps, after human approval of the text *(wave 1)*

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 02-04-PLAN.md — Playback safety in both layers (non-finite silence, bounded wrap, best-triplet fallback, engine hold-last-good) and the ear-oracle / coverage verification of the lower hemisphere *(wave 2)*
- [x] 02-05-PLAN.md — Textbook single-band VBIP, MDAP/DBAP corrections, and the panning-law suite for all 8 algorithms with D-18-scoped continuity *(wave 2)*

**Wave 3** *(blocked on Wave 2 completion)*
- [x] 02-06-PLAN.md — One SH evaluator with corrected SN3D orders 4-6, the ACN/SN3D/no-Condon-Shortley convention in code, the decode guard, one decoder, and the 23-format cross-check *(wave 3)*

**Wave 4** *(blocked on Wave 3 completion)*
- [x] 02-07-PLAN.md — README, integration guide and skill brought in line with the code; OpenSpatialDelay DR-3 build; phase gate; cross-repo follow-ups recorded *(wave 4)*

**Gap closure — UAT G-02-2** (below-horizon band must pan as ITU-R BS.2127 / EAR; user chose to change the panning)
- [x] 02-08-PLAN.md — Tracer on 7.1.4 az 60, then all 8 height layouts: one pair-pan region per ear-level pair replaces the tying trapezoid triangles; ear 2.1.0 band pins; band continuity 0.12 -> 0.01; above-horizon bit-identical (#22 untouched) *(gap wave 1)*
- [x] 02-09-PLAN.md — Guide, README and skill state the EAR-exact band and the 5.1.4 rear-gap exception, #22 scoped above the horizon (WR-01 fixed); phase gate; DR-3 OSD build; superseding OSD follow-ups *(gap wave 2)*

**Gap closure — UAT G-02-10** (full suite must pass in Release, the build type CI uses; `[ambi-pin]` fails on Apple Silicon Release only)
- [x] 02-10-PLAN.md — Tracer: derived `[ambi-pin]` float bound (2.5e-5, derivation beside it) plus a double-precision anchor, red then green in a Release build of this tree and green in Debug; +0.1% epsilon mutation must still fail; phase gate in Debug and Release; Release gate written into VALIDATION and TESTING.md; library and verbatim reference untouched *(gap wave 1)*

### Phase 3: Binaural Defects & HRTF Packaging

**Goal**: The binaural path renders correct spatial cues, and a consumer gets HRTF data by linking
rather than by hand-copying a directory.
**Depends on**: Phase 2
**Requirements**: EXTR-02, DATA-01, BUG-01, BUG-02
**Success Criteria** (what must be TRUE):

  1. A source at elevation +90° is measurably distinguishable from one at 0°, and azimuth 0° from 180°. *(SpatialCore#15 — the same defect exists in OSD, so this fix reaches both)*
  2. Rendering at 32, 64, and 128 sample blocks produces no artifacts. *(`Spatial-Media-Lab/OpenSpatialDelay#234`, still open, cited in live code at `PartitionedConvolver.cpp:120-124`)*
  3. A freshly cloned consumer links SpatialCore and renders through any of the 5 profiles with no install step and no path configuration. A SOFA file dropped into the shared platform folder is picked up ahead of the embedded copy, and a missing folder falls back silently to embedded rather than failing.
  4. Switching HRTF profile while audio is running produces no click, pop, or dropout.
  5. `PartitionedConvolver` and `BinauralRenderer` have dedicated test files — today they are only exercised indirectly.

**Open questions: none. OQ-6 resolved 2026-08-10 — lookup chain now, embedded default for v1.**
The user proposed that the 5 profiles live on disk once per machine
(`/Library/Application Support/Spatial Media Lab/HRTF/` on macOS,
`%ProgramData%\Spatial Media Lab\HRTF\` on Windows) with every SML plugin referencing them
instead of carrying a copy. **Adopted as the target architecture** — it is better than what OSD
does today, chiefly because it makes user-supplied SOFA files possible at all, and secondarily
because it lets profiles be fixed without re-shipping plugins and stops 58 MB of BinaryData
slowing every consumer's clean build.

**Decisive context:** OSD already embeds all 5 raw files via `juce_add_binary_data(HRTFData ...)`
(`CMakeLists.txt:50-58`) and ships a 64 MB VST3, 64 MB AU, and 73 MB macOS zip at v1.0.0. The
embedded approach is proven in production; the shared folder is the improvement on it.

**But v1 ships embedded anyway.** The shared folder depends on a signed installer that does not
exist, and OSD ships drag-and-drop today — so cutting the bundle before the installer lands means
an existing user who updates by drag-and-drop silently loses 4 of 5 profiles. That is a regression
in a milestone gated on zero regressions. "Migrate OSD onto SpatialCore" and "repackage OSD" are
independently risky and must not ride together.

**So Phase 3 builds the mechanism and ships it switched off:** `HRTFDatabase` gets a resolution
chain (shared folder → embedded BinaryData → loud error) plus a `SPATIALCORE_EMBED_ALL_HRTF`
CMake option defaulting to ON. When the installer lands, set it OFF: embedding drops to
`mit_kemar_large_pinna` (1.1 MB) and the installer supplies the rest, with **no SpatialCore code
change**. Tracked as SUITE-01.

**Rejected:** convert-then-embed (min-phase/int16 compaction to ~4 MB). It adds DSP work that can
change the sound during a zero-regressions migration, to solve a size problem the shipping product
proves it does not have.

**Corrected premise**: the original roadmap said "no `.sofa` file and no BinaryData target exists
anywhere in the tree, so DATA-01 is unstarted." Half wrong: 5 real HDF5 files (1.2–36.6 MB) are
present and LFS-tracked with a CI guard against pointer stubs. Only the embedding is absent.
**Plans**: 2/11 plans executed (run one at a time: every plan builds in the shared `build/` and `build-release/` trees, so each wave holds exactly one plan)

Plans:
**Wave 1**
- [x] 03-01-PLAN.md — Test foundation: shared metrics header, dedicated PartitionedConvolver and BinauralRenderer test files, HRTF-path cue test on all 5 profiles, D-14 loudness record, D-16 ITD characterisation, signature and legacy-swap baselines; sole owner of tests/CMakeLists.txt *(wave 1)*

**Wave 2** *(blocked on Wave 1 completion)*
- [x] 03-02-PLAN.md — Binaural goldens as tolerance fingerprints (HUTUBS Debug failure closed), with negative controls *(wave 2)*

**Wave 3** *(blocked on Wave 2 completion)*
- [ ] 03-03-PLAN.md — BUG-02: sample-based convolver warm-up/crossfade (moving source at 32 as smooth as 512), steady-state block-plan match, KEMAR scratch sized off the audio thread *(wave 3)*

**Wave 4** *(blocked on Wave 3 completion)*
- [ ] 03-04-PLAN.md — BUG-01: Simple-path rear/up/down cue bank (D-01), ear-level front half bit-identical, no legacy switch (D-02) *(wave 4)*

**Wave 5** *(blocked on Wave 4 completion)*
- [ ] 03-05-PLAN.md — DATA-01 data side: SpatialCoreHRTFData embedding, SPATIALCORE_EMBED_ALL_HRTF, LFS guard, loadFromBinaryData, shared folder → embedded → error resolver (D-06..D-11, D-18) *(wave 5)*

**Wave 6** *(blocked on Wave 5 completion)*
- [ ] 03-06-PLAN.md — Engine-owned switching: setHRTFProfile, background loader, mailbox claim, status, latest-wins, prepare during load (D-04, D-05, D-06) *(wave 6)*

**Wave 7** *(blocked on Wave 6 completion)*
- [ ] 03-07-PLAN.md — Concurrency proof of engine-owned switching: render thread plus switching thread, shutdown mid-load, ThreadSanitizer run with every report classified (D-05) *(wave 7)*

**Wave 8** *(blocked on Wave 7 completion)*
- [ ] 03-08-PLAN.md — libmysofa v1.3.5 evaluated in a disposable copy; the pin moves only after the user's choice, and the user signs off before any reference number moves (D-17) *(wave 8)*

**Wave 9** *(blocked on Wave 8 completion)*
- [ ] 03-09-PLAN.md — Click-free switching at 32/64/128/512: sample-based renderer crossfade and the opt-in engineSelectsHRTF Simple ↔ HRTF crossfade (D-15) *(wave 9)*

**Wave 10** *(blocked on Wave 9 completion)*
- [ ] 03-10-PLAN.md — Docs truth (CLAUDE.md, README, guide, skills, PROJECT.md follow-ups) and phase gate: Debug + Release, KEMAR-only build, consumer-mode proof, DR-3 OSD build *(wave 10)*

**Wave 11** *(blocked on Wave 10 completion)*
- [ ] 03-11-PLAN.md — Outward actions after user approval: OSD#234 comment (D-13), ITD-wrap issue (D-16), close SpatialCore#15 (D-03), loudness tolerance (D-14); ends with the full suite in both build types *(wave 11)*

### Phase 4: Control Surface & UI

**Goal**: An object's position can be driven by OSC, animated by a trajectory, and dragged on
screen — each route reaching the renderer.
**Depends on**: Phase 3
**Requirements**: EXTR-04, EXTR-05, DATA-02
**Starting position**: OSC and Trajectory have passing tests. All 10 `src/UI/*.cpp` files are
untested by design — UI lives in the separate `SpatialCoreUI` target that `SpatialCoreTests` does
not link — so UI verification here is by interaction, not by unit test.
**Success Criteria** (what must be TRUE):

  1. An external ADM-OSC sender moves a rendered object via `/adm/obj/N/{azim,elev,dist,aed,xyz}`.
  2. SpatialCore broadcasts positions at 30 Hz and stops re-sending while positions are static.
  3. Every trajectory shape animates forward and reverse.
  4. A host plugin embeds `SpatialMapComponent`, drags an object, and sees position, distance ring, and elevation opacity update — with SML fonts rendering on a machine with no SML font installed.

**Already verified**: `DATA-02` is done — `fonts/*.ttf` are compiled in as JUCE BinaryData.
`ADMOSCReceiver.cpp:43` bounds-checks correctly, closing the original RTSF-04 as a non-finding.
**Plans**: TBD
**UI hint**: yes

### Phase 5: Realtime Safety Hardening

**Goal**: Nothing reachable from `processBlock` allocates, locks, or races — proven in a Release
build with tools.
**Depends on**: Phase 4
**Requirements**: RTSF-01, RTSF-02, RTSF-03, RTSF-05, VERIFY-02
**Success Criteria** (what must be TRUE):

  1. Neither `BinauralRenderer.cpp:196-209` (`renderSourceBuffers`) nor `:134,158-163` (`updateSourceHRIR`, reached from `RenderEngine.cpp:178`) can allocate in a Release build. Both currently guard a `resize()` with `jassertfalse`, which is a no-op in Release. *(SpatialCore#19)*
  2. `TrajectoryEngine`'s `finalAz_`/`finalEl_`/`finalDist_` are no longer plain floats read across threads. Clean under thread sanitizer. *(SpatialCore#18)*
  3. `setProfile()` from a UI timer during audio processing does not race on the libmysofa handle.
  4. Several plugin instances in one host process render concurrently without crashing or buzzing. *(SpatialCore#12; regression tests for `OpenSpatialDelay#96` and `#131`)*
  5. A thread-sanitizer plus allocation-detector pass over the `processBlock` path reports zero allocations, zero locks, zero races.

**Why criterion 4 is the highest-value item in v1**: OSD#96 and OSD#131 were multi-instance
crashes, fixed in OpenSpatialDelay. That fix now lives in SpatialCore's `SharedFFTCache`. If it
regressed during extraction, migrating OSD reintroduces two crashes into a plugin that is already
shipping publicly at v1.0.0.
**Scope corrections from the 2026-08-10 audit**: `DBG()` calls at `BinauralRenderer.cpp:127` and
`HRTFDatabase.cpp:35,58,72` are configuration-thread only and compile out in Release — removed
from scope. `SharedFFTCache` is already correctly spinlock-guarded at
`PartitionedConvolver.cpp:16`; what remains unverified is concurrent-instance behaviour.
**Locked rule in play**: DR-1.
**Plans**: TBD

### Phase 6: Consumer Readiness & CI

**Goal**: OpenSpatialDelay can adopt SpatialCore as a submodule without surprises, proven by a
harness and a green CI run.
**Depends on**: Phase 5
**Requirements**: INTG-01, INTG-02, CI-01, TEST-01
**Success Criteria** (what must be TRUE):

  1. A minimal harness plugin submodules SpatialCore, links **both** `SpatialCore` and `SpatialCoreUI`, includes only `<SpatialCore/SpatialCore.h>`, resolves HRTF data per the Phase 3 decision, and builds clean in Release on macOS.
  2. A consumer that does not set `engineComputesGains` either gets computed gains anyway or fails loudly — it does not silently fall back to the pass-through path that SpatialCore#14 was filed for.
  3. A green macOS CI run builds against JUCE 9.0.0 and runs the Catch2 suite. *(SpatialCore#9)*
  4. Coverage instrumentation is wired into CMake and reports a real number, and `PartitionedConvolver.cpp` and `BinauralRenderer.cpp` have dedicated tests.
  5. `Spatial-Media-Lab/OpenSpatialDelay#50`, `#89`, `#96`, `#131`, and `#234` each have a regression test that fails against the old behaviour.

**Open question**: OQ-4 — the coverage target. It could not be set before because no coverage
tooling exists to measure against; the old "~5% today" figure was never measured. Wire up
instrumentation first, read the real number, then set the target with the user.
**Citation fix**: the original roadmap cited these as bare `#50, #89, #96, #131`, which resolve to
nothing in SpatialCore's tracker (highest issue: #17). They are real OpenSpatialDelay issues —
all four closed there, in code SpatialCore now owns — plus `#234`, which is still open.
**Milestone boundary**: v1 closes when OpenSpatialDelay itself ships on SpatialCore. That work
lives in the OpenSpatialDelay repository and has deliberately no SpatialCore phase — see PROJECT.md
External Dependencies.
**Plans**: TBD

## Progress

**Execution Order:** 1 → 2 → 3 → 4 → 5 → 6

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Documentation Truth & Contract Freeze | 5/5 | Complete    | 2026-08-15 |
| 2. Algorithm & Format Verification | 10/10 | Complete    | 2026-10-04 |
| 3. Binaural Defects & HRTF Packaging | 2/11 | In Progress|  |
| 4. Control Surface & UI | 0/TBD | Not started | - |
| 5. Realtime Safety Hardening | 0/TBD | Not started | - |
| 6. Consumer Readiness & CI | 0/TBD | Not started | - |

## Coverage

25 active v1 requirements map to exactly one phase. RTSF-04 was closed as a non-finding.
See `.planning/REQUIREMENTS.md` Traceability for the full table and the 14-issue triage.

| Phase | Requirements |
|-------|--------------|
| 1 | API-01, API-02, API-03, API-04, API-05, BUG-03 |
| 2 | EXTR-01, EXTR-03, VERIFY-01 |
| 3 | EXTR-02, DATA-01, BUG-01, BUG-02 |
| 4 | EXTR-04, EXTR-05, DATA-02 |
| 5 | RTSF-01, RTSF-02, RTSF-03, RTSF-05, VERIFY-02 |
| 6 | INTG-01, INTG-02, CI-01, TEST-01 |

## GitHub Issue Coverage

All open SpatialCore issues are triaged in REQUIREMENTS.md (14 at rewrite time, plus #18 and #19 filed 2026-08-10). Summary:

| In v1 | Issue |
|-------|-------|
| Phase 2 | #11 Ambisonics ACN/SN3D convention |
| Phase 3 | #15 Binaural elevation cue collapse |
| Phase 5 | #12 SharedFFTCache concurrent instances, #18 TrajectoryEngine race, #19 audio-thread allocation |
| Phase 6 | #9 CI vs JUCE 9.0.0, and #14's residual opt-in trap |

Deferred to v2 (OpenSpatialPanner): #2, #3, #4, #5, #6, #7, #10, #13, #16, #17.
None is dropped — see the REQUIREMENTS.md triage table for the reason on each.

## Backlog

### Phase 999.1: Follow-up — Phase 02 deferred UAT follow-up: Test 7 (BACKLOG)

**Goal:** Resolve the UAT checkpoint deferred during Phase 02 verification
**Source phase:** 02
**Deferred at:** 2026-10-03 during /gsd-verify-work 02 session completion
**Follow-ups:**
- [ ] Test 7: Next listening-review round — in OpenSpatialDelay on 7.1.4, lower a sound from ear height to 30 degrees below at about 60 degrees left; confirm it stays put (no drift toward one speaker, no side flip) (deferred 2026-10-03)

### Phase 999.2: Follow-up — Phase 02 deferred UAT follow-up: Test post-review (BACKLOG)

**Goal:** Resolve the UAT checkpoint deferred during Phase 02 verification
**Source phase:** 02
**Deferred at:** 2026-10-04 during /gsd-verify-work 02 session completion
**Follow-ups:**
- [ ] Test post-review: Listening checks for the 2026-10-04 code-review behaviour changes: (a) on a front-only rig (e.g. speakers at 0 and +-30 only), move a sound behind and below the listener and confirm it is never silent and moves smoothly across the gap (WR-05 gap bridge); (b) a DBAP source fed a broken distance (NaN/Inf) still plays, panned as distance 0.5 (WR-08) (deferred 2026-10-04)

### Phase 999.3: Custom HRTF import + loudness standard (BACKLOG)

**Goal:** A user imports their own measured `.sofa` HRTF from the plugin, names it, and keeps using it across sessions at the same perceived level as the built-ins
**Source phase:** 03
**Deferred at:** 2026-10-04 during /gsd-discuss-phase 3 (user has a custom `.sofa`; chose a later milestone over v1)
**Follow-ups:**
- [ ] "Import…" as the last item of the binaural profile dropdown; import copies the file into a personal library and asks for a display name
- [ ] Session opened without the custom file plays KEMAR + warning and keeps the custom choice
- [ ] Define a perceived-loudness standard (pink noise, ear-weighted, over all directions), apply it to custom and built-in profiles; OSD release note for the built-in level change. Input: Phase 3 D-14 measurement
- [ ] Decide shared `SpatialCoreUI` widget vs per-plugin import UI
- Full decision record: `.planning/phases/03-binaural-defects-hrtf-packaging/03-CONTEXT.md` § Deferred Ideas

### Phase 999.4: Listening check — Simple-mode height and rear cues (BACKLOG)

**Goal:** Confirm on headphones that Phase 3's new Simple (Woodworth) cues read as above and behind without sounding unnatural
**Source phase:** 03
**Deferred at:** 2026-10-04 during /gsd-discuss-phase 3 (D-03: test closes SpatialCore#15; listening is not a gate)
**Follow-ups:**
- [ ] After Phase 3 lands: in OSD Simple binaural mode, move a sound from ahead to overhead and from front to back; confirm each is audible and the ear-level front sound is close to before

---
*Roadmap rewritten 2026-08-10 against branch `gsd-remap`, after the original was found to have been planned against a branch missing 42 commits.*
