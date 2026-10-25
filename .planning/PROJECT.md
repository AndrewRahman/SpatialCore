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
- **Revenue model**: None. SpatialCore is open source (GPL-3.0) and is not sold.
- **Success metric**: Number of shipping plugins built on SpatialCore (target for v2: two).

## Milestones

Only **v1** is decomposed into roadmap phases. v2 and v3 are recorded here and will be
decomposed when their milestone opens.

| Milestone | Success metric | Status |
|-----------|----------------|--------|
| **v1** | OpenSpatialDelay ships on SpatialCore as a submodule with zero regressions | 🚧 SpatialCore side shipped as milestone v1.0.0 (2026-10-24, archived in `.planning/milestones/`); OSD migration is consumer-repo work and still open |
| **v2** | A second plugin is built on SpatialCore from scratch, proving the API generalizes | 📋 Future |
| **v3** | Public release under the Spatial-Media-Lab org with docs and a tagged v1.0 | 📋 Future |

## Requirements

### Validated

Shipped in milestone v1.0.0 (all 25, closed 2026-10-24). Full acceptance text in
`.planning/milestones/v1.0.0-REQUIREMENTS.md`.

- ✓ **API-01** — algorithm count is 8, derived from `AllAlgorithmTypes` and frozen by `static_assert` — v1.0.0 (Phase 1)
- ✓ **API-02** — `outputLimiter()` is the shipped tanh soft ceiling — v1.0.0 (Phase 1)
- ✓ **API-03** — HRTF profile count is 5 — v1.0.0 (Phase 1)
- ✓ **API-04 / API-05 / BUG-03** — 23 formats / 15 layouts match the tree; CLAUDE.md accurate; cross-repo issue references qualified — v1.0.0 (Phase 1)
- ✓ **EXTR-01** — all 8 algorithms verified against independent oracles (textbook VBAP/VBIP/DBAP, ear 2.1.0); below-horizon VBAP follows ITU-R BS.2127 (EAR); VBIP is textbook DAFx-98; non-finite input never crashes playback — v1.0.0 (Phase 2)
- ✓ **EXTR-03** — 23 formats resolve, 15 layouts populated, Ambisonics round-trips to order 6 with corrected SN3D at orders 4-6 and one decoder — v1.0.0 (Phase 2)
- ✓ **VERIFY-01** — Ambisonics convention is AmbiX (ACN, SN3D, no Condon-Shortley phase), stated in code and docs — v1.0.0 (Phase 2)
- ✓ **EXTR-02** — binaural rendering (HRTFDatabase, PartitionedConvolver, BinauralRenderer) measured-correct; sample-based crossfades — v1.0.0 (Phase 3)
- ✓ **DATA-01** — five SOFA profiles embedded as BinaryData (Git LFS), resolved by shared folder, then embedded copy, then a reported error; one-call `setHRTFProfile` — v1.0.0 (Phase 3)
- ✓ **BUG-01** — Simple binaural path separates front, rear, up and down through a cue bank; ear-level front half unchanged — v1.0.0 (Phase 3)
- ✓ **BUG-02** — no artifacts at host blocks below 256 samples (sample-counted warm-up and crossfade) — v1.0.0 (Phase 3)
- ✓ **EXTR-04** — ADM-OSC (all five address forms over real UDP), the self-clocked 30 Hz sender with query replies, and all 13 trajectory shapes forward and reverse each move a `RenderEngine` object — v1.0.0 (Phase 4)
- ✓ **EXTR-05** — `SpatialMapComponent` drag, rings and elevation brightness proven in `SpatialCoreUITests` and through `RenderEngine`; demo screenshots approved at review — v1.0.0 (Phase 4)
- ✓ **DATA-02** — the map loads its own embedded fonts and every embedded font equals its source file; the SAVE PRESET title keeps its original bold font by user ruling (D-22 rejected) — v1.0.0 (Phase 4)
- ✓ **RTSF-01** — no allocation on the audio thread in Release, proven by the `[rt]` trap — v1.0.0 (Phase 5)
- ✓ **RTSF-02** — `TrajectoryEngine` final positions are lock-free atomics — v1.0.0 (Phase 5)
- ✓ **RTSF-03** — `HRTFDatabase` handle access thread-safe on the `setHRTFProfile` route (legacy route is detection-only) — v1.0.0 (Phase 5)
- ✓ **RTSF-05** — audio path verified lock-free and realtime-safe (macOS Apple Silicon, in-process) — v1.0.0 (Phase 5)
- ✓ **VERIFY-02** — `SharedFFTCache` safe across concurrent instances, TSan-clean (macOS Apple Silicon) — v1.0.0 (Phase 5)
- ✓ **INTG-01** — a consumer can submodule, link and build against SpatialCore; throwaway consumer harness — v1.0.0 (Phase 6)
- ✓ **INTG-02** — `getUnsuppliedGainsBlockCount()` stops `engineComputesGains` from silently no-opping — v1.0.0 (Phase 6)
- ✓ **CI-01** — CI-verified against JUCE 9.0.0 (macos-gate, Linux tests, Windows build); runs on a `v*` tag or by hand (D-20) — v1.0.0 (Phase 6)
- ✓ **TEST-01** — coverage tooling and a 93% engine rule in `scripts/coverage-targets.json` (94.19% measured on CI; not yet re-measured on the merged tree) — v1.0.0 (Phase 6)

RTSF-04 was struck 2026-08-10 as a non-finding (`ADMOSCReceiver` already bounds-checks) and is not counted.
Milestone v1.0.0 is a milestone label; the library stays 0.5.0 until OpenSpatialDelay 2.0.0 ships on it.

### Active

Next milestone not yet defined. Candidates: bump the SpatialCore pins in OpenSpatialDelay and
OpenSpatialPanner, or pick up backlog 999.1-999.4 and the v2 items in the closed-out requirements
archive. At the first release candidate: push a `v*` tag so CI runs, re-measure coverage on the
merged tree, run the realtime-safety check.

### Out of Scope

- **Release and org migration (REQ-create-org-repo, REQ-set-dual-license, REQ-tag-v1-0-0)** — belongs to milestone v3, not v1. v1 succeeds when OpenSpatialDelay ships on SpatialCore from the current private remote.
- **OpenSpatialDelay repository work** — seven requirements live in that repo. Tracked below as external dependencies, deliberately given no SpatialCore phase.
- **Future plugin suite (OpenSpatialReverb, OpenSpatialGranular, OpenSpatialPanner, OpenSpatialChorus)** — 2027 targets. One of them becomes the v2 milestone.
- **Full Windows and Linux verification** — CI now runs the suite on Linux (allocation-trap cases skip) and builds Windows (tests not run, D-08); the realtime-safety proof is macOS Apple Silicon only.
- **Custom / vendor-specific speaker layouts** — only the 15 built-in layouts. Generalizing the layout API is a post-v1 API addition.
- **Replacing the JUCE FFT dependency** — noted as a medium risk in CONCERNS.md; swapping FFT libraries is disproportionate to v1.

## External Dependencies

The v1 milestone is defined by OpenSpatialDelay shipping on SpatialCore, so v1 cannot close
without work in the OpenSpatialDelay repository. These are **not** SpatialCore phases and must not
be planned here.

| ID | Work (external repo) | Gates v1? |
|----|----------------------|-----------|
| REQ-osd-consume-spatialcore-submodule | Refactor OpenSpatialDelay to consume SpatialCore as a submodule instead of inline framework code | Yes |
| OSD Phase 2 follow-ups | Release notes for the audible Phase 2 changes (wider VBIP, EAR below-horizon panning, corrected 4OA-6OA levels, non-finite positions held or silenced); four OSD docs still describing VBIP as squared gains; delete OSD's local `evalSH` / VBAP copies at migration. List in `02-09-SUMMARY.md` | Yes — ships with the migration |
| OSD Phase 3 follow-ups | (1) An OSD release note for the audible Phase 3 changes: Simple (profile 0) now has rear head-shadow and up/down pinna cues, so sources behind, above or below sound different (ear-level front half unchanged, D-02), and a profile switch now fades for at least 4096 samples. (2) At migration delete OSD's HRTF loading glue (load / swap / timer / `useHRTF` derivation) in favour of `RenderEngine::setHRTFProfile` with `engineSelectsHRTF` + `engineComputesGains`. (3) At migration delete OSD's own HRTF BinaryData (`HRTFData`). (4) Check OSD's own delay line and pitch shifter at 32-128-sample blocks (the rest of OSD#234; SpatialCore's convolver half is done, BUG-02). (5) Note the duplicate-embedding size (about 2 x 58 MB) while both OSD and SpatialCore embed the profiles. Also bump OSD's own libmysofa pin from v1.3.2 to v1.3.5: OSD's pin makes SpatialCore's guard skip its fetch, so OSD otherwise ships the old parser. List in `03-10-SUMMARY.md` | Yes — ships with the migration |
| OSD Phase 4 follow-ups | (1) An OSD release note for the wire-visible changes: `ADMOSCSender` keeps its own 30 Hz clock, so OSD's 60 Hz timer gives the same rate as before (D-07); the first position of every object is always sent, even at zero (D-09), and every enabled object is sent once on connect and on re-enable (D-08b). (2) SpatialCore can answer ADM-OSC position queries (a message with no arguments). It replies to the plugin's configured send destination, not to the address of the device that asked, which is a stated deviation from the ADM-OSC text (D-08a, D-20). To answer, OSD overrides `ADMOSCReceiver::Listener::admPositionQueried` and calls `ADMOSCSender::queueReply` from its own state; until it does, queries are ignored and OSD compiles unchanged. (3) The receiver now ignores wrong-typed and non-finite values, clamps elevation and distance, and wraps an out-of-range azimuth; compliant senders are unaffected (D-21). (4) Map visuals and trajectory motion are unchanged: the map render is byte-identical under the SML look-and-feel (D-06), and Bounce and Line keep their reverse behaviour (D-19). (5) At migration delete OSD's own OSC send gate and its map font fallback. List in `04-08-SUMMARY.md` | Yes — ships with the migration |
| OSD Phase 5 follow-ups | (1) `TrajectoryEngine` final positions (`getFinalAz`, `getFinalEl`, `getFinalDist`) are now lock-free `std::atomic<float>`, read through the same getters; OSD keeps its own `TrajectoryEngine` copy until the migration. (2) The legacy HRTF route (`swapActiveRenderer`, `getPrepareRendererIndex`, `getBinauralRenderer` loads) is message-thread only, a call that breaks its thread contract still runs but is counted by `RenderEngine::getLegacyRouteWrongThreadCount()` and `HRTFDatabase::getMutationWhileInUseCount()`, and `setHRTFProfile` is the recommended route; the legacy route stays until the next major version (D-09). (3) An oversized host block (longer than `prepare()`'s size) is now rendered in full, in pieces, and no longer trips a Debug assertion; `getOversizedBlockCount()` counts it (D-11). (4) Share one `SMLLookAndFeel` across editors, since each instance loads its own typefaces (`Spatial-Media-Lab/OpenSpatialDelay#131`, D-12). (5) The realtime-safety proof is macOS (Apple Silicon) only; the Windows and Linux fallback-FFT spinlock follow-up (SharedFFTCache with JUCE's fallback FFT) belongs to Phase 6 (D-13). List in `05-08-SUMMARY.md` | Yes — ships with the migration |
| OSD Phase 6 follow-ups | (1) Set the "Set these flags" set from `docs/integration-guide.md` on every block (`engineComputesGains`, `engineDerivesDispatch`, `engineSelectsHRTF`, `engineFadesFormatSwitch`) and assert `RenderEngine::getUnsuppliedGainsBlockCount() == 0` in OSD's own tests (INTG-02). (2) SpatialCore's own tests are no longer built in a consumer build by default (`SPATIALCORE_BUILD_TESTS` is OFF unless SpatialCore is the top-level project). (3) Include the two umbrellas `<SpatialCore/SpatialCore.h>` and `<SpatialCore/SpatialCoreUI.h>` (D-02). (4) `examples/harness/` is the reference wiring; its OSD-like configuration (own libmysofa v1.3.2, `-ffast-math` on its own targets only) builds and passes the headless sound check (`HARNESS_OSD_LIKE=1`). (5) Under CMake 4, OSD's own libmysofa fetch needs `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`; never apply fast-math globally before `add_subdirectory(SpatialCore)`; on Apple SpatialCore sets `CMAKE_OSX_ARCHITECTURES` to arm64 in its own scope. (6) Windows: the fallback-FFT finding for OSD's Windows build (JUCE 9.0.0 `FFTFallback::perform` takes a `SpinLock`, `juce_FFT.cpp:115`; OSD's first release already ships the same `SharedFFTCache` design on Windows, verified in its git history 2026-10-24, so no new exposure; not measured, not fixed in v1), and a Windows build must supply zlib itself (CI: vcpkg `zlib:x64-windows-static-md`, `zs.lib`; OSD's own workflow uses `zlib:x64-windows-static`). (7) SpatialCore's CI now gates changes (`macos-gate`, `linux-tests`, `windows-build`, `coverage` on every push; `realtime-nightly` nightly once on `main`); the protection mode is recorded by Plan 06-10. List in `06-09-SUMMARY.md` | Yes — ships with the migration |
| OSP Phase 3 follow-up | After OpenSpatialPanner bumps its SpatialCore submodule, invert (not delete) the `[binaural][sc12]` assertions in its `Tests/DevFormatTests.cpp`: they pin the old front/back and elevation defect that BUG-01 fixed | No — v2 |
| REQ-osd-custom-sofa-import | Let users load their own HRTF files. Interim path (D-08): name a file like a built-in profile and place it in the system shared HRTF folder (`/Library/Application Support/Spatial Media Lab/HRTF/`); it overrides that built-in at the next switch | No — OSD v1.0 backlog |
| REQ-osd-adm-osc-settings-ui | In-plugin UI for OSC port configuration | No — OSD v1.0 backlog |
| REQ-osd-aax-format | Pro Tools AAX format support | No — OSD v1.0 backlog |
| REQ-osd-code-signing | macOS notarization for distribution | No — OSD v1.0 backlog |
| REQ-osd-github-migration | Move OpenSpatialDelay to the Spatial-Media-Lab org | No — v3 |
| REQ-osd-repoint-submodule-to-org | Point the OSD submodule at the public org repo | No — v3 |

Rows marked "ships with the migration" (Phase 2 and Phase 3) must reach OSD's release note or its migration commit; nothing in SpatialCore replaces that release note.

## Context

**Origin.** SpatialCore was extracted from OpenSpatialDelay v1.0, where ~68% of the codebase was
marked as reusable spatial audio infrastructure. Extraction separated framework code (algorithms,
HRTF, I/O, OSC, trajectories, UI) from delay-specific code (delay line, pitch shift, feedback,
wobble).

**The tree is partial until proven otherwise — this is a user ruling.** *(Discharged by evidence —
see STATE.md Decisions. Phases 1-2 verified their modules with executed tests and offline oracles;
later phases still verify by probe, not by reading.)*
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

- **Tech stack**: JUCE 9.0.0, C++17, CMake 3.22+, libmysofa v1.3.5 (FetchContent), zlib (system), Catch2 v3.7.1 (FetchContent) — fixed by the SPEC's root CMake contract.
- **Distribution**: Static library consumed via git submodule + `add_subdirectory(SpatialCore)` + `target_link_libraries(... PRIVATE SpatialCore)`. Consumers include only `<SpatialCore/SpatialCore.h>`.
- **Platform**: macOS is primary — arm64, deployment target 12.0. Windows (MSVC) and Linux are intended but not verified in v1.
- **Float semantics**: SpatialCore sources must not be compiled with `-ffast-math`, `-ffinite-math-only` or `/fp:fast` — the non-finite guards are `std::isfinite` tests that fast-math folds away. `src/Core/FloatSemanticsGuard.h` makes it a build error (Phase 2, WR-04).
- **Realtime**: No malloc, locks, or logging in any function reachable from `processBlock`. Algorithms are stateless; all computation state lives in context structs. Layout changes use dual-buffered atomic swap.
- **Frozen API**: The `SpatializationAlgorithm` interface cannot change without a major version bump. Same for anything else consumers link against.
- **Scale limits**: `MAX_SOURCES = 32` (SC-1), `MAX_SPEAKERS = 16`, `MAX_AMBI_ORDER = 6` (49 channels).
- **Licensing**: GPL-3.0. Not dual-licensed; no commercial licence.
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
| DR-9 | Consumers integrate via git submodule + CMake `add_subdirectory()` | ✓ Good — proven by the consumer harness (INTG-01, Phase 6) |
| DR-10 | JUCE 9.0.0 / C++17 / CMake 3.22+ / libmysofa 1.3.2 / zlib / Catch2 3.7.1 | ✓ Good — corrected 2026-08-10; DR-10 originally said JUCE 8, tree is pinned 9.0.0 at `CMakeLists.txt:33` |
| DR-11 | All library code lives in `namespace spatialcore` (DSP utilities in `spatialcore::DSP`) | ✓ Good |
| DR-12 | License GPL-3.0 (not dual-licensed; no commercial licence) | — Pending — LICENSE file lands in v3 |
| DR-13 | Publish under Spatial-Media-Lab with fresh squashed history | — Pending — gated on proof, not on a date. See the "Remote topology" decision row below. |
| DR-14 | Extraction sequenced after OpenSpatialDelay v1.0 ships | ⚠️ Revisit — the PRD's 2026-03-31 OSD deadline has passed and extraction is well underway; the sequencing assumption no longer holds |
| DR-15 | SpatialCore ships no BinaryData; consumers supply HRTF data | ✗ **Superseded by DR-5** — closed by user ruling |
| DR-16 | UI components take an abstract Listener interface, never a concrete processor pointer | ✓ Good |
| DR-17 | Every SML plugin is ~68% SpatialCore framework + ~32% plugin-specific DSP | ✓ Good — the product thesis |
| P2-D04 | Below-horizon VBAP on height layouts follows ITU-R BS.2127 (EAR): nadir caps plus one pair-pan wedge per ear-level pair; ear-level azimuth gaps of 179 degrees or more are bridged (WR-05) | ✓ Good — Phase 2; ear 2.1.0 pins on all 8 height layouts |
| P2-D06 | Playback never crashes on an unplaceable position: `RenderEngine` holds the last good az/el/distance per field; VBAP falls back to the largest-min-gain triplet, and to the nearest speaker at unity if that clamps to zero (hand-built partial triplet lists only); no asserts in `computeGains`. DBAP maps a non-finite distance to 0.5 and clamps finite distance to +-1000 (WR-08) | ✓ Good — Phase 2; D-06 amended 2026-10-04 (WR-03 / IN-14) |
| P2-D08 | One SH evaluator (`evalSH`) and one decoder (`AmbisonicsCodec::getDecodeMatrix`); AmbiX convention | ✓ Good — Phase 2 |
| P2-D14 | VBIP is textbook single-band VBIP (Pernaux, Boussard & Jot, DAFx-98); dual-band tracked in SpatialCore#20 | ✓ Good — Phase 2; audible change for OSD |
| P2-D18 | Coplanar-quad triplet tie-break above the horizon is not fixed in v1; tracked as SpatialCore#22 | — Pending |
| P2-VER | Phase 2 public API changes are additive (`VBAPTriplet::kind()`, `AmbisonicsCodec::getDecodeMatrix` now returns `bool`) — a minor-version bump at the next release (DR-6) | — Pending — next tag |
| P4-D20 | ADM-OSC position-query replies go to the sender's configured destination, never the packet source — a stated deviation from the ADM-OSC text | ✓ Good — Phase 4; in the OSD release-note row |
| P4-D19 | Bounce and Line reverse keep their OSD#100 behaviour as intentional exceptions to "reverse retraces forward" (D-13) | ✓ Good — Phase 4; pinned by tests |
| P4-D22 | SAVE PRESET title stays in its original system bold font; the DM Sans Bold change was rejected at review as too small | ✓ Decided 2026-10-05 — documented exemption in `[ui][fonts][spy]` |
| DR-18 | **Remote topology.** `AndrewRahman/SpatialCore` is the deliberate development remote, not an accident awaiting cleanup. Development stays on the personal remote until the pipeline is proven, for risk containment: `Spatial-Media-Lab/OpenSpatialDelay` is public and in use by real people right now, so migrating it onto an unproven SpatialCore could break a live plugin. Migration is **gated on proof, not on a date** — SpatialCore, OpenSpatialDelay-on-SpatialCore, and OpenSpatialPanner land on the organisation together once the process is proven. `docs/integration-guide.md` carries the working remote as the live instruction and labels the organisation URL as the post-proof destination. | ✓ Decided 2026-08-10 — resolves OQ-5 |
| D-18 | The coverage build and the realtime-safety check (`build-tsan`, `[rt]` trap) run only when preparing a public release; the 20 s `[rt]` trap stays in the everyday Release gate | ✓ Good — decided 2026-10-24 (user) |
| D-19 | The full Debug run of both executables runs only when preparing a public release; the everyday gate is Release build plus Release `ctest` | ✓ Good — decided 2026-10-24 (user) |
| D-20 | GitHub builds run only on a `v*` release tag or by hand; a branch push or pull request starts none, so `ci.yml` has had no run since 310fb93 | ✓ Decided 2026-10-24 — first real run waits for the first release candidate |
| D-VER | Milestone label v1.0.0 is not the library version: SpatialCore stays `0.x` (0.5.0) and becomes `1.0.0` when OpenSpatialDelay 2.0.0 ships on it; no git tag at milestone close | ✓ Decided 2026-10-24 |
| D-BP | `main` has no branch protection; the local gate is the only check before a push, and a push to `main` waits for the user's go-ahead | ✓ Decided 2026-10-24 |
| P6-DR9 | DR-9 (git submodule + `add_subdirectory`) proven by `examples/harness/`, a throwaway consumer built from a real submodule at a pushed commit | ✓ Good — Phase 6 (run on a release-tag or manual CI run) |
| P999.5-D01 | The Ambisonics output-format stream is plain AmbiX SN3D: `evalSH` x signal x the block's distance gain x tap fade. No near-field shelf and no max-rE on that stream; max-rE stays in SpatialCore's own decode (`AmbisonicsAlgorithm`, `[ambi-pin]`). Per-block SH interpolation kept; public API source-compatible. Why: the shelf was inverted against Daniel 2003 (low-frequency gain r / 1.5, no per-order power) and max-rE was applied a second time by IEM decoders; research in `.planning/quick/261024-mhh-re-check-the-ambisonics-maths-end-to-end/research/`. References SpatialCore#32 (open until openspatialdelay#22 ships and the pre-release speaker check passes) and the closed SpatialCore#21 (the order-change defect of the very filters now removed; its `[sc19]` tests stay as order-change guards). | ✓ Decided 2026-10-24, quick 261024-qlw |

</decisions>

---
*Last updated: 2026-10-24 after v1.0.0 milestone and quick task 261024-qlw (plain AmbiX Ambisonics output)*
