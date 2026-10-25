# Phase 6: Consumer Readiness & CI - Context

**Gathered:** 2026-10-17
**Status:** Ready for planning

<domain>
## Phase Boundary

Prove that OpenSpatialDelay can adopt SpatialCore as a submodule without surprises: a minimal
consumer harness that submodules, links and runs SpatialCore; a fix for the silent
`engineComputesGains` trap; CI that actually runs the tests on macOS (green, against JUCE 9.0.0);
and measured coverage with the remaining OSD regression tests. Requirements: INTG-01, INTG-02,
CI-01, TEST-01. OSD's own migration lives in the OSD repo and is not part of this phase.

**Starting position (found 2026-10-17, do not rebuild):**
- `tests/Binaural/PartitionedConvolverTests.cpp` and `tests/Binaural/BinauralRendererTests.cpp`
  already exist (TEST-01's "dedicated tests" clause is largely met; verify, don't recreate).
- Regression tests already cite `OpenSpatialDelay#89` (`SadieD2KU100Tests.cpp:67`), `#96`
  (`Engine/MultiInstanceTests.cpp`), `#131` (`UI/MultiMapTests.cpp`), `#234`
  (`PartitionedConvolverTests.cpp:130`). **`#50` (dual-convolver HRTF transition,
  `src/Binaural/PartitionedConvolver.cpp:50,90,209`) has no regression test.**
- CI (`.github/workflows/ci.yml`) is Linux-only, green, and runs **zero tests**: the 2026-10-17
  run logs "No tests were found!!!". Cause: `ctest` runs in `build/` but the root
  `CMakeLists.txt` has no `enable_testing()`; tests are discovered in `build/tests`
  (`tests/CMakeLists.txt:107-108`).
- No coverage tooling exists anywhere in the build.

</domain>

<decisions>
## Implementation Decisions

### Consumer harness (INTG-01)
- **D-01:** The harness lives **in this repo** (e.g. `examples/harness/`) and pulls SpatialCore in
  **as a git submodule from the GitHub remote**, exactly as OSD does, then
  `add_subdirectory(SpatialCore)` + `target_link_libraries(... PRIVATE SpatialCore SpatialCoreUI)`.
  It must not point at the local source folder — the submodule step is what is being proved.
  Remote is `https://github.com/AndrewRahman/SpatialCore` (CLAUDE.md; `Spatial-Media-Lab/` does
  not resolve yet, DR-18). It is built from a clean clone in Release on macOS, in CI.
- **D-02:** **Add a UI umbrella header `<SpatialCore/SpatialCoreUI.h>`** that includes all public
  UI headers. The harness includes exactly `<SpatialCore/SpatialCore.h>` and
  `<SpatialCore/SpatialCoreUI.h>`. The DSP umbrella stays UI-free on purpose. Amend ROADMAP
  Phase 6 SC-1 and REQUIREMENTS INTG-01 ("includes only `<SpatialCore/SpatialCore.h>`") to the
  two-header wording. Also fold in audit W2 (`OSC/OSCPortValidation.h` missing from the umbrella)
  and W3 (stale umbrella comment) while touching the umbrellas.
- **D-03:** The harness **uses all five opt-in facade features** with no worked example today
  (audit W1): `setHRTFProfile`, `engineSelectsHRTF`, `engineFadesFormatSwitch`,
  `setAlgorithmIndex`, `getBlockLayout()` — alongside `engineComputesGains` and
  `engineDerivesDispatch`. It is the copy-paste reference for OSD's migration.
- **D-04:** Beyond building, the harness has a **short headless automatic sound check** run in CI:
  render blocks for each output format and assert output is finite and not silent. No DAW.

### Gains safety (INTG-02)
- **D-05:** **Keep `engineComputesGains` default `false`** (OSD's sound stays byte-for-byte; the
  CLAUDE.md compatibility promise holds). Add **detection**: when the flag is off and a block has
  active objects whose supplied gains are all zero, the engine counts it in a lock-free counter
  readable from any thread (same pattern as `getOversizedBlockCount()` /
  `getWriterViewOnRenderThreadCount()`) and asserts in Debug builds. — **Reversibility:** reversible
- **D-06:** In Release: **count only, keep playing.** No audio-thread logging, no silent fallback
  that computes gains on the consumer's behalf (that would hide the mistake #14 was filed about).
- **D-07:** Scope is **gains only**. The other opt-in flags get no detector. The integration guide
  gets one clear "set these flags" block naming the recommended set and which mode a consumer must
  choose (INTG-02 acceptance). No one-call helper API.

### CI (CI-01)
- **D-08:** Runners (GitHub-hosted, repo is public so macOS minutes are free; the user only has a
  Mac locally):
  - **macOS (Apple Silicon) — the gate:** full tests.
  - **Linux:** keep the existing job and fix it so it actually runs the tests.
  - **Windows:** build-only (catches breakage before OSD ships a Windows build).
  This honours Phase 5 D-13's "Windows and Linux are checked in Phase 6" in a limited form;
  amend REQUIREMENTS CI-01's scope note ("cross-platform matrices remain out of scope")
  accordingly. — **Reversibility:** reversible
- **D-09:** Every push/PR on macOS runs the **same as the local gate**: Debug build + both test
  executables, Release build + `ctest`, `SpatialCoreUITests`, and the harness build + sound check.
  CI then replaces "the local gate is the only gate" in CLAUDE.md / README / memory.
- **D-10:** ThreadSanitizer build + `[rt]` trap pass run **nightly (schedule) and on
  `workflow_dispatch`**, not on every push. They are still run by hand at every phase end and
  before a release tag.
- **D-11:** **Require green CI to merge to `main`** via GitHub branch protection, turned on once
  the macOS job is green. This is an outward-facing repo-settings change: confirm with the user at
  the moment it is applied.
- **D-12:** Fix the existing workflow's stale comments (Phase 8/9 plan references, "suite is empty
  of new tests") and the `ctest` working-directory / `enable_testing()` defect.

### Coverage (TEST-01, OQ-4)
- **D-13:** **Measure first, then the user picks the target mid-phase.** Wire instrumentation into
  CMake, produce the real number per area (algorithms, binaural, engine, IO, OSC, trajectory, UI),
  then a `checkpoint:decision` presents it in plain terms and the user sets the target. OQ-4
  closes there.
- **D-14:** Once set, **CI fails when coverage drops below the target.**
- **D-15:** Reports stay **inside GitHub**: a job summary on each run plus a downloadable artifact.
  No external service (no Codecov, no account, no data sent to a third party).
- **D-16:** UI code is **measured as a separate number** (from `SpatialCoreUITests`), so the UI
  cannot move the engine's figure. Amend the REQUIREMENTS TEST-01 note that excluded `src/UI/*.cpp`
  (written before `SpatialCoreUITests` existed).

### Regression tests (TEST-01)
- **D-17:** Add the missing `OpenSpatialDelay#50` regression test, and confirm each of the four
  existing ones (#89, #96, #131, #234) fails against the old behaviour, under Phase 5's D-07 rule
  (temporarily restore the old behaviour, see red, restore). Record the red evidence.

### Claude's Discretion
- Coverage tool choice (Clang source-based `-fprofile-instr-generate` + `llvm-cov` vs gcov/lcov),
  and which build type it runs in.
- Exact harness folder name and whether it is a `juce_add_plugin` or a console app (it must link
  both targets and exercise the five features; a plugin target is preferred if it builds headless
  in CI).
- Exact detection rule for D-05 (e.g. per-object all-zero gain row, which objects count as
  active) and the counter's name.
- macOS runner image and JUCE/LFS caching in CI.
- How the Windows build-only job handles LFS HRTF data and the `%ProgramData%` path (build only;
  no claim that it runs).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope and requirements
- `.planning/ROADMAP.md` § "Phase 6: Consumer Readiness & CI" — success criteria 1-5, OQ-4
- `.planning/REQUIREMENTS.md` — INTG-01, INTG-02, CI-01, TEST-01 (and their scope notes D-02/D-08/D-16 amend)
- `.planning/v0.5.0-MILESTONE-AUDIT.md` — gaps for Phase 6, warnings W1 (opt-in features lack an example), W2 (OSCPortValidation.h not in umbrella), W3 (stale umbrella comment)
- `.planning/PROJECT.md` — External Dependencies: v1 closes when OSD ships on SpatialCore (out of this repo)

### Prior decisions that bind this phase
- `.planning/phases/05-realtime-safety-hardening/05-CONTEXT.md` — D-07 (regression tests shown red against old behaviour), D-13 (Windows/Linux promised in Phase 6), deferred "sanitizer build in CI"
- `.planning/phases/03-binaural-defects-hrtf-packaging/03-CONTEXT.md` — HRTF data resolution the harness must follow (embedded BinaryData, shared folder override)

### Consumer contract and build
- `CLAUDE.md` — consumer CMake snippet, remote topology, flags (`engineComputesGains`, `engineDerivesDispatch`, `engineSelectsHRTF`, `engineFadesFormatSwitch`), local gate description
- `docs/integration-guide.md` — consumer guide; lines ~119-158 and ~350 describe the flags; gets the D-07 "set these flags" block
- `README.md` § "Local test gate" (line 110) and § "Realtime-safety check" (line 132) — exact commands CI must mirror
- `include/SpatialCore/SpatialCore.h` — DSP umbrella
- `include/SpatialCore/Engine/RenderEngine.h` — `RenderBlockContext` (flag at line 189), existing lock-free counters (lines ~421-436)
- `.github/workflows/ci.yml` — current Linux-only workflow
- `CMakeLists.txt` (JUCE 9.0.0 FetchContent ~line 30, `SPATIALCORE_BUILD_TESTS` line 361), `tests/CMakeLists.txt` (catch_discover_tests lines 107-108), `examples/CMakeLists.txt` (demo pattern)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `examples/demo/` + `examples/CMakeLists.txt`: existing worked example (ADM-OSC, trajectories, map into `RenderEngine`) with `--selftest` mode; pattern for the harness, but it links via the parent build, not a submodule.
- `tests/Support/RouteRenderRig.h`, `RealtimeGuard.h`, `rtguard/`: render rigs and the allocation/lock trap, reusable for the harness sound check and the nightly `[rt]` job.
- `RenderEngine` lock-free counters (`getOversizedBlockCount`, `getWriterViewOnRenderThreadCount`, `getLegacyRouteWrongThreadCount`): the pattern for the D-05 counter.

### Established Patterns
- Misuse is counted lock-free and asserted in Debug only; never logged from the audio thread (Phase 5 D-04, D-09).
- Regression tests cite the full tracker (`Spatial-Media-Lab/OpenSpatialDelay#N`) and are shown red first (Phase 1 API-05, Phase 5 D-07).
- Release is the CI build type; Debug and Release both must pass (Phase 2 G-02-10).

### Integration Points
- `RenderEngine::renderBlock()` gain path, where the flag-off branch reads `objChannelGains`/`objGains`: the D-05 detector goes here.
- `.github/workflows/ci.yml`: becomes a macOS gate + Linux tests + Windows build + nightly sanitizer workflow.
- CLAUDE.md / README / user memory "CI runs zero tests": update once CI runs the suite.

</code_context>

<specifics>
## Specific Ideas

- The user only owns a Mac. Every check must run on their Mac or on GitHub's hosted runners; never ask them to operate Windows or Linux.
- The user is not a programmer: the coverage-target checkpoint (D-13) must present numbers in plain terms (per area, what "X% covered" means) and ask one judgment call.

</specifics>

<deferred>
## Deferred Ideas

- Running the full test suite on Windows (D-08 keeps Windows build-only).
- Detectors for the other opt-in flags, or a one-call "recommended setup" helper API (D-07).
- External coverage service / README badge (D-15).
- Real-DAW multi-instance check and OSD's own migration: OSD repo.

</deferred>

---

*Phase: 06-consumer-readiness-ci*
*Context gathered: 2026-10-17*
