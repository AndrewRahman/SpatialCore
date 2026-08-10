# Roadmap: SpatialCore

## Overview

Milestone **v1** ends when OpenSpatialDelay ships on SpatialCore as a submodule with zero
regressions. The journey there has a specific shape because of one ruling: the working tree looks
finished but is treated as partial until proven otherwise. So the roadmap opens by settling the
frozen public contracts and producing an evidence-backed gap list, then completes and verifies the
extraction module group by module group — spatialization core, then binaural and its HRTF data,
then the control and UI surface. With functionality final, a dedicated phase closes the four
documented breaches of the locked realtime-safety rule and proves the whole audio path clean. The
last phase raises test coverage and proves a real consumer can submodule, link, and build against
the library.

**Scope:** v1 only. Milestones v2 (a second plugin built from scratch) and v3 (public org release
with a tagged v1.0) are recorded in PROJECT.md, not decomposed here.

**Granularity:** standard (no `.planning/config.json` present; default applied).
**Phase ID convention:** sequential (default).

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Public Contract Freeze & Extraction Audit** - Settle the ambiguous frozen contracts and replace "looks done" with an evidence-backed gap list
- [ ] **Phase 2: Spatialization Core** - Every algorithm produces correct gains for every supported output format
- [ ] **Phase 3: Binaural Rendering & HRTF Data** - Headphone rendering through real measured HRTF data shipped inside the library
- [ ] **Phase 4: Control Surface & UI** - Object positions driven by OSC, animated by trajectories, and edited on screen
- [ ] **Phase 5: Realtime Safety Hardening** - Nothing on the audio path allocates, locks, or races — in release builds, not just debug
- [ ] **Phase 6: Verification & Consumer Readiness** - Provably correct and provably consumable as a submodule

## Phase Details

### Phase 1: Public Contract Freeze & Extraction Audit
**Goal**: Every ambiguous public contract has exactly one answer recorded in both code and docs, and every module carries a complete/partial/stub verdict that was earned by running the code rather than reading it.
**Depends on**: Nothing (first phase)
**Requirements**: API-01, API-02, API-03, AUDIT-01
**Success Criteria** (what must be TRUE):
  1. A plugin author reading any SpatialCore header or doc finds the same public algorithm count, and the umbrella header exposes exactly that set.
  2. `spatialcore::DSP::outputLimiter()` behaves the way the governing spec says it does — spec and implementation no longer describe different curves.
  3. The number of selectable HRTF profiles is one number across CLAUDE.md, the roadmap doc, and the header exposing `profileIndex`.
  4. Each of the 7 modules plus `Core/` has a recorded verdict with named gaps, and every verdict cites a probe that was compiled and executed.
**Open questions to resolve with the user before planning**: OQ-1 (algorithm count 6/7/8), OQ-2 (`outputLimiter` hard clamp vs tanh), OQ-3 (HRTF profile count 5 vs 6). These are frozen, major-version-gated contracts under DR-2 and DR-7 — surface them, do not choose silently.
**Why first**: The three contract questions span three different modules, so they belong to no single module phase, and the gap list from AUDIT-01 is the scope input for Phases 2 through 4.
**Plans**: TBD

### Phase 2: Spatialization Core
**Goal**: A source position and an output format together produce correct speaker gains, for every algorithm and every layout the library advertises.
**Depends on**: Phase 1
**Requirements**: EXTR-01, EXTR-03
**Audit first**: Start from the Phase 1 gap list for `Algorithms/`, `IO/`, and `Core/`. Confirm by execution what is genuinely complete before writing code — the codebase map reports 8 algorithms, 22 formats, and 13 layouts as implemented, and the user has ruled that reading non-authoritative.
**Success Criteria** (what must be TRUE):
  1. Selecting any public algorithm with any of the 22 output formats yields non-zero gains that obey that algorithm's panning law.
  2. A layout with height channels pans smoothly through VBAP triplets instead of collapsing to the nearest-speaker fallback.
  3. Requesting any of the 13 ITU-R layouts returns a populated layout with correct channel indices and LFE placement.
  4. Ambisonics encode/decode round-trips a source position within tolerance at every order up to 6.
**Grouping rationale**: Algorithms and IO are one deliverable, not two — `computeGains()` consumes `LayoutContext`, which is built from `SpeakerLayout` and its VBAP triplets. The documented triplet gap spans both modules and cannot be closed from either side alone.
**Plans**: TBD

### Phase 3: Binaural Rendering & HRTF Data
**Goal**: A source position renders to headphones through real measured HRTF data that ships inside the library.
**Depends on**: Phase 2
**Requirements**: EXTR-02, DATA-01
**Audit first**: Start from the Phase 1 gap list for `Binaural/`. Note one gap is already confirmed — no `.sofa` file and no BinaryData target exists anywhere in the tree, so DATA-01 is unstarted regardless of what CLAUDE.md asserts.
**Success Criteria** (what must be TRUE):
  1. A freshly cloned repo builds and renders binaural audio with no consumer-supplied HRTF file.
  2. Switching HRTF profile while audio is running produces no audible click, pop, or dropout.
  3. The build fails with a clear error when an embedded SOFA file is a Git LFS pointer instead of real data.
  4. A hard-left and a hard-right source show the expected interaural time and level difference in the rendered output.
**Locked decision in play**: DR-5 — SpatialCore owns and embeds the profiles. SPEC decision DR-15 (consumer-supplied raw pointers) is superseded. `loadFromMemory()` may remain as a secondary API, but embedded BinaryData is the shipping path.
**Plans**: TBD

### Phase 4: Control Surface & UI
**Goal**: An object's position can be driven from outside by OSC, animated by a trajectory, and dragged on screen — with each route reaching the renderer.
**Depends on**: Phase 3
**Requirements**: EXTR-04, EXTR-05, DATA-02
**Audit first**: Start from the Phase 1 gap list for `OSC/`, `Trajectory/`, and `UI/`. The codebase map reports 1,150+ lines of working UI and a 13-shape trajectory engine; verify by interaction, not by line count.
**Success Criteria** (what must be TRUE):
  1. An external ADM-OSC sender moves a rendered object via `/adm/obj/N/{azim,elev,dist,aed,xyz}`.
  2. SpatialCore broadcasts object positions at 30 Hz and stops re-sending while positions are static.
  3. Selecting any of the 13 trajectory shapes animates the object along that shape, forward and reverse.
  4. A host plugin embeds `SpatialMapComponent`, drags an object, and sees position, distance ring, and elevation opacity update — with SML fonts rendering on a machine that has no SML font installed.
**Grouping rationale**: These three are the object-position surface — everything that moves a position or shows one. The spatial map both displays and edits what OSC and trajectories write, and the font data exists only to make the UI render correctly.
**Plans**: TBD
**UI hint**: yes

### Phase 5: Realtime Safety Hardening
**Goal**: Nothing reachable from `processBlock` allocates, locks, logs, or races — proven in a release build with tools, not asserted in a debug build.
**Depends on**: Phase 4
**Requirements**: RTSF-01, RTSF-02, RTSF-03, RTSF-04, RTSF-05
**Success Criteria** (what must be TRUE):
  1. `BinauralRenderer::renderSourceBuffers()` cannot resize a buffer in a Release build; a mis-sized `prepare()` fails loudly or is made impossible by construction-time allocation.
  2. Trajectory positions read by the audio thread while the timer thread writes them are never torn, verified under thread sanitizer.
  3. Calling `setProfile()` from a UI timer while audio is processing does not race on the libmysofa handle.
  4. An ADM-OSC message carrying an out-of-range object index is ignored instead of writing past the end of an array.
  5. A thread-sanitizer plus allocation-detector pass over the `processBlock` path reports zero allocations, zero locks, and zero data races, and no `DBG()` output remains compiled into release.
**Why here and not earlier**: These four fixes live in files that Phases 3 and 4 modify. Running them last means one consistent thread-safety approach applied across all modules at once, and a verification sweep (RTSF-05) that stays valid because no functional code lands after it. The four named violations are tracked as individual requirements precisely so none of them dissolves into the sweep.
**Locked rule in play**: DR-1. All four items are documented live breaches of it in `.planning/codebase/CONCERNS.md`, which ranks them Critical / address before production release.
**Plans**: TBD

### Phase 6: Verification & Consumer Readiness
**Goal**: The library is provably correct and a plugin can adopt it as a submodule without surprises — the gate OpenSpatialDelay migrates through.
**Depends on**: Phase 5
**Requirements**: TEST-01, INTG-01
**Open question to resolve before planning**: OQ-4 — the test coverage target is undefined in every source doc. CONCERNS.md measures ~5% today and recommends 50% minimum. Set the number with the user at phase start; TEST-01 cannot be sized without it.
**Success Criteria** (what must be TRUE):
  1. The full test suite passes and hits the agreed coverage target, with binaural rendering, HRTF loading, the partitioned convolver, OSC parsing, Ambisonics, and the format registry all exercised.
  2. Known issues #50, #89, #96, and #131 each have a regression test that fails against the old behaviour.
  3. A minimal consumer plugin adds SpatialCore as a git submodule, includes only `<SpatialCore/SpatialCore.h>`, links it, and builds clean in Release on macOS.
  4. From a clean clone, `cmake -B build -DCMAKE_BUILD_TYPE=Release`, `cmake --build build --config Release`, and `./build/SpatialCoreTests` all succeed.
**Milestone boundary**: v1 closes when OpenSpatialDelay itself ships on SpatialCore. That work lives in the OpenSpatialDelay repository (REQ-osd-consume-spatialcore-submodule) and has deliberately no SpatialCore phase — see PROJECT.md External Dependencies.
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Public Contract Freeze & Extraction Audit | 0/TBD | Not started | - |
| 2. Spatialization Core | 0/TBD | Not started | - |
| 3. Binaural Rendering & HRTF Data | 0/TBD | Not started | - |
| 4. Control Surface & UI | 0/TBD | Not started | - |
| 5. Realtime Safety Hardening | 0/TBD | Not started | - |
| 6. Verification & Consumer Readiness | 0/TBD | Not started | - |

## Coverage

All 18 v1 requirements map to exactly one phase. No orphans, no duplicates.
See `.planning/REQUIREMENTS.md` Traceability for the full table.

| Phase | Requirements |
|-------|--------------|
| 1 | API-01, API-02, API-03, AUDIT-01 |
| 2 | EXTR-01, EXTR-03 |
| 3 | EXTR-02, DATA-01 |
| 4 | EXTR-04, EXTR-05, DATA-02 |
| 5 | RTSF-01, RTSF-02, RTSF-03, RTSF-04, RTSF-05 |
| 6 | TEST-01, INTG-01 |
