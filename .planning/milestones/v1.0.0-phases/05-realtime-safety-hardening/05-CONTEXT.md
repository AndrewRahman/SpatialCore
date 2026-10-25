# Phase 5: Realtime Safety Hardening - Context

**Gathered:** 2026-10-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Nothing reachable from `processBlock` (in SpatialCore terms: everything `RenderEngine::renderBlock()`
reaches) allocates, locks, or races, proven in a Release-class build with tools. Requirements:
RTSF-01, RTSF-02, RTSF-03, RTSF-05, VERIFY-02 (ROADMAP Phase 5 criteria 1-5).

- RTSF-01: the two `jassertfalse` + `resize()` guards in `BinauralRenderer` (SpatialCore#19).
- RTSF-02: `TrajectoryEngine` `finalAz_`/`finalEl_`/`finalDist_` plain floats read across threads (SpatialCore#18).
- RTSF-03: `HRTFDatabase` libmysofa handle (`easyHandle`, `loaded`) races with `setProfile()`.
- RTSF-05: race checker plus allocation detector over the whole audio path report zero.
- VERIFY-02: several engines in one process render concurrently without crash or buzz (SpatialCore#12; OSD#96, OSD#131).

Not in scope: CI wiring (Phase 6, CI-01), coverage numbers (Phase 6), the OSD migration itself,
removing the legacy HRTF route (next major). RTSF-04 is closed as a non-finding. The `DBG()` calls in
`BinauralRenderer.cpp`/`HRTFDatabase.cpp` were audited as configuration-thread only and are out of scope.

</domain>

<decisions>
## Implementation Decisions

### How the safety proof runs (RTSF-05)
- **D-01:** **A separate check, run at every phase end gate and before any release tag.** It is a
  third build tree (optimised with debug info, i.e. RelWithDebInfo, plus ThreadSanitizer) and a
  no-allocation test pass. The everyday Debug + Release local gate stays as it is today. README's
  "Local test gate" documents the extra step. Phase 3 (03-07) found an -O0 instrumented SADIE load
  exceeds the engine's 15 s `stopThread`, so the race-checker build must be optimised.
- **D-02:** **The proof walks every render path, every output format and every switch.** All 5
  render paths, all 23 output formats, plus mid-play HRTF profile switches (`setHRTFProfile`,
  including 0 ↔ SOFA), output-format switches with and without `engineFadesFormatSwitch`, all three
  opt-in engine flags (`engineComputesGains`, `engineDerivesDispatch`, `engineSelectsHRTF`), and
  trajectories animating. "Whole audio path" means whole; a format-specific allocation must not slip
  through.

### What a "can't happen" case does (RTSF-01)
- **D-03:** **Keep the last good sound; never allocate.** If an audio-thread buffer is found too
  small (reachable only on a prepare-contract violation), skip just that update: in
  `updateSourceHRIR` the source keeps its previous HRIR; in `renderSourceBuffers` the affected
  work is skipped for that block rather than resized. No silence-by-design, no allocation. Prefer one
  shared guarded helper over two copies (REQUIREMENTS RTSF-01).
- **D-04:** **Count it and assert in Debug.** A lock-free counter readable from any thread (tests and
  plugins can read it) plus a Debug-build assert, matching the SC-17
  `getWriterViewOnRenderThreadCount()` pattern. Shipped builds stay silent apart from the counter.
  Public API addition is additive only (minor bump).

### Multi-instance proof (VERIFY-02)
- **D-05:** **Automated in-process stress test only.** Many `RenderEngine`s in one process, created,
  prepared, rendering and destroyed on their own threads at once (including concurrent first use of
  `SharedFFTCache`), run under the race checker. Claude runs it; the user runs nothing. No real-DAW
  run in this phase: real-host multi-instance testing happens when OSD migrates, outside this repo.
- **D-06:** **Include the UI.** OSD#131 was a CoreGraphics crash with several plugin windows open,
  and `SpatialCoreUI` now ships that code. Several `SpatialMapComponent`s are created and painted at
  once in the `SpatialCoreUITests` target.
- **D-07:** **Each regression test is shown to fail against the old behaviour.** Temporarily
  reintroduce the old bug (for example a per-instance or unguarded FFT cache), show the test fails,
  restore, and record the evidence in the plan SUMMARY, as in the Phase 3 mutation checks. Applies
  to the OSD#96 and OSD#131 regressions and the RTSF fixes where practical.
- **D-08:** `SharedFFTCache` is create-once-never-destroy by design: document it as a deliberate
  permanent allocation, not a leak (REQUIREMENTS VERIFY-02 note).

### Legacy HRTF route and sound (RTSF-03)
- **D-09:** **Keep the legacy route, document its thread contract, detect misuse.**
  `getBinauralRenderer()` / `getPrepareRendererIndex()` / `swapActiveRenderer()` stay working so OSD
  is not broken before its migration. Docs state exactly which thread may call what; a misuse
  counter plus Debug assert (SC-17 pattern) catches a call from the wrong thread. Removal waits for
  the next major version. The `setHRTFProfile` route (DATA-01) is the safe route and is what
  RTSF-03's "`setProfile()` from a UI timer during audio processing" scenario must be proven against.
  — **Reversibility:** costly — removing or reshaping these public methods later breaks OSD's current calls and needs a major version bump.
- **D-10:** **Any fix must be bit-identical in sound.** If the checker finds libmysofa allocating on
  the audio thread (for example in the per-block HRIR lookup reached from
  `RenderEngine.cpp` `updateSourceHRIR`), the fix (such as precomputing lookups at load time) must
  produce exactly today's output, proven against the Phase 3 binaural goldens. If bit-identity is
  impossible, stop and ask the user; do not re-baseline.

### Decisions added at plan time (2026-10-06, after research)
- **D-11:** **An oversized host block is rendered correctly in pieces.** When a host delivers a
  block longer than the prepared maximum, the engine-side sites (`RenderEngine.cpp` resize sites
  found by research) process it in prepared-size chunks, as `PartitionedConvolver` already does: no
  audio is dropped and nothing allocates. The existing CR-03 oversized-block test changes to assert
  this. (D-03's "keep the last good sound" still governs the `BinauralRenderer` scratch guards that
  cannot be chunked.)
- **D-12:** **OSD#131 UI scope is a test only.** Add the multi-map regression test proving several
  `SpatialMapComponent`s share their typefaces and paint without crashing. `SMLLookAndFeel`'s
  per-instance fonts are not changed in this phase.
- **D-13:** **Platform scope of the proof is macOS arm64.** Docs state "proved on macOS (Apple
  Silicon); Windows and Linux are checked in Phase 6". Phase 5 closes on the macOS proof.

### Claude's Discretion
- **Allocation detector mechanism (D-01/D-02):** the user said "you decide". Options: a test-only
  allocation trap (operator new/malloc hook armed around `renderBlock`) that works with the current
  toolchain, Clang's RealtimeSanitizer (`-fsanitize=realtime`) where the toolchain supports it, or
  both. Recommendation recorded during discussion was both. Locks must be detected too, not only
  allocations.
- RTSF-02 fix shape: atomic floats vs an atomically swapped struct (REQUIREMENTS allows either);
  must keep the per-object az/el/dist triple from tearing in a way that is audible.
- Exact counter names and where the shared guarded helper lives.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope and requirements
- `.planning/ROADMAP.md` §"Phase 5: Realtime Safety Hardening" — goal, 5 success criteria, 2026-08-10 scope corrections
- `.planning/REQUIREMENTS.md` §"Realtime Safety" (RTSF-01..05) and VERIFY-02 — acceptance per requirement
- `.planning/PROJECT.md` — locked rule DR-1 (no allocation, locks or logging reachable from `processBlock`), DR-7 (`SpatializationAlgorithm` frozen)
- `CLAUDE.md` §"Design Principles" — lock-free audio path, three-slot layout handoff (SC-16/SC-17), profile loader and mailbox (DATA-01), the RTSF-01 note

### Prior-phase evidence
- `.planning/phases/03-binaural-defects-hrtf-packaging/` — 03-06/03-07 SUMMARYs (loader/mailbox design, TSAN pass, RelWithDebInfo requirement), 03-02 binaural goldens (bit-identity oracle for D-10), 03-03 (scratch sizing in `prepare()`/`setProfile()`)
- `README.md` §"Local test gate" — the gate D-01 extends

### Issues
- SpatialCore#12 (multi-instance), #18 (TrajectoryEngine), #19 (BinauralRenderer allocation)
- Spatial-Media-Lab/OpenSpatialDelay#96 (two instances buzzing + Reaper crash), #131 (CoreGraphics crash, multiple instances)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `RenderEngine::getWriterViewOnRenderThreadCount()` (`include/SpatialCore/Engine/RenderEngine.h:421`, `src/Engine/RenderEngine.cpp:1721`): the counter + Debug assert pattern for D-04 and D-09.
- `RouteRenderRig` (Phase 4 tests) and the `[hrtf-switch][churn]` / `[threads]` tests (Phase 3): starting points for the all-paths proof and the stress test.
- `SpatialCoreUITests` target (Phase 4, 04-05): home for the D-06 multi-map test.

### Sites to change
- `src/Binaural/BinauralRenderer.cpp:178-183` (`updateSourceHRIR` guard) and `:223-228` (`renderSourceBuffers` guard): RTSF-01.
- `include/SpatialCore/Trajectory/TrajectoryEngine.h:95-116` (plain-float finals, only `active_` atomic): RTSF-02.
- `include/SpatialCore/Binaural/HRTFDatabase.h:126-129`, `src/Binaural/HRTFDatabase.cpp` (`easyHandle`, `loaded`, `mysofa_getfilter_float` at :289): RTSF-03 and the D-10 check.
- `src/Binaural/PartitionedConvolver.cpp:14-27` (`SharedFFTCache::getOrCreate`, spinlock, static instance): VERIFY-02.
- `src/Engine/RenderEngine.cpp:1116` (per-block `updateSourceHRIR` call).

### Established Patterns
- Opt-in flags and additive API only; flag-off output byte-for-byte unchanged.
- Mutation evidence recorded in SUMMARY files (Phase 3).
- Local gate is the only gate; CI runs zero tests until Phase 6.

</code_context>

<specifics>
## Specific Ideas

- The user is not a programmer: Claude runs every check (builds, sanitizers, stress tests, mutation runs) and brings back only plain-English judgment calls.
- "Keep the last good sound" is the guiding behaviour for any defensive path on the audio thread.

</specifics>

<deferred>
## Deferred Ideas

- Real-DAW multi-instance check (OSD loaded several times in Reaper): happens with the OSD migration, outside this repo.
- Removing the legacy HRTF route (`getBinauralRenderer` / `swapActiveRenderer`): next major version.
- Running the sanitizer build in CI: Phase 6 (CI-01).

</deferred>

---

*Phase: 05-realtime-safety-hardening*
*Context gathered: 2026-10-05*
