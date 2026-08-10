# Codebase Structure

**Analysis Date:** 2026-08-10

**Branch/commit analyzed:** `gsd-remap` @ `8d1868e` (merge of `origin/spatialcore-v2-extraction` + `origin/main`). Verified via `find`/`ls`/`git log` at this commit; `build/`, `build-map/`, and `JUCE/` are generated/vendored and excluded below.

## Directory Layout

```text
kelowna/
├── CMakeLists.txt          # Root build: SpatialCore (DSP) + SpatialCoreUI (GUI) static-lib targets
├── CLAUDE.md                # Project-level agent instructions (partially stale re: Engine/, SpatialCoreUI)
├── README.md
├── .gitattributes           # Git LFS tracking (HRTF SOFA binaries)
├── include/SpatialCore/     # Public headers, mirrors src/ by module
│   ├── SpatialCore.h        # DSP-only umbrella header (Core+Algorithms+Binaural+IO+OSC+Trajectory+Engine; NOT UI)
│   ├── Core/                # Shared types/constants: Types.h, SourcePosition.h, BinauralGains.h, SpatialMath.h
│   ├── Algorithms/           # SpatializationAlgorithm base + 8 concrete implementations + AllAlgorithms.h
│   ├── Binaural/              # SharedFFTCache, HRTFDatabase, PartitionedConvolver, BinauralRenderer
│   ├── Engine/                 # RenderEngine.h — consumer-facing render facade (RenderSources/RenderBlockContext)
│   ├── IO/                      # OutputFormat, OutputFormatRegistry, SpeakerLayout, AmbisonicsCodec
│   ├── OSC/                      # ADMOSCReceiver, ADMOSCSender, OSCPortValidation
│   ├── Trajectory/                 # TrajectoryEngine, DopplerVelocity
│   └── UI/                          # SpatialMapComponent, SMLLookAndFeel, widgets (SpatialCoreUI target only)
├── src/                      # Implementation .cpp files, same submodule breakdown as include/SpatialCore/
│   ├── Algorithms/ Binaural/ Core/ Engine/ IO/ OSC/ Trajectory/ UI/
├── tests/                    # Catch2 tests, mirrors src/ submodule breakdown
│   ├── CMakeLists.txt        # SpatialCoreTests executable target
│   ├── Algorithms/ Binaural/ Core/ Engine/ IO/ OSC/ Trajectory/
├── HRTF/                     # SOFA HRTF profile binaries (Git LFS)
├── fonts/                    # UI font assets
├── docs/                     # Planning/integration docs (some stale — see notes below)
│   └── superpowers/
├── .github/workflows/         # CI
├── .claude/skills/             # 15 JUCE/DSP project-level skills
└── .planning/                   # GSD planning state (PROJECT.md, ROADMAP.md, codebase/ docs, intel/)
```

Note: no top-level `include/SpatialCore/Engine/` or `Engine/` mention exists in `CLAUDE.md`'s "Core Components" table — that table is out of date; the tree has an `Engine/` module in both `include/SpatialCore/` and `src/` (confirmed via `find`).

## Directory Purposes

**`include/SpatialCore/`:**
- Purpose: all public headers, the only thing a consumer project needs on its include path.
- Contains: one subdirectory per module (`Core`, `Algorithms`, `Binaural`, `Engine`, `IO`, `OSC`, `Trajectory`, `UI`), plus the umbrella `SpatialCore.h`.
- Key files: `SpatialCore.h` (DSP-only umbrella — explicitly excludes `UI/`), `Engine/RenderEngine.h` (facade entry point), `Core/Types.h` (`MAX_SOURCES`/`MAX_SPEAKERS`).

**`src/`:**
- Purpose: implementation files, 1:1 module mirror of `include/SpatialCore/` (module names match exactly: `Algorithms`, `Binaural`, `Core`, `Engine`, `IO`, `OSC`, `Trajectory`, `UI`).
- Contains: `.cpp` files. `Core/` has only `SpatialMath.cpp` (most `Core/` types are header-only structs).
- Key files: `src/Engine/RenderEngine.cpp` (facade implementation), `src/Binaural/BinauralRenderer.cpp`.

**`tests/`:**
- Purpose: Catch2-based unit/regression tests, one directory per tested module (`Algorithms`, `Binaural`, `Core`, `Engine`, `IO`, `OSC`, `Trajectory` — no `tests/UI/`).
- Contains: `*Tests.cpp` files; `tests/CMakeLists.txt` builds a single `SpatialCoreTests` executable via `catch_discover_tests`.
- Key files: `tests/Engine/RenderEngineTests.cpp` (facade behavior/regression tests), `tests/Binaural/*Tests.cpp` (5 HRTF-profile-specific test files: Bernschuetz KU100, Cipic Subject003, HUTUBS PP2, MIT KEMAR Large Pinna, SADIE D2 KU100, plus WoodworthFallbackTests).

**`HRTF/`:**
- Purpose: SOFA-format HRTF profile binaries, Git LFS tracked (per `CLAUDE.md`: "5 SOFA files embedded as BinaryData").
- Generated: No (source assets). Committed: Yes (via LFS).

**`docs/`:**
- Purpose: project/integration documentation. Contains `development-roadmap.md` (marked superseded per commit `3da7d89`'s message: "mark development-roadmap.md as a superseded pre-extraction planning doc"), `integration-guide.md`, `conductor-setup-guide.md`, `workflow-tutorials.md`.
- Caution: per this mapping task's brief, `docs/` and `CLAUDE.md` are known to lag the current tree (e.g. algorithm count, `Engine/` module, two-link-target requirement were all previously wrong or missing until commit `3da7d89` partially fixed `CLAUDE.md`/`README.md`).

**`.planning/`:**
- Purpose: GSD workflow state — `PROJECT.md`, `ROADMAP.md`, `REQUIREMENTS.md`, `STATE.md`, `codebase/` (this document's home), `intel/` (synthesis notes).
- Generated: partially (codebase/ docs are agent-generated). Committed: Yes.

**`build/`, `build-map/`, `JUCE/`:**
- Purpose: CMake build output / vendored JUCE checkout. Excluded from this analysis per task scope. `build-map/_deps/` shows FetchContent pulls JUCE, mysofa, and Catch2 sources when not locally vendored.
- Generated: Yes. Committed: No (should be gitignored).

## Key File Locations

**Entry Points:**
- `include/SpatialCore/Engine/RenderEngine.h` / `src/Engine/RenderEngine.cpp`: `RenderEngine::renderBlock` — the per-block consumer-facing entry point.
- `include/SpatialCore/SpatialCore.h`: umbrella header for DSP-only consumption.

**Configuration:**
- `CMakeLists.txt` (root): defines `SpatialCore` and `SpatialCoreUI` static-lib targets, JUCE/libmysofa/zlib dependency wiring, `PROJECT_IS_TOP_LEVEL` standalone-vs-consumed link logic.
- `tests/CMakeLists.txt`: `SpatialCoreTests` executable, Catch2 FetchContent.

**Core Logic:**
- `include/SpatialCore/Core/Types.h`: `MAX_SOURCES = 12`, `MAX_SPEAKERS = 16`, `ObjectState`, `BinauralGains`, `LayoutContext`, `BinauralContext`, `kDefaultBinauralProfiles[5]`.
- `include/SpatialCore/Algorithms/SpatializationAlgorithm.h`: frozen abstract interface (D-02) all 8 algorithms implement.
- `include/SpatialCore/Trajectory/DopplerVelocity.h`: independent `kMaxObjects = 12` constant (does not reference `MAX_SOURCES`).

**Testing:**
- `tests/Engine/RenderEngineTests.cpp`: facade regression tests (referenced by name in `RenderEngine.h` comments as the byte-for-byte-behavior guardrail for `engineComputesGains`).
- `tests/Binaural/*Tests.cpp`: per-HRTF-profile convolution correctness tests.

## Naming Conventions

**Files:**
- Header/implementation pairs share a name: `FooAlgorithm.h` / `FooAlgorithm.cpp` (e.g. `VBAPAlgorithm.h`/`.cpp`).
- Header-only Core types have no `.cpp` counterpart except `SpatialMath.h`/`.cpp`.
- Test files: `<ModuleOrFeature>Tests.cpp`, one per functional area or HRTF profile, e.g. `RenderEngineTests.cpp`, `BernschuetzKU100Tests.cpp`.
- Umbrella/aggregator headers: `AllAlgorithms.h` (all 8 algorithm headers), `SpatialCore.h` (whole-library DSP umbrella).

**Directories:**
- PascalCase module names matching C++ namespaces/domains: `Algorithms`, `Binaural`, `Core`, `Engine`, `IO`, `OSC`, `Trajectory`, `UI`.
- `include/SpatialCore/<Module>/` mirrors `src/<Module>/` mirrors `tests/<Module>/` exactly (module-for-module), except `UI` has no `tests/UI/` directory and `Core` has almost no `.cpp` files.

**Types/Classes:**
- Concrete algorithm classes: `<Name>Algorithm` (e.g. `VBAPAlgorithm`, `DirectBinauralAlgorithm`).
- Facade/orchestration classes: `<Name>Engine` (`RenderEngine`, `TrajectoryEngine`).
- Data hand-off structs: `<Verb/Noun>Context` (`LayoutContext`, `BinauralContext`, `RenderBlockContext`) or `<Noun>Sources`/`<Noun>State` (`RenderSources`, `ObjectState`, `LayoutState`).

## Where to Add New Code

**New spatialization algorithm:**
- Header: `include/SpatialCore/Algorithms/<Name>Algorithm.h`, implementing `SpatializationAlgorithm` (do not change the frozen virtual interface, per D-02 — `SpatializationAlgorithm.h:9-13`).
- Implementation: `src/Algorithms/<Name>Algorithm.cpp`.
- Register in: `include/SpatialCore/Algorithms/AllAlgorithms.h`, `include/SpatialCore/SpatialCore.h`, and `CMakeLists.txt`'s `add_library(SpatialCore STATIC ...)` source list.
- Tests: `tests/Algorithms/` (existing pattern uses a shared `SpatializationAlgorithmTests.cpp` covering multiple algorithms rather than one file per algorithm — check that file before adding a new one).

**New output format / speaker layout:**
- `include/SpatialCore/IO/OutputFormat.h`: add to the `OutputFormat` enum (currently 25 values) and `OutputFormatInfo` registry entry.
- `include/SpatialCore/IO/SpeakerLayout.h` / `src/IO/SpeakerLayout.cpp`: add to `LayoutID` enum (currently 14 named layouts + `NUM_LAYOUT_DEFS` sentinel) and the layout-definition table.
- Tests: `tests/IO/SpeakerLayoutTests.cpp`, `tests/IO/AmbisonicsCodecTests.cpp` if Ambisonics-adjacent.

**Changes to the RenderEngine facade contract:**
- `include/SpatialCore/Engine/RenderEngine.h`: `RenderSources`/`RenderBlockContext` field additions should follow the existing pattern of MAX_SOURCES-indexed flat arrays (documented as provisional but current convention) and update the extensive inline comments explaining consumer-vs-engine field ownership.
- Any behavior change must preserve the exact 5-branch dispatch order (`RenderEngine.h:199-206`) unless deliberately changing it with a version bump.
- Tests: `tests/Engine/RenderEngineTests.cpp`.

**New UI widget:**
- Header: `include/SpatialCore/UI/<Name>.h`. Implementation: `src/UI/<Name>.cpp`.
- Add to `CMakeLists.txt`'s `add_library(SpatialCoreUI STATIC ...)` source list — NOT the `SpatialCore` target (DSP/GUI separation is enforced by having two targets).
- No `tests/UI/` directory currently exists; there is no established UI test pattern to follow.

**Utilities:**
- Shared math helpers: `include/SpatialCore/Core/SpatialMath.h` / `src/Core/SpatialMath.cpp`.
- Shared per-object data types: `include/SpatialCore/Core/Types.h`.

## Special Directories

**`HRTF/`:**
- Purpose: SOFA-format HRTF profile binary assets.
- Generated: No. Committed: Yes (Git LFS).

**`.planning/codebase/`:**
- Purpose: this document and its siblings (STACK.md, INTEGRATIONS.md, ARCHITECTURE.md, plus CONVENTIONS/TESTING/CONCERNS when generated) — GSD codebase-map output, consumed by `/gsd-plan-phase` and `/gsd-execute-phase`.
- Generated: Yes (by mapper agents). Committed: Yes.

**`build/`, `build-map/`:**
- Purpose: CMake build directories, including `build-map/_deps/` FetchContent checkouts of JUCE/mysofa/Catch2 sources.
- Generated: Yes. Committed: No — excluded from this structural analysis per task scope.

---

*Structure analysis: 2026-08-10*
</content>
