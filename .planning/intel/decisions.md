# Decisions

Synthesized: 2026-08-09
Paths are relative to repo root `/Users/andrewrahman/conductor/workspaces/SpatialCore/kelowna/`.

No ADRs exist in the ingest set. The ADR precedence tier is empty. Entries below are decisions
stated declaratively by the SPEC (highest-precedence source present) and by the PRD's
"Critical Design Rules" section, which the PRD explicitly labels as locked.

- `status: locked` is used only where the source itself declares the rule locked/frozen.
- `status: proposed` is used for declarative architecture statements that carry no lock language.

---

## DR-1: Never allocate memory in any function called from processBlock
- source: docs/development-roadmap.md ("Critical Design Rules", rule 1); restated in CLAUDE.md
- status: locked
- decision: NEVER allocate memory in any function called from processBlock.
- scope: realtime audio path, all SpatialCore modules

## DR-2: Never modify the SpatializationAlgorithm interface without a major version bump
- source: docs/development-roadmap.md ("Critical Design Rules", rule 2); restated in CLAUDE.md
- status: locked
- decision: NEVER modify the SpatializationAlgorithm interface without bumping the major version.
- scope: public API, versioning, Algorithms module

## DR-3: Maintain backward compatibility with existing plugins when adding features
- source: docs/development-roadmap.md ("Critical Design Rules", rule 3); restated in CLAUDE.md
- status: locked
- decision: ALWAYS maintain backward compatibility with existing plugins when adding features.
- scope: public API, consumer plugins

## DR-4: Run the full test suite before tagging a release
- source: docs/development-roadmap.md ("Critical Design Rules", rule 4); restated in CLAUDE.md
- status: locked
- decision: ALWAYS run the full test suite before tagging a release.
- scope: release process

## DR-5: HRTF profiles are embedded as BinaryData; changing them forces consumer rebuilds
- source: docs/development-roadmap.md ("Critical Design Rules", rule 5); restated in CLAUDE.md
- status: locked
- decision: HRTF profiles are embedded as BinaryData — adding/removing profiles requires rebuild of ALL consumer plugins.
- scope: Binaural module, HRTF data distribution, consumer plugins

## DR-6: Semantic versioning
- source: docs/development-roadmap.md ("Critical Design Rules", rule 6); restated in CLAUDE.md
- status: locked
- decision: Semantic versioning — Major (breaking API), Minor (new features), Patch (bug fixes).
- scope: release process, versioning

## DR-7: SpatializationAlgorithm interface is frozen
- source: docs/development-roadmap.md ("Key Interface (Frozen — Major Version Bump Required to Change)"); identical interface declared in docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 3, Step 1)
- status: locked
- decision: The SpatializationAlgorithm abstract base is frozen — computeGains(const SourcePosition&, const LayoutContext&, float* outputGains, int numSpeakers) const = 0; supportsBinauralDirect(); supportsSurround(); supportsSHDomain(); getName(). Changing it requires a major version bump.
- scope: public API, Algorithms module

## DR-8: SpatialCore is a static library composed of 7 modules with a fixed source layout
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md ("Architecture")
- status: proposed
- decision: SpatialCore is a static library with 7 modules (Algorithms, Binaural, IO, OSC, Trajectory, DSP, UI). All public headers live under `include/SpatialCore/`. Implementations live under `src/`. Tests live under `tests/`. It compiles as a JUCE module-style static library that consumer plugins link via `add_subdirectory()`.
- scope: repository layout, build system, module boundaries

## DR-9: Consumers integrate via git submodule + CMake subdirectory
- source: docs/development-roadmap.md ("Consumer Integration Model"); docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md ("Architecture"); docs/integration-guide.md (Steps 2-3); CLAUDE.md ("Consumer Plugins")
- status: proposed
- decision: Plugins add SpatialCore as a git submodule, then `add_subdirectory(SpatialCore)` and `target_link_libraries(MyPlugin PRIVATE SpatialCore)`.
- scope: consumer integration, build system

## DR-10: Tech stack is JUCE 8 / C++17 / CMake 3.22+ with libmysofa, zlib, Catch2
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md ("Tech Stack"); restated in CLAUDE.md ("Build System")
- status: proposed
- decision: JUCE 8, C++17, CMake 3.22+, libmysofa v1.3.2 (FetchContent), zlib (system), Catch2 v3.7.1 (FetchContent).
- scope: build system, dependencies

## DR-11: All library code lives in the `spatialcore` namespace
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (all header snippets, Tasks 1-8)
- status: proposed
- decision: All public types and functions are declared inside `namespace spatialcore` (DSP utilities nested in `spatialcore::DSP`).
- scope: public API, naming

## DR-12: Dual license GPL-3.0 + commercial
- source: docs/development-roadmap.md (Phase 2, "Set license"); restated in CLAUDE.md ("License")
- status: proposed
- decision: SpatialCore is dual-licensed GPL-3.0 + Commercial.
- scope: licensing, public release

## DR-13: Publish under the Spatial-Media-Lab organization with fresh squashed history
- source: docs/development-roadmap.md (Phase 2, "Create org repo")
- status: proposed
- decision: Create `github.com/Spatial-Media-Lab/SpatialCore` as a fresh repo with squashed history and publish there.
- scope: repository hosting, public release

## DR-14: SpatialCore extraction is sequenced after OpenSpatialDelay v1.0 ships
- source: docs/development-roadmap.md ("Sequencing")
- status: proposed
- decision: Order is (1) finish OpenSpatialDelay v1.0 (deadline 2026-03-31), (2) extract SpatialCore from OpenSpatialDelay, (3) build future plugins on SpatialCore.
- scope: program sequencing, OpenSpatialDelay, SpatialCore

## DR-15: SpatialCore ships no BinaryData of its own; consumers supply HRTF and font data
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 4, Step 1: "HRTFDatabase stubs must NOT reference BinaryData ... The stub `loadFromMemory()` accepts raw data pointers that consumers provide")
- status: proposed
- decision: HRTFDatabase must not reference BinaryData; it accepts raw data pointers supplied by the consumer via `loadFromMemory()`.
- scope: Binaural module, data distribution
- note: This decision is in tension with locked decision DR-5 (profiles embedded as BinaryData). See INGEST-CONFLICTS.md.

## DR-16: UI components are decoupled from any concrete plugin processor
- source: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 8, Step 1: "decoupled from OpenSpatialDelayProcessor (SpatialMapComponent uses an abstract Listener interface instead of processor pointer)")
- status: proposed
- decision: UI components take an abstract Listener interface rather than a processor pointer, so no UI component depends on a specific plugin type.
- scope: UI module, public API

## DR-17: Every SML plugin is ~68% SpatialCore framework + ~32% plugin-specific DSP
- source: docs/development-roadmap.md ("Architecture Pattern", Phase 1 goal, Phase 3); docs/integration-guide.md ("Architecture Pattern")
- status: proposed
- decision: Plugins layer their effect-specific DSP (~32%) on top of SpatialCore's reusable framework (~68%): algorithms, HRTF, I/O, OSC, trajectories, UI.
- scope: product architecture, all SML plugins
