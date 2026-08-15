# SpatialCore

## What This Is

SpatialCore is the shared spatial audio rendering engine for the Spatial Media Library, built by
Spatial Media Lab (spatialmedialab.org). It is a JUCE 9.0.0 / C++17 static library that takes audio
objects carrying 3D positions and renders them to any output format — binaural, stereo, surround,
or Ambisonics — through a set of spatialization algorithms, with HRTF convolution, ADM-OSC
control, trajectory animation, and shared UI widgets included.

Its users are plugin authors inside Spatial Media Lab. They add SpatialCore as a git submodule,
`add_subdirectory()` it, and write only their own effect DSP — roughly 32% of a plugin — on top of
the ~68% that SpatialCore supplies.

## Core Value

A Spatial Media Lab plugin author gets production-grade spatial rendering by linking one library,
so the only audio code they write is their own effect.

## Business Context

- **Customer**: Spatial Media Lab plugin authors first; third-party plugin developers after public release.
- **Revenue model**: Dual license — GPL-3.0 for open use, commercial license for closed-source plugins.
- **Success metric**: Number of shipping plugins built on SpatialCore (target for v2: two).

## Milestones

Only **v1** is decomposed into roadmap phases. v2 and v3 are recorded here and will be
decomposed when their milestone opens.

| Milestone | Success metric | Status |
|-----------|----------------|--------|
| **v1** | OpenSpatialDelay ships on SpatialCore as a submodule with zero regressions | 🚧 Active — see ROADMAP.md |
| **v2** | A second plugin is built on SpatialCore from scratch, proving the API generalizes | 📋 Future |
| **v3** | Public release under the Spatial-Media-Lab org with docs and a tagged v1.0 | 📋 Future |

## Requirements

### Validated

None yet. The working tree contains substantial implementation, but the user has ruled it
**partial until audited** (see Context). Nothing moves to Validated until a phase verifies it.

### Active

Milestone v1 scope. Full detail and acceptance in `.planning/REQUIREMENTS.md`.

**Public contract resolution**
- [ ] **API-01** — Public algorithm count fixed at one number and made consistent across headers, SPEC, PRD, and integration guide
- [ ] **API-02** — `outputLimiter()` transfer function decided (hard clamp vs tanh) and spec/implementation aligned
- [ ] **API-03** — HRTF profile count fixed at one number across docs, headers, and embedded data
- [ ] **AUDIT-01** — Every module carries a complete/partial/stub verdict backed by an executed probe

**Extraction completion**
- [ ] **EXTR-01** — Algorithms module complete and verified
- [ ] **EXTR-02** — Binaural rendering (HRTFDatabase, PartitionedConvolver, BinauralRenderer) complete and verified
- [ ] **EXTR-03** — Speaker layouts, output format registry, and Ambisonics codec complete and verified
- [ ] **EXTR-04** — ADM-OSC receive/send and trajectory engine complete and verified
- [ ] **EXTR-05** — UI rendering (SpatialMapComponent, SMLLookAndFeel, widgets) complete and verified

**Embedded data**
- [ ] **DATA-01** — HRTF SOFA profiles embedded as BinaryData, Git LFS tracked
- [ ] **DATA-02** — Font BinaryData provided for SMLLookAndFeel

**Realtime safety**
- [ ] **RTSF-01** — `BinauralRenderer::renderSourceBuffers()` never allocates on the audio thread in release builds
- [ ] **RTSF-02** — TrajectoryEngine final positions synchronised across timer and audio threads
- [ ] **RTSF-03** — `HRTFDatabase` access to `easyHandle` made thread-safe
- [ ] **RTSF-04** — `ADMOSCReceiver` bounds-checks object index against `MAX_SOURCES`
- [ ] **RTSF-05** — Whole audio path verified lock-free and realtime-safe

**Verification and consumability**
- [ ] **TEST-01** — OpenSpatialDelay tests ported and coverage raised to an agreed target
- [ ] **INTG-01** — A consumer plugin can submodule, link, and build against SpatialCore using only the umbrella header

### Out of Scope

- **Release and org migration (REQ-create-org-repo, REQ-set-dual-license, REQ-tag-v1-0-0)** — belongs to milestone v3, not v1. v1 succeeds when OpenSpatialDelay ships on SpatialCore from the current private remote.
- **OpenSpatialDelay repository work** — seven requirements live in that repo. Tracked below as external dependencies, deliberately given no SpatialCore phase.
- **Future plugin suite (OpenSpatialReverb, OpenSpatialGranular, OpenSpatialPanner, OpenSpatialChorus)** — 2027 targets. One of them becomes the v2 milestone.
- **Windows and Linux CI verification** — cross-platform support is an intent and the build flags exist, but v1 verification is macOS-only. Adding CI matrices before a second consumer exists is premature.
- **Custom / vendor-specific speaker layouts** — only the 14 built-in layouts. Generalizing the layout API is a post-v1 API addition.
- **Replacing the JUCE FFT dependency** — noted as a medium risk in CONCERNS.md; swapping FFT libraries is disproportionate to v1.

## External Dependencies

The v1 milestone is defined by OpenSpatialDelay shipping on SpatialCore, so v1 cannot close
without work in the OpenSpatialDelay repository. These are **not** SpatialCore phases and must not
be planned here.

| ID | Work (external repo) | Gates v1? |
|----|----------------------|-----------|
| REQ-osd-consume-spatialcore-submodule | Refactor OpenSpatialDelay to consume SpatialCore as a submodule instead of inline framework code | Yes |
| REQ-osd-custom-sofa-import | Let users load their own HRTF files | No — OSD v1.0 backlog |
| REQ-osd-adm-osc-settings-ui | In-plugin UI for OSC port configuration | No — OSD v1.0 backlog |
| REQ-osd-aax-format | Pro Tools AAX format support | No — OSD v1.0 backlog |
| REQ-osd-code-signing | macOS notarization for distribution | No — OSD v1.0 backlog |
| REQ-osd-github-migration | Move OpenSpatialDelay to the Spatial-Media-Lab org | No — v3 |
| REQ-osd-repoint-submodule-to-org | Point the OSD submodule at the public org repo | No — v3 |

## Context

**Origin.** SpatialCore was extracted from OpenSpatialDelay v1.0, where ~68% of the codebase was
marked as reusable spatial audio infrastructure. Extraction separated framework code (algorithms,
HRTF, I/O, OSC, trajectories, UI) from delay-specific code (delay line, pitch shift, feedback,
wobble).

**The tree is partial until proven otherwise — this is a user ruling.**
`.planning/codebase/ARCHITECTURE.md` and `STRUCTURE.md` report all five extraction requirements as
already implemented: 8 algorithms in `src/Algorithms/`, a working `HRTFDatabase` with libmysofa,
KD-tree and ITD extraction, a dual-slot `PartitionedConvolver`, 22 populated output formats, 13
ITU-R layouts, ADM-OSC receive/send, a 13-shape trajectory engine, and 1,150+ lines of UI. The
user has explicitly overridden that reading: **existing `src/` code is treated as partial or stub
work to be completed and verified, not as finished work.**

The practical consequence: **every extraction phase begins by auditing what actually exists before
writing any code.** The audit must be evidence-based — compile and run behavioral probes against
the module rather than reading the source and declaring it done. The codebase map is
current-state ground truth about *what files exist*, not a completion certificate.

**Confirmed gaps in the tree.** No `.sofa` file and no BinaryData target exist anywhere in the
working copy, so embedding the HRTF profiles is genuinely unstarted despite CLAUDE.md asserting it
as fact. The git remote is still `github.com/AndrewRahman/SpatialCore.git` while
`docs/integration-guide.md` already publishes a `Spatial-Media-Lab` submodule URL that does not
resolve.

**Known realtime-safety violations.** `.planning/codebase/CONCERNS.md` documents four live
breaches of locked rule DR-1. These are named requirements (RTSF-01 through RTSF-05) with their
own phase, not assumptions folded into a general verification sweep. *(Corrected 2026-08-10:
RTSF-04 was closed as a non-finding — `ADMOSCReceiver.cpp:43` already bounds-checks correctly —
while RTSF-01 widened to two allocation sites and a new unfiled TrajectoryEngine race was found.)*

**Test coverage.** *(Corrected 2026-08-10 — the previous figure was measured against the wrong
branch.)* The suite builds and passes: **144 TEST_CASE blocks, 1585 assertions across 16 test
files**, covering Core, Algorithms, IO, OSC, Trajectory, Binaural (against all 5 real SOFA
profiles), and Engine. No coverage tooling is configured anywhere in the build, so no percentage
can be measured — the earlier "~5%" was never a real measurement. Modules with no dedicated tests:
`src/Binaural/PartitionedConvolver.cpp`, `src/Binaural/BinauralRenderer.cpp`, and all of
`src/UI/*.cpp` (the last by design — UI lives in the separate `SpatialCoreUI` target).

## Constraints

- **Tech stack**: JUCE 9.0.0, C++17, CMake 3.22+, libmysofa v1.3.2 (FetchContent), zlib (system), Catch2 v3.7.1 (FetchContent) — fixed by the SPEC's root CMake contract.
- **Distribution**: Static library consumed via git submodule + `add_subdirectory(SpatialCore)` + `target_link_libraries(... PRIVATE SpatialCore)`. Consumers include only `<SpatialCore/SpatialCore.h>`.
- **Platform**: macOS is primary — arm64, deployment target 12.0, `-ffast-math`. Windows (MSVC, `/fp:fast`) and Linux are intended but not verified in v1.
- **Realtime**: No malloc, locks, or logging in any function reachable from `processBlock`. Algorithms are stateless; all computation state lives in context structs. Layout changes use dual-buffered atomic swap.
- **Frozen API**: The `SpatializationAlgorithm` interface cannot change without a major version bump. Same for anything else consumers link against.
- **Scale limits**: `MAX_SOURCES = 12`, `MAX_SPEAKERS = 16`, `MAX_AMBI_ORDER = 6` (49 channels).
- **Licensing**: GPL-3.0 + commercial dual license.
- **Backward compatibility**: OpenSpatialDelay must keep working across every SpatialCore change.

## Open Questions

Unresolved. Each must be settled by an owning phase before v1 closes. **Do not silently pick a
side during planning** — surface the decision to the user.

| # | Question | Positions | Why it matters | Owning phase |
|---|----------|-----------|----------------|--------------|
| ~~OQ-1~~ | ~~How many spatialization algorithms are public?~~ **RESOLVED 2026-08-11 — 8** | 6 (`integration-guide` code sample `algorithms[6]`) vs 7 (SPEC, PRD) vs 8 (CLAUDE.md and the actual tree, including `ConstantPowerAlgorithm`) | **Closed by evidence, then frozen in code.** The answer is 8. `include/SpatialCore/Algorithms/AllAlgorithms.h` now derives `NUM_ALGORITHMS` from a `AlgorithmTypeList<...>` pack rather than a literal, so a 9th implementation cannot be added without failing the build and forcing every doc surface to be updated with it. | Phase 1 (API-01) — closed |
| ~~OQ-2~~ | ~~What is `outputLimiter()`'s transfer function?~~ **RESOLVED 2026-08-10** | SPEC: hard clamp at 1.2589f. `include/SpatialCore/Core/SpatialMath.h:100-106`: `threshold * std::tanh(x / threshold)` | **Resolved by user decision:** keep the shipped `tanh` soft ceiling — OpenSpatialDelay shipped publicly at v1.0.0 with this curve, and the header docblock at `include/SpatialCore/Core/SpatialMath.h:98-99` is the governing contract. A selectable hard-clamp mode is deferred to v2 as LIMIT-01. | Phase 1 (API-02) — closed |
| ~~OQ-3~~ | ~~How many HRTF profiles ship?~~ **RESOLVED 2026-08-11 — 5** | 5 (CLAUDE.md, PRD) vs 6 (`.planning/codebase/ARCHITECTURE.md`) | **Closed by evidence:** `HRTF/` holds exactly 5 `.sofa` files, the same 5 as OSD's `HRTF/`. `profileIndex` remains part of the public `BinauralContext`; Phase 3 still owns packaging (SUITE-01), but the count itself is settled. | Phase 1 (API-03) — closed; packaging consumed by Phase 3 |
| OQ-4 | What is the test coverage target? | Undefined in all source docs. CONCERNS.md recommends 50% minimum (~2,000 lines of tests) | TEST-01 cannot be sized or called done without a number and a priority order. | Phase 6 (TEST-01) — set the target at phase start |
| ~~OQ-5~~ | ~~When does the org repo migration happen?~~ **RESOLVED — see Key Decisions "Remote topology."** | — | Not an open question: the personal remote is correct by design, gated on proof, not on a date. | — |

## Key Decisions

<decisions>

### Locked

These are declared locked by their source and are binding on all SpatialCore work. Changing any
of them requires explicit user approval, not a planning decision.

| ID | Decision | Source | Scope |
|----|----------|--------|-------|
| DR-1 | NEVER allocate memory in any function called from `processBlock` | PRD "Critical Design Rules" 1; CLAUDE.md | Realtime audio path, all modules |
| DR-2 | NEVER modify the `SpatializationAlgorithm` interface without a major version bump | PRD rule 2; CLAUDE.md | Public API, Algorithms |
| DR-3 | ALWAYS maintain backward compatibility with existing plugins when adding features | PRD rule 3; CLAUDE.md | Public API, consumer plugins |
| DR-4 | ALWAYS run the full test suite before tagging a release | PRD rule 4; CLAUDE.md | Release process |
| DR-5 | HRTF profiles are embedded as BinaryData — adding or removing profiles requires a rebuild of ALL consumer plugins | PRD rule 5; CLAUDE.md | Binaural, data distribution |
| DR-6 | Semantic versioning — major for breaking API, minor for features, patch for fixes | PRD rule 6; CLAUDE.md | Versioning |
| DR-7 | The `SpatializationAlgorithm` abstract base is frozen | PRD "Key Interface (Frozen)"; SPEC Task 3 | Public API, Algorithms |

**DR-5 supersedes DR-15 — locked by user ruling.** SpatialCore owns and embeds the HRTF profiles
as BinaryData, Git LFS tracked. The SPEC's DR-15 ("HRTFDatabase must NOT reference BinaryData;
consumers supply raw pointers via `loadFromMemory()`") is **superseded and closed**.
`loadFromMemory()` may remain as a secondary API for consumer-supplied SOFA files, but embedded
BinaryData is the shipping path. Note that no `.sofa` file and no BinaryData target currently
exists in the tree, so embedding the profiles is real, unstarted work (DATA-01), not a
documentation fix.

### Proposed

Architecture statements carrying no lock language. Treat as the working default; changing one is a
normal planning decision.

| ID | Decision | Outcome |
|----|----------|---------|
| DR-8 | Static library, 7 modules (Algorithms, Binaural, IO, OSC, Trajectory, DSP, UI) plus shared `Core/`, fixed `include/` / `src/` / `tests/` layout | ✓ Good — tree matches, with additive extras (`Core/SpatialMath`, `Algorithms/ConstantPower`, `Algorithms/AllAlgorithms.h`, `Binaural/SharedFFTCache.h`) |
| DR-9 | Consumers integrate via git submodule + CMake `add_subdirectory()` | — Pending — unverified by a real consumer until INTG-01 |
| DR-10 | JUCE 9.0.0 / C++17 / CMake 3.22+ / libmysofa 1.3.2 / zlib / Catch2 3.7.1 | ✓ Good — corrected 2026-08-10; DR-10 originally said JUCE 8, tree is pinned 9.0.0 at `CMakeLists.txt:33` |
| DR-11 | All library code lives in `namespace spatialcore` (DSP utilities in `spatialcore::DSP`) | ✓ Good |
| DR-12 | Dual license GPL-3.0 + commercial | — Pending — LICENSE file lands in v3 |
| DR-13 | Publish under Spatial-Media-Lab with fresh squashed history | — Pending — gated on proof, not on a date. See the "Remote topology" decision row below. |
| DR-14 | Extraction sequenced after OpenSpatialDelay v1.0 ships | ⚠️ Revisit — the PRD's 2026-03-31 OSD deadline has passed and extraction is well underway; the sequencing assumption no longer holds |
| DR-15 | SpatialCore ships no BinaryData; consumers supply HRTF data | ✗ **Superseded by DR-5** — closed by user ruling |
| DR-16 | UI components take an abstract Listener interface, never a concrete processor pointer | ✓ Good |
| DR-17 | Every SML plugin is ~68% SpatialCore framework + ~32% plugin-specific DSP | ✓ Good — the product thesis |
| DR-18 | **Remote topology.** `AndrewRahman/SpatialCore` is the deliberate development remote, not an accident awaiting cleanup. Development stays on the personal remote until the pipeline is proven, for risk containment: `Spatial-Media-Lab/OpenSpatialDelay` is public and in use by real people right now, so migrating it onto an unproven SpatialCore could break a live plugin. Migration is **gated on proof, not on a date** — SpatialCore, OpenSpatialDelay-on-SpatialCore, and OpenSpatialPanner land on the organisation together once the process is proven. `docs/integration-guide.md` carries the working remote as the live instruction and labels the organisation URL as the post-proof destination. | ✓ Decided 2026-08-10 — resolves OQ-5 |

</decisions>

---
*Last updated: 2026-08-09 after ingest of docs + codebase map, with user-supplied conflict rulings*
