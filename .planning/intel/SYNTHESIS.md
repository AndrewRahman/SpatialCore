# Synthesis

Generated: 2026-08-09 — /gsd-ingest-docs (MODE: new)
Repo root: `/Users/andrewrahman/conductor/workspaces/SpatialCore/kelowna/`
Entry point for `gsd-roadmapper`. Read the per-type intel files below for detail.

---

## Docs synthesized (5)

- SPEC (1): `docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md` — confidence medium
- PRD (1): `docs/development-roadmap.md` — confidence medium
- DOC (3): `docs/integration-guide.md`, `docs/conductor-setup-guide.md`, `docs/workflow-tutorials.md` — confidence high
- ADR (0): none. The ADR precedence tier is empty; the SPEC is the highest-precedence source.

Cycle detection: run over the `cross_refs` graph, no cycles, max depth 2 (cap 50).
UNKNOWN / low-confidence classifications: none.

## Decisions (17) — `intel/decisions.md`

- Locked (7): DR-1 no allocation in processBlock; DR-2 no interface change without major bump; DR-3 backward compatibility; DR-4 full test suite before tagging; DR-5 HRTF profiles embedded as BinaryData; DR-6 semantic versioning; DR-7 SpatializationAlgorithm interface frozen.
  Source of all seven: `docs/development-roadmap.md` ("Critical Design Rules" / "Key Interface (Frozen)"), restated in `CLAUDE.md`. No ADR file backs them.
- Proposed (10): DR-8 static library, 7 modules, fixed source layout; DR-9 submodule + CMake subdirectory integration; DR-10 JUCE 8 / C++17 / CMake 3.22+ / libmysofa 1.3.2 / zlib / Catch2 3.7.1; DR-11 `spatialcore` namespace; DR-12 dual GPL-3.0 + commercial license; DR-13 publish under Spatial-Media-Lab with squashed history; DR-14 extraction sequenced after OpenSpatialDelay v1.0; DR-15 no BinaryData inside SpatialCore, consumers supply HRTF data; DR-16 UI decoupled from any concrete processor; DR-17 68% framework / 32% plugin DSP split.
- DR-5 (locked) and DR-15 (SPEC) disagree on HRTF data ownership — raised as a WARNING.

## Requirements (23) — `intel/requirements.md`

All from the PRD; the SPEC produced none (its work is already done and was recorded as constraints).

- SpatialCore extraction, Phase 1 (5): REQ-extract-algorithms, REQ-extract-binaural-rendering, REQ-extract-speaker-layouts-and-format-registry, REQ-extract-adm-osc-and-trajectory, REQ-extract-ui-rendering — all five are contradicted by the codebase map, see conflicts.
- SpatialCore data and quality (4): REQ-embed-hrtf-binarydata, REQ-smllookandfeel-font-binarydata, REQ-test-port-and-coverage, REQ-verify-lockfree-realtime-safe.
- Release / migration (4): REQ-create-org-repo, REQ-set-dual-license, REQ-tag-v1-0-0, REQ-osd-repoint-submodule-to-org.
- OpenSpatialDelay, external repo (6): REQ-osd-consume-spatialcore-submodule, REQ-osd-custom-sofa-import, REQ-osd-adm-osc-settings-ui, REQ-osd-aax-format, REQ-osd-code-signing, REQ-osd-github-migration.
- Future plugin suite, 2027 (4): REQ-plugin-openspatialreverb, REQ-plugin-openspatialgranular, REQ-plugin-openspatialpanner, REQ-plugin-openspatialchorus.

Twenty of the 23 carry no acceptance criteria in the source; those fields are marked absent, not inferred.

## Constraints (19) — `intel/constraints.md`

Type breakdown: api-contract 13, schema 3, nfr 3.

- api-contract: header layout / module boundaries, root CMake build contract, OutputFormatRegistry API, AmbisonicsCodec API, SpatializationAlgorithm interface + context structs, Binaural module responsibilities, TrajectoryEngine API, DSP utilities behaviour, UI decoupling, umbrella header, test target contract (13 total including sub-contracts listed in the file).
- schema: core types and constants (MAX_SOURCES 12, MAX_SPEAKERS 16, SourcePosition, BinauralGains), SpeakerLayout / VBAPTriplet / ITU-R factories, OutputFormat enum (22) + OutputFormatInfo.
- nfr: platform and optimization flags, build verification gate, realtime safety of the audio path.

## Context topics (5) — `intel/context.md`

Consumer plugin integration workflow; program narrative and module extraction map; Phase 3 plugin delivery pattern; Conductor setup (developer tooling); multi-plugin workflow patterns (developer tooling).

## Conflicts — `.planning/INGEST-CONFLICTS.md`

- 0 blockers
- 11 competing variants / warnings — five PRD extraction requirements the codebase map shows as already implemented; test-coverage requirement partially satisfied with no target; lock-free verification contradicted by four documented realtime-safety violations; three-way conflict on HRTF SOFA data ownership and profile count; conflicting canonical repository URL; algorithm count stated as 6, 7 and 8 across sources; outputLimiter hard clamp (SPEC) vs tanh soft ceiling (implementation)
- 11 auto-resolved / informational — empty ADR tier, acyclic ref graph, SPEC recorded as constraints not requirements, locked rules sourced from the PRD, three DOCs contributing no requirements, SPEC > PRD on module decomposition, codebase superset of the SPEC tree, TrajectoryEngine superset API, seven requirements scoped to external repositories

Every warning requires user resolution before routing; nothing gates on a blocker.

## Files

- `.planning/intel/decisions.md`
- `.planning/intel/requirements.md`
- `.planning/intel/constraints.md`
- `.planning/intel/context.md`
- `.planning/INGEST-CONFLICTS.md`
- Ground truth for current state (not produced by this ingest): `.planning/codebase/ARCHITECTURE.md`, `STACK.md`, `STRUCTURE.md`, `CONVENTIONS.md`, `INTEGRATIONS.md`, `TESTING.md`, `CONCERNS.md`
