# Requirements: SpatialCore

**Defined:** 2026-08-09
**Milestone:** v1 — "OpenSpatialDelay ships on SpatialCore as a submodule with zero regressions"
**Core Value:** A Spatial Media Lab plugin author gets production-grade spatial rendering by linking one library, so the only audio code they write is their own effect.

Source intel: `.planning/intel/requirements.md` (slug IDs preserved in the Traceability table).
Where the intel recorded no acceptance criteria, criteria below are derived from the governing
constraints in `.planning/intel/constraints.md` and from the observable-behaviour test — never
invented scope.

---

## v1 Requirements

### Public Contract Resolution

The `SpatializationAlgorithm` interface and everything consumers link against are frozen under
DR-7 and gated by DR-2. Ambiguity in a frozen contract is a defect. These four settle it before
any module work begins.

- [ ] **API-01**: The public algorithm count is one number, and headers, SPEC, PRD, and integration guide all state it
  - Acceptance: `SpatialCore.h` and `AllAlgorithms.h` expose exactly the decided set; no source doc states a different count; if `ConstantPowerAlgorithm` is ruled internal, it is absent from the umbrella header.
  - Open question: **OQ-1** — 6 vs 7 vs 8. Resolve with the user; do not pick a side during planning.

- [ ] **API-02**: `spatialcore::DSP::outputLimiter()` has one defined transfer function, and spec and implementation agree
  - Acceptance: Either `Utilities.h` restores the SPEC's hard clamp at 1.2589f, or the SPEC is amended to the tanh soft ceiling. A test pins the chosen curve at, below, and above the ceiling, and for non-finite input (returns 0.0f).
  - Open question: **OQ-2**.

- [ ] **API-03**: The number of selectable HRTF profiles is one number across docs, headers, and embedded data
  - Acceptance: CLAUDE.md, `docs/development-roadmap.md`, and the header exposing `profileIndex` state the same count; the count is the number of profiles DATA-01 embeds.
  - Open question: **OQ-3** — 5 vs 6.

- [ ] **AUDIT-01**: Every module carries a complete / partial / stub verdict backed by an executed probe
  - Acceptance: All 7 modules plus `Core/` have a recorded verdict with named gaps. Each verdict is backed by a probe that was compiled and run — reading the source and declaring it done does not satisfy this. The gap list is the scope input for Phases 2 through 4.
  - Rationale: the user has ruled the tree partial despite `.planning/codebase/` reporting it implemented. This requirement resolves that disagreement with evidence.

### Extraction Completion

Five requirements carried forward from the PRD. **Still open by user ruling** — the codebase map
reports them implemented; existing `src/` code is treated as partial work to be completed and
verified. Each phase starts with its slice of the AUDIT-01 gap list.

- [ ] **EXTR-01**: Every public spatialization algorithm computes correct gains — no zeroed or stubbed `computeGains()`
  - Acceptance: For each algorithm in the API-01 set, `computeGains()` returns non-zero gains obeying that algorithm's panning law across the supported layouts. The 3D triplet fallback path (`jassertfalse` → nearest-speaker heuristic in VBAP, VBIP, MDAP) is either eliminated or fails loudly.

- [ ] **EXTR-02**: HRTF/SOFA loading and convolution render real binaural audio
  - Acceptance: `HRTFDatabase` loads a SOFA profile and returns interpolated HRIRs with ITD; `PartitionedConvolver` performs FFT overlap-save with a click-free dual-slot transition; `BinauralRenderer` drives 12 per-source convolver pairs. Verified by measured output, not by inspection.

- [ ] **EXTR-03**: Speaker layouts, output format registry, and Ambisonics codec return real data
  - Acceptance: All 22 `OutputFormat` entries resolve to correct `OutputFormatInfo`; all 13 ITU-R factories return populated layouts with correct channel indices and LFE placement; `AmbisonicsCodec` encodes/decodes to order 6.

- [ ] **EXTR-04**: ADM-OSC receive/send and the trajectory engine drive real object motion
  - Acceptance: `ADMOSCReceiver` parses `/adm/obj/N/{azim,elev,dist,aed,xyz}` with ITU-R BS.2127-0 Cartesian-to-polar conversion; `ADMOSCSender` broadcasts `/adm/obj/N/aed` at 30 Hz with position-change gating; `TrajectoryEngine` animates all 13 shapes forward and reverse.

- [ ] **EXTR-05**: UI components render and interact for real — no stub paint methods
  - Acceptance: `SpatialMapComponent` renders objects, distance rings, and elevation-as-opacity, and supports drag; `SMLLookAndFeel` applies the SML palette; `ReverseSlider`, `IndicatorToggle`, `StyledButton` render and respond. No component holds a concrete processor pointer (DR-16).

### Embedded Data

- [ ] **DATA-01**: HRTF SOFA profiles are embedded as BinaryData and tracked in Git LFS
  - Acceptance: A fresh clone builds and renders binaural audio with no consumer-supplied HRTF file. The profile count matches API-03. The build fails with a clear error when a SOFA file is a Git LFS pointer rather than real data (pointer files are ~100 bytes; real files exceed 100 KB).
  - Locked by DR-5. No `.sofa` file and no BinaryData target exists in the tree today — this is unstarted work.

- [ ] **DATA-02**: `SMLLookAndFeel` has the font BinaryData it needs
  - Acceptance: SML typography renders correctly in a consumer plugin with no font installed on the host system.

### Realtime Safety

Four documented breaches of locked rule DR-1, plus the whole-path sweep. The four fixes are
tracked individually so none of them disappears inside the sweep.

- [ ] **RTSF-01**: `BinauralRenderer::renderSourceBuffers()` cannot allocate on the audio thread in a release build
  - Acceptance: The `jassertfalse`-then-resize path at `src/Binaural/BinauralRenderer.cpp:205-210` is gone. A mis-sized or missing `prepare()` fails loudly or is made impossible by construction-time maximum allocation. Verified in a Release build, where `jassertfalse` is a no-op.

- [ ] **RTSF-02**: TrajectoryEngine final positions are safe to read from the audio thread while the timer thread writes
  - Acceptance: `finalAz_`, `finalEl_`, `finalDist_` are no longer plain unsynchronised floats. A concurrent timer-writes / audio-reads test runs clean under thread sanitizer.

- [ ] **RTSF-03**: `HRTFDatabase` access to the libmysofa handle is thread-safe
  - Acceptance: `setProfile()` called from a UI timer while audio is processing does not race on `easyHandle` or `loaded`. Achieved by a lock outside the audio path, atomic double-buffering, or a documented-and-enforced threading contract — never by an unguarded shared handle.

- [ ] **RTSF-04**: `ADMOSCReceiver` rejects out-of-range object indices
  - Acceptance: An OSC message addressing `/adm/obj/999/...` is ignored rather than writing out of bounds. Bounds are checked against `MAX_SOURCES` (12) before any array access.

- [ ] **RTSF-05**: The whole audio path is verified lock-free and realtime-safe
  - Acceptance: A sweep over every function reachable from `processBlock` finds no allocation, no locks, and no logging, evidenced by an actual tool run (thread sanitizer plus an allocation detector) rather than a code read. Includes removing the `DBG()` calls compiled into release builds at `HRTFDatabase.cpp:36,50` and `BinauralRenderer.cpp:127`.

### Verification and Consumability

- [ ] **TEST-01**: OpenSpatialDelay tests are ported and coverage reaches the agreed target
  - Acceptance: Coverage meets the number set at phase start. The currently untested modules — binaural rendering, HRTF database, partitioned convolver, OSC, Ambisonics, output format registry — are exercised. Known issues #50, #89, #96, #131 each get a regression test.
  - Open question: **OQ-4** — target undefined in all source docs; CONCERNS.md recommends 50% (~2,000 lines). Set the number before planning this phase.

- [ ] **INTG-01**: A consumer plugin can submodule, link, and build against SpatialCore using only the umbrella header
  - Acceptance: A minimal harness plugin adds SpatialCore as a git submodule, calls `add_subdirectory(SpatialCore)` and `target_link_libraries(... PRIVATE SpatialCore)`, includes only `<SpatialCore/SpatialCore.h>`, and builds clean in Release on macOS. From a clean clone: `cmake -B build -DCMAKE_BUILD_TYPE=Release`, `cmake --build build --config Release`, and `./build/SpatialCoreTests` all succeed.
  - Derived from the SPEC's build verification gate (nfr) and from the v1 milestone metric. Verified with a throwaway harness, not by modifying OpenSpatialDelay.

## Future Milestones

Not in the v1 roadmap. Recorded so nothing is lost.

### v2 — A second plugin is built on SpatialCore from scratch

| ID | Requirement |
|----|-------------|
| REQ-plugin-openspatialreverb | OpenSpatialReverb — algorithmic reverb with spatial reflections |
| REQ-plugin-openspatialgranular | OpenSpatialGranular — granular synthesis with 3D grain positioning |
| REQ-plugin-openspatialpanner | OpenSpatialPanner — object-based panner with energy distribution |
| REQ-plugin-openspatialchorus | OpenSpatialChorus — chorus/flanger with spatially distributed voices |

One of these becomes the v2 milestone. All four target 2027.

### v3 — Public release under the org with docs and a tagged v1.0

| ID | Requirement |
|----|-------------|
| REQ-create-org-repo | Create `github.com/Spatial-Media-Lab/SpatialCore` with squashed history |
| REQ-set-dual-license | Land the GPL-3.0 + commercial dual license |
| REQ-tag-v1-0-0 | Tag v1.0.0 (gated by DR-4: full test suite passes first) |
| REQ-osd-repoint-submodule-to-org | Point OpenSpatialDelay's submodule at the public org repo (external repo) |

## Out of Scope

| Feature | Reason |
|---------|--------|
| OpenSpatialDelay repository work (7 × REQ-osd-*) | Lives in another repo. Tracked in PROJECT.md as external dependencies of v1; deliberately given no SpatialCore phase. |
| Windows / Linux CI verification | Cross-platform is an intent and the flags exist, but v1 verifies macOS only. Build matrices before a second consumer exists are premature. |
| Custom / vendor-specific speaker layouts | Only the 13 ITU-R layouts. Generalizing the layout API is a post-v1 additive change. |
| Replacing the JUCE FFT dependency | Medium risk in CONCERNS.md; swapping FFT libraries is disproportionate to v1. |
| Integrating `convertToMinPhase()` into the HRTF pipeline | Implemented but unused. Quality enhancement, not a v1 shipping gate. |
| Configurable convolver crossfade duration | Hardcoded `kCrossfadeBlocks = 4` works. Post-v1 tuning. |

## Traceability

| Requirement | Intel source ID | Phase | Status |
|-------------|-----------------|-------|--------|
| API-01 | (from INGEST-CONFLICTS OQ-1) | Phase 1 | Pending |
| API-02 | (from INGEST-CONFLICTS OQ-2) | Phase 1 | Pending |
| API-03 | (from INGEST-CONFLICTS OQ-3) | Phase 1 | Pending |
| AUDIT-01 | (from user ruling on Conflict 1) | Phase 1 | Pending |
| EXTR-01 | REQ-extract-algorithms | Phase 2 | Pending |
| EXTR-03 | REQ-extract-speaker-layouts-and-format-registry | Phase 2 | Pending |
| EXTR-02 | REQ-extract-binaural-rendering | Phase 3 | Pending |
| DATA-01 | REQ-embed-hrtf-binarydata | Phase 3 | Pending |
| EXTR-04 | REQ-extract-adm-osc-and-trajectory | Phase 4 | Pending |
| EXTR-05 | REQ-extract-ui-rendering | Phase 4 | Pending |
| DATA-02 | REQ-smllookandfeel-font-binarydata | Phase 4 | Pending |
| RTSF-01 | CONCERNS.md violation 1 | Phase 5 | Pending |
| RTSF-02 | CONCERNS.md violation 2 | Phase 5 | Pending |
| RTSF-03 | CONCERNS.md violation 3 | Phase 5 | Pending |
| RTSF-04 | CONCERNS.md violation 4 | Phase 5 | Pending |
| RTSF-05 | REQ-verify-lockfree-realtime-safe | Phase 5 | Pending |
| TEST-01 | REQ-test-port-and-coverage | Phase 6 | Pending |
| INTG-01 | (derived: build verification gate + v1 metric) | Phase 6 | Pending |

**Coverage:**
- v1 requirements: 18 total
- Mapped to phases: 18
- Unmapped: 0 ✓
- Duplicated across phases: 0 ✓

---
*Requirements defined: 2026-08-09*
*Last updated: 2026-08-09 after initial definition from docs ingest + codebase map*
