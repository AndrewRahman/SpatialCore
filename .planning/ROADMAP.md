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

- [ ] **Phase 1: Documentation Truth & Contract Freeze** - Every public number matches the tree, and CLAUDE.md stops mis-steering future sessions
- [ ] **Phase 2: Algorithm & Format Verification** - Ambisonics convention stated; gain paths verified against the panning laws
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
**Plans**: 5 plans in 3 waves

Plans:
- [ ] 01-01-PLAN.md — Freeze the count contract in code: compile-time assertions for 23 formats / 15 layouts / 8 algorithms, plus a `[counts]` Catch2 backstop *(wave 1)*
- [ ] 01-02-PLAN.md — Qualify all 39 cross-repo issue citations across 17 files as `Spatial-Media-Lab/OpenSpatialDelay#N` *(wave 2)*
- [ ] 01-03-PLAN.md — Correct all five Tier A doc surfaces, including both auto-loading skill files *(wave 2)*
- [ ] 01-04-PLAN.md — Give `outputLimiter()` one contract: named at/below/above-ceiling tests, and stamp the contradicting scaffold plan historical *(wave 3)*
- [ ] 01-05-PLAN.md — Stamp the remaining Tier B documents and correct PROJECT.md / REQUIREMENTS.md's own factual errors *(wave 3)*

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
  3. All 23 `OutputFormat` entries resolve to correct info; all 15 layouts return populated channel indices and LFE placement.
  4. Ambisonics encode/decode round-trips a source position within tolerance at every order up to 6.
**Plans**: TBD

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
**Plans**: TBD

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
| 1. Documentation Truth & Contract Freeze | 0/TBD | Not started | - |
| 2. Algorithm & Format Verification | 0/TBD | Not started | - |
| 3. Binaural Defects & HRTF Packaging | 0/TBD | Not started | - |
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

---
*Roadmap rewritten 2026-08-10 against branch `gsd-remap`, after the original was found to have been planned against a branch missing 42 commits.*
