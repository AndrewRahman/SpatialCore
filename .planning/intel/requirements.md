# Requirements

> **Historical — superseded by `.planning/ROADMAP.md`, 2026-08-10.** This document was derived
> from the 2026-08-09 ingest, which ran against a branch missing 42 commits. Counts, status,
> and constraints below are a dated snapshot and are not maintained. For current canonical
> counts see `CLAUDE.md` and `README.md`; for current requirements and phase status see
> `.planning/REQUIREMENTS.md` and `.planning/ROADMAP.md`.

Synthesized: 2026-08-09
Paths are relative to repo root `/Users/andrewrahman/conductor/workspaces/SpatialCore/kelowna/`.

Source: `docs/development-roadmap.md` (PRD — the only forward-looking scope document in this ingest
set). Only items the PRD leaves open are recorded; PRD deliverables already checked `[x]` are
recorded as decisions/constraints instead. Several requirements below are contradicted by
`.planning/codebase/` which shows them as already implemented — see `.planning/INGEST-CONFLICTS.md`
before routing. Where the PRD states no acceptance criteria, the field is marked absent rather than
inferred.

---

## REQ-extract-algorithms
- source: docs/development-roadmap.md (Phase 1 Deliverables: "Extract real algorithm implementations from OpenSpatialDelay"; Phase 1 module table; "What Does NOT Exist Yet")
- description: Extract the real spatialization algorithm implementations (VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural) from OpenSpatialDelay's PluginProcessor.h/cpp into SpatialCore's Algorithms module, replacing the stubs whose `computeGains()` return zeroes.
- acceptance: (absent — source specifies no acceptance criteria beyond "all 7 SpatializationAlgorithm implementations" extracted and `computeGains()` no longer returning zeroes)
- scope: Algorithms module, SpatialCore, OpenSpatialDelay extraction

## REQ-extract-binaural-rendering
- source: docs/development-roadmap.md (Phase 1 Deliverables: "Extract real HRTF/binaural rendering from OpenSpatialDelay"; Phase 1 module table; "What Does NOT Exist Yet")
- description: Extract real HRTF/SOFA loading and convolution from OpenSpatialDelay — HRTFDatabase (SOFA/libmysofa), PartitionedConvolver (FFT overlap-save), BinauralRenderer (12 per-source convolvers) — replacing the Binaural stubs.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: Binaural module, SpatialCore, OpenSpatialDelay extraction

## REQ-extract-speaker-layouts-and-format-registry
- source: docs/development-roadmap.md (Phase 1 Deliverables: "Extract real speaker layout data and format registry"; Phase 1 module table; "What Does NOT Exist Yet")
- description: Extract real OutputFormatRegistry (22 formats), SpeakerLayout data (13 ITU-R layouts) and AmbisonicsCodec (SH eval, decode matrices) from OpenSpatialDelay, replacing factory functions that currently return empty layouts.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: IO module, SpatialCore, OpenSpatialDelay extraction

## REQ-extract-adm-osc-and-trajectory
- source: docs/development-roadmap.md (Phase 1 Deliverables: "Extract real ADM-OSC parsing and trajectory engine"; Phase 1 module table; "What Does NOT Exist Yet")
- description: Extract real ADM-OSC receive (parse `/adm/obj/N/`) and send (30 Hz broadcast) plus the trajectory engine (13 shapes, origin-point architecture, forward/reverse) from OpenSpatialDelay's PluginProcessor.cpp.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: OSC module, Trajectory module, SpatialCore, OpenSpatialDelay extraction

## REQ-extract-ui-rendering
- source: docs/development-roadmap.md (Phase 1 Deliverables: "Extract real UI rendering code"; Phase 1 module table; "What Does NOT Exist Yet")
- description: Extract real UI rendering from OpenSpatialDelay's PluginEditor.h/cpp — SpatialMapComponent, SMLLookAndFeel, ReverseSlider, IndicatorToggle, StyledButton — replacing stub paint methods.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: UI module, SpatialCore, OpenSpatialDelay extraction

## REQ-embed-hrtf-binarydata
- source: docs/development-roadmap.md (Phase 1 Deliverables: "HRTF data embedded as BinaryData (5 SOFA files)"; "What Does NOT Exist Yet": "HRTF SOFA data files (5 profiles, Git LFS)")
- description: Embed the 5 HRTF SOFA profile files as BinaryData, tracked via Git LFS.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: Binaural module, HRTF data, build system, Git LFS

## REQ-smllookandfeel-font-binarydata
- source: docs/development-roadmap.md ("What Does NOT Exist Yet": "Font BinaryData for SMLLookAndFeel")
- description: Provide the font BinaryData required by SMLLookAndFeel.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: UI module, SMLLookAndFeel, build system

## REQ-test-port-and-coverage
- source: docs/development-roadmap.md (Phase 1 Deliverables: "Port existing tests from OpenSpatialDelay + add comprehensive coverage")
- description: Port the existing OpenSpatialDelay test suite into SpatialCore and add comprehensive coverage beyond the current smoke tests.
- acceptance: (absent — source specifies no coverage target or acceptance criteria)
- scope: tests, SpatialCore, OpenSpatialDelay extraction

## REQ-verify-lockfree-realtime-safe
- source: docs/development-roadmap.md (Phase 1 Deliverables: "All code verified lock-free and realtime-safe"; "Critical Design Rules" rule 1)
- description: Verify that all SpatialCore code on the audio path is lock-free and realtime-safe — no allocation, locks, or logging in any function called from processBlock.
- acceptance: (absent — source names no verification method; the governing rule is "NEVER allocate memory in any function called from processBlock")
- scope: all modules, realtime audio path

## REQ-osd-consume-spatialcore-submodule
- source: docs/development-roadmap.md (Phase 1 Deliverables: "OpenSpatialDelay refactored to consume SpatialCore as submodule")
- description: Refactor OpenSpatialDelay to consume SpatialCore as a git submodule instead of holding the framework code inline.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: OpenSpatialDelay repository (external), consumer integration

## REQ-create-org-repo
- source: docs/development-roadmap.md (Phase 2: "Create org repo")
- description: Create `github.com/Spatial-Media-Lab/SpatialCore` as a fresh repository with squashed history.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: repository hosting, public release

## REQ-set-dual-license
- source: docs/development-roadmap.md (Phase 2: "Set license")
- description: Set SpatialCore's license to dual GPL-3.0 + Commercial.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: licensing, public release

## REQ-tag-v1-0-0
- source: docs/development-roadmap.md (Phase 2: "Tag v1.0.0")
- description: Tag v1.0.0 as the first public release, with all modules extracted and tested.
- acceptance: All modules extracted and tested (per the Phase 2 table entry). Governing release rule: run the full test suite before tagging a release.
- scope: release process, public release

## REQ-osd-repoint-submodule-to-org
- source: docs/development-roadmap.md (Phase 2: "Update OpenSpatialDelay — Point submodule to public org repo")
- description: Update OpenSpatialDelay's SpatialCore submodule to point at the public Spatial-Media-Lab org repository.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: OpenSpatialDelay repository (external), release

## REQ-osd-custom-sofa-import
- source: docs/development-roadmap.md ("OpenSpatialDelay v1.0 — Remaining Items": Custom SOFA Import)
- description: Let users load their own HRTF files.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: OpenSpatialDelay v1.0 (external repo)

## REQ-osd-adm-osc-settings-ui
- source: docs/development-roadmap.md ("OpenSpatialDelay v1.0 — Remaining Items": ADM-OSC Settings UI)
- description: In-plugin UI for configuring OSC ports/settings.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: OpenSpatialDelay v1.0 (external repo)

## REQ-osd-aax-format
- source: docs/development-roadmap.md ("OpenSpatialDelay v1.0 — Remaining Items": AAX Format)
- description: Pro Tools plugin format support.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: OpenSpatialDelay v1.0 (external repo)

## REQ-osd-code-signing
- source: docs/development-roadmap.md ("OpenSpatialDelay v1.0 — Remaining Items": Code Signing)
- description: macOS notarization for distribution.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: OpenSpatialDelay v1.0 (external repo)

## REQ-osd-github-migration
- source: docs/development-roadmap.md ("OpenSpatialDelay v1.0 — Remaining Items": GitHub Migration)
- description: Move OpenSpatialDelay to the Spatial-Media-Lab/OpenSpatialDelay org.
- acceptance: (absent — source specifies no acceptance criteria)
- scope: OpenSpatialDelay v1.0 (external repo)

## REQ-plugin-openspatialreverb
- source: docs/development-roadmap.md (Phase 3 plugin table)
- description: OpenSpatialReverb — algorithmic reverb with spatial reflections, built on SpatialCore with ~32% effect-specific DSP. Target 2027.
- acceptance: (absent — source specifies no acceptance criteria beyond the shared pattern: repo with SpatialCore submodule, effect-specific DSP, use SpatialCore rendering/algorithms/UI, build + test + release)
- scope: future plugin suite (new repository)

## REQ-plugin-openspatialgranular
- source: docs/development-roadmap.md (Phase 3 plugin table)
- description: OpenSpatialGranular — granular synthesis with 3D grain positioning, built on SpatialCore. Target 2027.
- acceptance: (absent — source specifies no acceptance criteria beyond the shared Phase 3 pattern)
- scope: future plugin suite (new repository)

## REQ-plugin-openspatialpanner
- source: docs/development-roadmap.md (Phase 3 plugin table)
- description: OpenSpatialPanner — object-based spatial panner with energy distribution, built on SpatialCore. Target 2027.
- acceptance: (absent — source specifies no acceptance criteria beyond the shared Phase 3 pattern)
- scope: future plugin suite (new repository)

## REQ-plugin-openspatialchorus
- source: docs/development-roadmap.md (Phase 3 plugin table)
- description: OpenSpatialChorus — chorus/flanger with spatially distributed voices, built on SpatialCore. Target 2027.
- acceptance: (absent — source specifies no acceptance criteria beyond the shared Phase 3 pattern)
- scope: future plugin suite (new repository)
