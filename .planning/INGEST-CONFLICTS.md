## Conflict Detection Report

Generated: 2026-08-09 — /gsd-ingest-docs synthesis (MODE: new)
Ingest set: 1 SPEC, 1 PRD, 3 DOC, 0 ADR. Precedence applied: ADR > SPEC > PRD > DOC (ADR tier empty).
Paths are relative to repo root `/Users/andrewrahman/conductor/workspaces/SpatialCore/kelowna/`.

### BLOCKERS (0)

None. No locked-vs-locked contradictions, no cross-ref cycles, and no UNKNOWN/low-confidence
classifications in the ingest set.

### WARNINGS (11)

[WARNING] REQ-extract-algorithms is listed as open but the codebase shows it implemented
  Found: docs/development-roadmap.md leaves "Extract real algorithm implementations from OpenSpatialDelay" unchecked and states "Real algorithm implementations (all `computeGains()` return zeroes)" under "What Does NOT Exist Yet"
  Impact: .planning/codebase/ARCHITECTURE.md and STRUCTURE.md record 8 implemented algorithms (VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural, ConstantPower) in src/Algorithms/ — routing this as open work would re-plan finished work
  → Confirm the algorithm extraction is done and close REQ-extract-algorithms, or state what remains unextracted

[WARNING] REQ-extract-binaural-rendering is listed as open but the codebase shows it implemented
  Found: docs/development-roadmap.md leaves "Extract real HRTF/binaural rendering from OpenSpatialDelay" unchecked and states "Real HRTF/SOFA loading and convolution (stubs only)"
  Impact: .planning/codebase/ARCHITECTURE.md records working HRTFDatabase (libmysofa, KD-tree, ITD extraction), PartitionedConvolver (dual-slot overlap-save with crossfade) and BinauralRenderer (12 convolvers, per-profile normGain), and CONCERNS.md cites version history through v1.0.11 for these files
  → Confirm the binaural extraction is done and close REQ-extract-binaural-rendering, or state what remains

[WARNING] REQ-extract-speaker-layouts-and-format-registry is listed as open but the codebase shows it implemented
  Found: docs/development-roadmap.md leaves "Extract real speaker layout data and format registry" unchecked and states "Real speaker layout data (factory functions return empty layouts)"
  Impact: .planning/codebase/ARCHITECTURE.md records OutputFormatRegistry with 22 populated formats, SpeakerLayout with 13 ITU-R layouts plus VBAP triplet generation, and a working AmbisonicsCodec
  → Confirm the I/O extraction is done and close REQ-extract-speaker-layouts-and-format-registry, or state what remains

[WARNING] REQ-extract-adm-osc-and-trajectory is listed as open but the codebase shows it implemented
  Found: docs/development-roadmap.md leaves "Extract real ADM-OSC parsing and trajectory engine" unchecked and states "Real ADM-OSC parsing and trajectory animation" does not exist
  Impact: .planning/codebase/ARCHITECTURE.md records ADMOSCReceiver parsing `/adm/obj/N/azim|elev|dist|aed|xyz` with ITU-R BS.2127-0 Cartesian-to-polar conversion, ADMOSCSender broadcasting at 30 Hz, and a TrajectoryEngine with 13 shapes and tick()/random-noise state
  → Confirm the OSC/Trajectory extraction is done and close REQ-extract-adm-osc-and-trajectory, or state what remains

[WARNING] REQ-extract-ui-rendering is listed as open but the codebase shows it implemented
  Found: docs/development-roadmap.md leaves "Extract real UI rendering code" unchecked and states "Real UI rendering (paint methods are stubs)"
  Impact: .planning/codebase/CONCERNS.md counts 1,150+ lines of UI implementation and ARCHITECTURE.md describes working SpatialMapComponent behaviour (object dragging, distance rings, elevation-as-opacity) and the SMLLookAndFeel palette
  → Confirm the UI extraction is done and close REQ-extract-ui-rendering, or state what remains

[WARNING] REQ-test-port-and-coverage is only partially satisfied and its target is undefined
  Found: docs/development-roadmap.md leaves "Port existing tests from OpenSpatialDelay + add comprehensive coverage" unchecked while its "What Exists" section claims "22 smoke tests (52 assertions), all passing"
  Impact: .planning/codebase/CONCERNS.md measures 209 lines of test code against 4,084 lines of source (~5% coverage) with binaural rendering, HRTF database, convolver, OSC, Ambisonics, format registry and UI untested, and recommends a 50% target — the PRD sets no target, so the requirement cannot be sized
  → Set an explicit coverage target and priority list for REQ-test-port-and-coverage before routing

[WARNING] REQ-verify-lockfree-realtime-safe conflicts with documented violations of locked rule DR-1
  Found: docs/development-roadmap.md declares "NEVER allocate memory in any function called from processBlock" a locked rule and leaves "All code verified lock-free and realtime-safe" unchecked
  Impact: .planning/codebase/CONCERNS.md documents live violations — BinauralRenderer::renderSourceBuffers() resizes buffers on the audio thread in release builds (jassertfalse is a no-op), TrajectoryEngine's finalAz_/finalEl_/finalDist_ are unsynchronised across timer and audio threads, HRTFDatabase has no lock around easyHandle, and ADMOSCReceiver does not bounds-check object index against MAX_SOURCES
  → Decide whether these four fixes are in scope for REQ-verify-lockfree-realtime-safe or split into their own remediation items

[WARNING] Conflicting accounts of HRTF SOFA data ownership and count
  Found: docs/development-roadmap.md declares as locked "HRTF profiles are embedded as BinaryData — adding/removing profiles requires rebuild of ALL consumer plugins" and separately lists "HRTF SOFA data files (5 profiles, Git LFS)" as not yet existing
  Found: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 4) requires the opposite ownership — "HRTFDatabase stubs must NOT reference BinaryData ... `loadFromMemory()` accepts raw data pointers that consumers provide"
  Found: CLAUDE.md asserts "5 SOFA files embedded as BinaryData (Git LFS tracked)" as current fact, while .planning/codebase/ARCHITECTURE.md says 6 embedded profiles are supported; no `.sofa` file and no BinaryData target exists anywhere in this working tree, and include/SpatialCore/Binaural/HRTFDatabase.h documents loadFromMemory() as taking consumer-provided memory
  Impact: Whether SpatialCore ships HRTF data or delegates it to consumers is unresolved, and the profile count is stated as 5 in two places and 6 in another — REQ-embed-hrtf-binarydata cannot be scoped until this is settled
  → Decide data ownership (SpatialCore BinaryData vs consumer-supplied) and fix the profile count, then reconcile the locked rule DR-5 with SPEC decision DR-15

[WARNING] Conflicting accounts of the canonical SpatialCore repository URL
  Found: CLAUDE.md states GitHub is `https://github.com/Spatial-Media-Lab/SpatialCore` and docs/integration-guide.md instructs consumers to `git submodule add https://github.com/Spatial-Media-Lab/SpatialCore.git`
  Found: docs/development-roadmap.md lists the current remote as `github.com/AndrewRahman/SpatialCore` and puts "Create org repo `github.com/Spatial-Media-Lab/SpatialCore` (fresh, squash history)" in Phase 2 as unfinished; docs/conductor-setup-guide.md also records `github.com/AndrewRahman/SpatialCore` (private); this checkout's `git remote -v` is `https://github.com/AndrewRahman/SpatialCore.git`
  Impact: The integration guide currently publishes a submodule URL that does not exist, and REQ-create-org-repo / REQ-osd-repoint-submodule-to-org read as done when they are not
  → Confirm the migration is still pending, and either mark the integration-guide URL as forward-looking or complete REQ-create-org-repo

[WARNING] Conflicting algorithm counts across sources
  Found: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md defines the base class plus 7 implementations (VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural), docs/development-roadmap.md says "all 7 SpatializationAlgorithm implementations", and docs/integration-guide.md says "7 spatialization algorithms" while its example declares `spatialcore::SpatializationAlgorithm* algorithms[6];`
  Found: CLAUDE.md and .planning/codebase/STRUCTURE.md both list 8 algorithms — the seven above plus ConstantPowerAlgorithm (include/SpatialCore/Algorithms/ConstantPowerAlgorithm.h, src/Algorithms/ConstantPowerAlgorithm.cpp)
  Impact: The advertised public algorithm surface is stated three different ways (6, 7, 8), which matters because the algorithm interface is a frozen, major-version-gated contract
  → Fix the count to 8 across the SPEC, roadmap and integration guide, or state explicitly that ConstantPower is internal and not part of the public set

[WARNING] outputLimiter transfer function differs between SPEC and implementation
  Found: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md (Task 7) specifies a hard clamp — `const float ceiling = 1.2589f; if (x > ceiling) return ceiling; if (x < -ceiling) return -ceiling; return x;`
  Found: include/SpatialCore/DSP/Utilities.h implements a soft ceiling — `const float threshold = 1.2589f; return threshold * std::tanh(x / threshold);` — and .planning/codebase/ARCHITECTURE.md describes it as "tanh-based +2dB ceiling"
  Impact: Same ceiling constant, different audible behaviour on a public inline API that consumer plugins already call; the SPEC is the highest-precedence document present, so the implementation currently contradicts the governing contract
  → Confirm the tanh soft ceiling is intentional and update the SPEC, or restore the hard clamp

### INFO (11)

[INFO] No ADRs in the ingest set — SPEC is the top precedence tier
  Note: Classification counts are 1 SPEC (docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md), 1 PRD (docs/development-roadmap.md), 3 DOC (docs/integration-guide.md, docs/conductor-setup-guide.md, docs/workflow-tutorials.md). With the ADR tier empty, the SPEC wins all precedence contests. No classification carried a per-doc `precedence` override and none is `locked: true`.

[INFO] Cross-reference graph is acyclic
  Note: DFS three-colour cycle detection over `cross_refs` found no cycles. Classified-doc edges are SPEC → PRD → {integration-guide, conductor-setup-guide, workflow-tutorials}; the DOC nodes are sinks. Maximum traversal depth reached was 2, far below the 50 cap. Remaining cross_refs point at non-classified artifacts (CLAUDE.md, README.md, CMakeLists.txt, tests/CMakeLists.txt, external conductor.build URLs) and were not traversed.

[INFO] SPEC recorded as constraints/decisions rather than requirements
  Note: All 12 tasks in docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md are unchecked (`- [ ]`), but .planning/codebase/ shows the scaffolding complete — CMakeLists.txt, the full include/SpatialCore/ tree, src/ implementations and tests/ with Catch2 v3.7.1 all exist. Per ingest guidance the SPEC was mined for architecture decisions and binding contracts (see intel/constraints.md and intel/decisions.md) and produced zero open requirements.

[INFO] Locked design rules originate from the PRD, not from an ADR
  Note: docs/development-roadmap.md "Critical Design Rules" says "These rules are locked in and apply to all SpatialCore development", so DR-1 through DR-6 in intel/decisions.md carry `status: locked` on the strength of that statement plus their restatement in CLAUDE.md. DR-7 is locked by the PRD heading "Key Interface (Frozen — Major Version Bump Required to Change)". No ADR file backs any of them.

[INFO] docs/conductor-setup-guide.md contributes no requirements
  Note: Classified DOC (high confidence). Its scope is developer tooling — installing Conductor, workspace/branch/PR mechanics, setup and run scripts — not the SpatialCore library. Captured as context only in intel/context.md.

[INFO] docs/workflow-tutorials.md contributes no requirements
  Note: Classified DOC (high confidence). Describes three Claude Code working patterns (Conductor, Worktree, GitTree) for multi-plugin development. No statement about the SpatialCore library itself. Captured as context only in intel/context.md.

[INFO] docs/integration-guide.md contributes no requirements
  Note: Classified DOC (high confidence). Consumer-facing usage documentation restating contracts already held by the SPEC and PRD (submodule + add_subdirectory, umbrella header, UI components, Catch2 testing). Captured as context in intel/context.md; its one divergence — `algorithms[6]` — is raised under WARNINGS.

[INFO] Auto-resolved: SPEC > PRD on module decomposition
  Note: docs/superpowers/plans/2026-03-22-scaffold-spatialcore.md defines 7 modules (Algorithms, Binaural, IO, OSC, Trajectory, DSP, UI) with Core/ as shared types alongside them; docs/development-roadmap.md says "full public API headers for all 7 modules" and then lists 8 entries including Core/. SPEC wins on precedence — DR-8 records 7 modules plus Core.

[INFO] Codebase implements a superset of the SPEC file structure
  Note: Beyond the SPEC's declared tree, the working copy adds include/SpatialCore/Core/SpatialMath.h + src/Core/SpatialMath.cpp, include/SpatialCore/Algorithms/ConstantPowerAlgorithm.h + .cpp, include/SpatialCore/Algorithms/AllAlgorithms.h and include/SpatialCore/Binaural/SharedFFTCache.h; src/DSP/ is absent because DSP/Utilities.h is header-only as the SPEC intended. Additive only — no SPEC-declared header is missing, so no contradiction was raised.

[INFO] TrajectoryEngine provides the SPEC API plus an instance API
  Note: include/SpatialCore/Trajectory/TrajectoryEngine.h retains the SPEC's static `compute(TrajectoryShape, float phase, float baseAzDeg, float baseElDeg, float baseDist, bool reverse)`, `getNumShapes()` and `getShapeName()`, and adds `struct ObjectInput`, `kMaxObjects = 12` and instance `tick(int objectIndex, const ObjectInput&, float dt)`. Superset, not a contradiction.

[INFO] Five requirements are scoped to repositories outside this checkout
  Note: REQ-osd-custom-sofa-import, REQ-osd-adm-osc-settings-ui, REQ-osd-aax-format, REQ-osd-code-signing and REQ-osd-github-migration come from the PRD's "OpenSpatialDelay v1.0 — Remaining Items" table and target the OpenSpatialDelay repository, not SpatialCore. REQ-osd-consume-spatialcore-submodule and REQ-osd-repoint-submodule-to-org are likewise external. The PRD's stated OpenSpatialDelay v1.0 deadline of 2026-03-31 has passed relative to the synthesis date of 2026-08-09.
