# Context

> **Historical — superseded by `.planning/ROADMAP.md`, 2026-08-10.** This document was derived
> from the 2026-08-09 ingest, which ran against a branch missing 42 commits. Counts, status,
> and constraints below are a dated snapshot and are not maintained. For current canonical
> counts see `CLAUDE.md` and `README.md`; for current requirements and phase status see
> `.planning/REQUIREMENTS.md` and `.planning/ROADMAP.md`.

Synthesized: 2026-08-09
Paths are relative to repo root `/Users/andrewrahman/conductor/workspaces/SpatialCore/kelowna/`.

Reference material from the three DOC-classified sources plus non-normative narrative from the PRD.
Nothing here is a requirement or a binding constraint.

---

## Consumer plugin integration workflow
- source: docs/integration-guide.md

Prerequisites: JUCE 8 (C++17), CMake 3.22+, a C++17 compiler (Clang on macOS, MSVC on Windows).

Setup sequence:
1. `mkdir OpenSpatialYourEffect && cd OpenSpatialYourEffect && git init`
2. `git submodule add https://github.com/Spatial-Media-Lab/SpatialCore.git SpatialCore` and `git submodule add https://github.com/juce-framework/JUCE.git JUCE`
3. Plugin `CMakeLists.txt`: `add_subdirectory(JUCE)`, `add_subdirectory(SpatialCore)`, `juce_add_plugin(...)` with `COMPANY_NAME "Spatial Media Lab"`, `PLUGIN_MANUFACTURER_CODE SMLb`, `FORMATS VST3 AU`, then `target_link_libraries(OpenSpatialYourEffect PRIVATE SpatialCore)`
4. Build: `cmake -B build -DCMAKE_BUILD_TYPE=Release` then `cmake --build build --config Release`; the post-build script copies AU and VST3 to `~/Library/Audio/Plug-Ins/` on macOS.

Processor usage pattern: keep all spatial parameters (outputFormat, algorithm, hrtfProfile, per-object azimuth/elevation/distance), add effect-specific parameters, implement effect DSP in processBlock and call SpatialCore for spatialization. Illustrative members: `spatialcore::BinauralRenderer binauralRenderer;`, `spatialcore::SpatializationAlgorithm* algorithms[6];`, `spatialcore::HRTFDatabase hrtfDb;`. Per-object loop: run effect DSP to a mono sample, build `spatialcore::SourcePosition`, spatialize, then mix dry/wet and output.

Editor usage pattern: `#include <SpatialCore/UI/SpatialMapComponent.h>` and `SMLLookAndFeel.h`; hold `spatialcore::SMLLookAndFeel` and `spatialcore::SpatialMapComponent`; `setLookAndFeel(&smlLookAndFeel)`, `addAndMakeVisible(spatialMap)`, `setSize(820, 580)` (standard SML plugin size).

Keep from SpatialCore: output format detection and bus negotiation; algorithm selection and dispatch; HRTF profile loading and convolution; speaker layout activation; ADM-OSC receive/send; trajectory animation; spatial map component; LookAndFeel and shared widgets; soft clipper and output limiter. Replace with your own DSP: effect engine, per-object processing, modulation/feedback/filters, tempo sync/timing, effect-specific parameters, effect UI controls.

Naming convention: repository and plugin name `OpenSpatial{Effect}`; unique 4-char plugin code (e.g. `OsCh` for Chorus); manufacturer code `SMLb`.

Consumer testing pattern: FetchContent Catch2 v3.7.1, `add_executable(YourPluginTests tests/YourTests.cpp)`, link `Catch2::Catch2WithMain` and `SpatialCore`.

Preset system pattern (follows OpenSpatialDelay): presets at `~/Library/Audio/Presets/OpenSpatial{Effect}/`; `PresetData` struct with JSON serialization; factory presets in C++ source installed via a build-time CLI tool; user presets in a `User/` subfolder, never overwritten.

## Program narrative — origin and module extraction map
- source: docs/development-roadmap.md ("Current State", Phase 1 module table, "Architecture Pattern")

SpatialCore is being pulled out of OpenSpatialDelay, where ~68% of the codebase was marked `SPATIAL MEDIA LIBRARY`. Extraction mapping: Algorithms and Binaural and IO from `PluginProcessor.h/cpp`; OSC, Trajectory and DSP from `PluginProcessor.cpp`; UI from `PluginEditor.h/cpp`. Every SML plugin is ~32% plugin-specific DSP layered over SpatialCore (~68% reusable framework), linked via CMake subdirectory.

The PRD's own snapshot of state (dated 2026-03-22) describes SpatialCore as complete scaffolding with stub implementations: headers for all modules, CMakeLists.txt, `src/` stubs returning zeroed/default values, and a Catch2 suite of 22 smoke tests / 52 assertions. See `.planning/INGEST-CONFLICTS.md` — `.planning/codebase/` contradicts this snapshot.

## Phase 3 plugin delivery pattern
- source: docs/development-roadmap.md (Phase 3)

Each future plugin follows the same four steps: create a repo with SpatialCore as a git submodule; implement effect-specific DSP (~32% custom code); use SpatialCore's rendering paths, algorithms and UI components; build, test and release.

## Developer tooling — Conductor setup
- source: docs/conductor-setup-guide.md

Conductor (conductor.build, macOS only) manages parallel Claude Code workspaces across Spatial Media Library repos. Setup was recorded as complete on 2026-03-22, with OpenSpatialDelay (workspaces Valletta, Brisbane) and SpatialCore (workspace Tyler) added, both against private `github.com/AndrewRahman/*` remotes.

Prerequisites: `gh auth status` showing a logged-in GitHub CLI, and Claude Code logged in via `claude /login`. Conductor requires each repo to have a GitHub `origin` remote; local-only projects must be `git init`-ed, committed, and pushed with `gh repo create RepoName --private --source=. --push` first.

Flow: install the app; add each repo (local folder or Git URL); a workspace is created automatically, each an isolated copy of the repo on its own branch with its own Claude agent; more workspaces via Cmd+N / Cmd+Shift+N; optional per-repo setup script (for JUCE projects: export a homebrew-inclusive PATH then `cmake -B build -DCMAKE_BUILD_TYPE=Release`) and optional run script (10 ports allocated per workspace starting at `$CONDUCTOR_PORT`). Script environment variables: `$CONDUCTOR_WORKSPACE_PATH` and `$CONDUCTOR_ROOT_PATH`. Review via the Changes tab and Diff Viewer, open in IDE with Cmd+O, then sync to GitHub and merge branches or open PRs. Shortcuts: Cmd+N, Cmd+Shift+N, Cmd+O, Cmd+1/Cmd+2 to switch workspaces.

This document is developer-workflow tooling; it defines nothing about the SpatialCore library itself.

## Developer tooling — multi-plugin workflow patterns
- source: docs/workflow-tutorials.md

Three patterns for parallel work with Claude Code, written for non-technical users.

Conductor pattern — one Claude session acts as conductor, creates isolated workspaces for 2-3 plugins, delegates to agents, summarizes and helps merge branches. Terms: conductor (main session that plans/delegates/reviews), agents (helper sessions with isolated copies), worktree (separate working folder from the repository). Work stays on its own branch until explicitly merged.

Worktree pattern — one Terminal window per plugin folder, each running `claude` independently; full independence between plugins; switch windows with Cmd+backtick, stop with Escape, resume by `cd`-ing back and running `claude`.

GitTree pattern — multiple features of a single plugin via git worktrees, one folder and branch per feature ("Start a worktree called ...", "Exit the worktree and keep the changes"); feature branches are preserved and merged into main one at a time.

Selection guidance: Conductor for 2-3 plugins in one conversation or quick cross-plugin prototyping; Worktree for completely separate sessions per plugin; GitTree for multiple features of one plugin or deep focused work.

This document is developer-workflow tooling; it defines nothing about the SpatialCore library itself.
