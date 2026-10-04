# Phase 3: Binaural Defects & HRTF Packaging - Research

**Researched:** 2026-10-04
**Domain:** C++17 / JUCE 9.0.0 spatial-audio library: binaural rendering defects (Simple-mode spectral cues, convolver small-block behaviour), embedded-resource packaging (`juce_add_binary_data`), a resolution chain for SOFA data, and a wait-free background profile switch.
**Confidence:** HIGH for everything measured or read in this tree this session; MEDIUM for the literature values (primary source read for Brown & Duda; Blauert / Hebrank & Wright only via search snippets, so the *design values come from measurements of the five shipped SOFA files*, with the literature as corroboration).

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Simple-mode elevation and front/back fix (BUG-01, SpatialCore#15)**
- **D-01:** Simple (Woodworth) mode gains lightweight **spectral cues**: a rear high-cut / head-shadow
  filter for sources behind the listener and a pinna-style elevation cue for sources above and
  below. Simple mode stays HRTF-free and cheap. Rejected: front/back only (leaves criterion 1
  unmet), routing Simple through the embedded KEMAR profile (CPU cost, Simple stops being its own
  mode), and leaving Simple mode left/right-only. Note: `computeBinauralGains` today returns gains
  and delays only, so the filter state lives on the engine's simple-binaural render path (or a
  new per-source filter), not in the stateless algorithm (Design Principle: stateless algorithms).
  — **Reversibility:** costly — this is an audible change to a shipping product; reverting after
  OSD releases on it changes users' sound a second time.
- **D-02:** **One behaviour, no legacy flag.** OSD does not get a switch back to the flat Simple
  sound. This follows the Phase 2 VBIP precedent (P2-D14): a defect fix that changes the sound is
  shipped with an OSD release note, not hidden behind an option. Add the release-note item to the
  OSD-side follow-ups in PROJECT.md External Dependencies.
- **D-03:** SpatialCore#15 closes on an **automated test**: on both the Simple path and the HRTF
  path, a source at elevation +90° is measurably different from 0°, and azimuth 0° from 180°.
  This replaces the deliberately unasserted DirectBinaural elevation/front-back cases in
  `tests/Algorithms/PanningLawTests.cpp`. A headphone listening check goes to the ROADMAP
  backlog next to 999.1/999.2. It is **not** a phase gate.

**Profile switching ownership (DATA-01, criterion 3 + 4)**
- **D-04:** **SpatialCore owns "switch to profile N" behind one call** on `RenderEngine` (e.g.
  `setHRTFProfile(int)`, name at planner's discretion). It runs the resolution chain, loads into
  the inactive renderer, and performs the click-free double-buffered swap. Consumers delete their
  glue. The reference for what moves into the library is OSD's
  `OpenSpatialDelayProcessor::loadHRTFProfileIntoRenderer` / `loadHRTFProfile` /
  `timerCallback` (OSD `Source/PluginProcessor.cpp` ~1495-1600). The existing escape hatches
  (`getBinauralRenderer`, `getPrepareRendererIndex`, `swapActiveRenderer`) stay public (D-02 from
  Phase 1/2: leaf classes stay public), so existing consumers are unaffected. This is an additive
  API (minor bump, DR-6).
  — **Reversibility:** costly — once OSD and OpenSpatialPanner delete their glue and call this,
  moving it back means re-adding consumer code in two repos.
- **D-05:** **Background loading.** The call returns immediately and never blocks the UI. The SOFA
  load (SADIE is 36 MB) runs off the message thread and off the audio thread. Audio keeps playing
  the current profile, then crossfades (existing `kRendererXfadeBlocks` machinery) when the new
  one is ready. If the user requests profiles faster than they load, the most recent request
  wins (Claude's discretion on the mechanism). The loader writes only the **inactive** renderer.
  It must not widen the existing `HRTFDatabase` `easyHandle` race (RTSF-03, owned by Phase 5).
- **D-06:** **A failed load never causes a dropout.** If a profile cannot be resolved anywhere, audio
  keeps the current profile (or Simple mode if nothing is loaded yet), and SpatialCore exposes a
  readable status (e.g. "profile 3 failed: file missing") that the plugin can show in its UI. This
  is how the roadmap's "loud error, never silent" is implemented: loud to the plugin and the user,
  not a mute. The status must be readable without the audio thread logging, allocating or locking
  (DR-1).
- **D-07:** **Profile numbering is OSD's existing numbering**, so saved sessions reopen on the same
  profile: 0 = Simple (Woodworth), 1 = `sadie_d2_ku100`, 2 = `cipic_subject_003`,
  3 = `hutubs_pp2`, 4 = `bernschuetz_ku100`, 5 = `mit_kemar_large_pinna` (source: OSD
  `loadHRTFProfileIntoRenderer` switch; `BinauralRenderer::isSimpleMode()` = profile 0; KEMAR LF
  shelf keyed on index 5 at `src/Binaural/BinauralRenderer.cpp:106`).
  — **Reversibility:** one-way — the indices are persisted in OSD session state, and renumbering
  silently changes the profile a saved session reopens with.

**Shared-folder rules (DATA-01, criterion 3)**
- **D-08:** **Same filename wins.** A file in the shared folder whose name exactly matches a built-in
  profile file (e.g. `hutubs_pp2.sofa`) replaces that profile. Any other `.sofa` in the folder is
  ignored in v1. User-supplied extra profiles are deferred.
- **D-09:** **A broken shared file falls back to embedded and reports it.** If a same-name file
  exists but fails to load (corrupt, or a Git LFS pointer stub), SpatialCore uses the embedded copy
  and reports a status ("shared file X unreadable, used built-in") through the same channel as D-06.
  A missing **folder** still falls back silently, as the roadmap says.
- **D-10:** **System folder only**: `/Library/Application Support/Spatial Media Lab/HRTF/` (macOS),
  `%ProgramData%\Spatial Media Lab\HRTF\` (Windows). No per-user `~/Library` folder in v1.
- **D-11:** The folder is checked **on every profile load** (not cached once at startup), so a newly
  dropped file takes effect on the next switch without restarting. The Windows path is
  implemented but unverified (v1 verification is macOS-only, PROJECT.md Out of Scope).

**Small-block correctness (BUG-02, `Spatial-Media-Lab/OpenSpatialDelay#234`, criterion 2)**
- **D-12:** **Automated match test is the bar.** Render identical input through the binaural path
  at 32, 64 and 128 samples, plus irregular/variable host block sizes (e.g. 37, alternating sizes),
  and require output that matches a 512-sample reference render to within float rounding. Runs in
  the suite every time. No DAW listening gate.
- **D-13:** **Comment on OSD#234, leave it open.** When Phase 3 passes, post a comment on
  `Spatial-Media-Lab/OpenSpatialDelay#234` stating that SpatialCore's convolver is proven clean at
  32-128 and that OSD's delay line and pitch shifter (the issue's other two named causes) are
  still unchecked. Posting is an outward action, so confirm the exact text with the user at the
  time (CLAUDE.md: outward-facing actions are confirmed first).

**Cross-profile level measurement (EXTR-02, added 2026-10-04 follow-up)**
- **D-14:** **Measure now, change later.** Phase 3 adds a test that measures each of the 5 built-in
  profiles' **perceived loudness** (pink noise rendered through each profile and averaged over
  directions, ear-weighted loudness in the K-weighted / ITU-R BS.1770 sense) and reports the
  spread. **No change to the built-ins' level in Phase 3**, so OSD's sound is unchanged. The
  current rule (`src/Binaural/BinauralRenderer.cpp:43-79`) equalises raw broadband energy over
  6 reference directions to `targetRMS = 1/sqrt(irLen)`. It is not frequency-weighted, and no
  test today checks that the built-ins actually land at the same level. The test's pass/fail
  tolerance is set **after** the first measurement and approved by the user. Until then it
  records and prints the numbers. The measured spread feeds the future loudness standard
  (Deferred → custom HRTFs).

### Claude's Discretion
- **Filter design and strength for D-01** (user: "you decide"). Pick literature-grounded values
  (e.g. Brown & Duda structural model for head shadow / pinna cues). The test must show a clear
  measurable difference while Simple mode stays recognisably close to today's sound at ear level,
  front. Record the chosen values and their source in the plan.
- The embedded BinaryData namespace/target name. It must not collide with OSD's own
  `juce_add_binary_data(HRTFData ...)` / `namespace HRTFData` while OSD still has it. Follow the
  `SpatialCoreUIFontData` precedent at `CMakeLists.txt:233-236` (e.g. `SpatialCoreHRTFData`).
- The API name and the status-reporting shape (enum/atomic/struct) for D-04/D-06/D-09.
- How `SPATIALCORE_EMBED_ALL_HRTF=OFF` selects KEMAR-only (profile 5) at build time.
- Fixing the `tests/Binaural/HutubsPP2Tests.cpp:47` Debug-only golden failure (Phase 2 deferred
  item; Phase 3 owns the binaural goldens).
- Whether to address the `SharedFFTCache` "Leaked objects detected: 1 FFT" false positive here or
  leave it for Phase 5.
- **Doc correction:** CLAUDE.md "Build System" and "Critical Rules" say HRTF profiles are not
  BinaryData and that OSD resolves them bundle-relative (`<bundle>/Contents/Resources/HRTF/`).
  Introduced by `87cb7a3` (CR-01, 2026-07-06), which predates the OQ-6 ruling. It was already
  wrong about OSD: OSD v1.0.0 embeds all 5 via `juce_add_binary_data(HRTFData ...)` at its
  `CMakeLists.txt:50-58`, and the shipped `.vst3` has no `Resources/HRTF/`. Once DATA-01 lands,
  rewrite those CLAUDE.md lines (and `docs/integration-guide.md` if affected) to describe the
  resolution chain and embedding.

### Deferred Ideas (OUT OF SCOPE)
- **Custom HRTFs: later milestone (ROADMAP backlog 999.3).** The user has their own measured
  `.sofa` file. Decisions already made for that phase (2026-10-04):
  - Import happens through the plugin UI: the **last item in the binaural profile dropdown is
    "Import…"**. It is not a drop-a-file-in-a-folder workflow, which is only a prototype path.
  - Import **copies** the file into a personal library so it stays available in future sessions.
  - The user **types a display name at import**; delivered filenames are unreadable.
  - Session opened without the custom file: play a built-in (KEMAR) + warning; the session keeps
    the custom choice so it returns when the file is present.
  - **Auto-level** custom profiles to a **new perceived-loudness standard** that SpatialCore
    must define (none exists today; see D-14). Built-ins move onto it in the same phase (an
    audible change, so it needs an OSD release note).
  - Open for that phase: whether the dropdown/import widget is a shared `SpatialCoreUI`
    component or per-plugin (leaning shared so every SML plugin sees the same library).
  - **Interim prototype path that works after Phase 3:** name the custom file like a built-in
    (e.g. `hutubs_pp2.sofa`) and place it in `/Library/Application Support/Spatial Media Lab/HRTF/`
    (D-08). It replaces that profile.
- **Per-user HRTF folder** (`~/Library/Application Support/Spatial Media Lab/HRTF/`): subsumed by
  the import library above.
- **Headphone listening check** of the new Simple-mode cues: ROADMAP backlog 999.4.
- **OSD-side:** release note for the Simple-mode sound change; delete OSD's HRTF glue and its own `HRTFData` BinaryData at migration; check OSD's delay line and pitch shifter at small block sizes (rest of OSD#234). Track in PROJECT.md External Dependencies.
- **OpenSpatialPanner-side:** invert (not delete) the `[binaural][sc12]` assertions after the submodule bump.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| EXTR-02 | Binaural rendering produces measured-correct output; `PartitionedConvolver` and `BinauralRenderer` get dedicated test files (+ D-14 loudness measurement) | F1/F7 give the oracles and tolerances (direct-convolution oracle at 2e-7, engine steady-state 3.3e-5); F8 gives a working K-weighted loudness harness and a preview of the spread; F5/F4 are latent `BinauralRenderer` defects the dedicated test will meet |
| DATA-01 | HRTF resolves shared folder -> embedded -> reported error; `SPATIALCORE_EMBED_ALL_HRTF` (default ON); `loadFromBinaryData()`; click-free engine-owned profile switch | F9-F11 (BinaryData cost, JUCE paths, LFS guard), Pattern 2/3 (resolver, loader protocol), F2 (the existing swap clicks at small blocks and on Simple transitions, so the xfade fix is a DATA-01 prerequisite), Security Domain |
| BUG-01 | Simple path has no elevation / front-back cue (SpatialCore#15) | F3 (current code, measured defect), F7 (metric + thresholds measured on the shipped HRTFs), Pattern 1 (cue filter design, values from the shipped data) |
| BUG-02 | Small-block artifacts (`Spatial-Media-Lab/OpenSpatialDelay#234`) | F1 (overlap-add is already correct; the real residual is block-count-based warm-up/crossfade, prototype proves the fix) |
</phase_requirements>

## Summary

Four findings reshape the plan, all measured in this tree this session. **(1) BUG-02 is half-fixed.** The overlap-add decoupling the roadmap points at (`PartitionedConvolver.cpp:120-168`) is correct: a standalone convolver matches a double-precision direct convolution to 2e-7 at block sizes 32, 64, 128, 512, 37 and irregular mixes. But a *moving* source at 32-sample blocks produces an output step 4.6x larger than at 512 (0.0688 vs 0.0149), because the convolver's IR warm-up (`kWarmupBlocks = 1`) and crossfade (`kCrossfadeBlocks = 4`) are counted in *blocks*, so at 32 samples the new IR slot is faded in after 160 samples while the IRs are 128-558 samples long. Making both sample-based (warm-up at least `irLen` samples, crossfade at least 2048 samples) removed the excess completely (0.0150 at 32, 64, 128 and 512) and broke no existing test. **(2) Criterion 4 fails today**, independent of the loader: the engine's renderer crossfade (`kRendererXfadeBlocks = 8`) has the same block-count flaw (a profile swap at 32-sample blocks steps 8.2x above the steady-state level, 3.2x at 64), and transitions to/from Simple (profile 0) are a hard path switch with no crossfade (Simple to KEMAR steps 15-20x). The same sample-floor fix on the renderer crossfade made the 5 to 5 swap click-free at 32/64/128/512; the Simple transitions need a real design (Pattern 4). **(3) BUG-01 is confirmed and the HRTF path is already fine**: engine-level impulse-response comparison gives 0.00 dB RMS between front/back and front/overhead on Simple, and 2.9-6.3 dB RMS on all five HRTF profiles. The fix is a fixed-coefficient, blend-weighted filter bank on the engine's Simple path whose target curves were taken from the *measured* median-plane response differences of the five shipped SOFA files, which agree with Blauert / Hebrank-Wright. **(4) DATA-01 mechanics are cheap and proven**: `juce_add_binary_data` over the 5 files builds in 17 s (Release, 8 jobs), generates 185 MB of C++ and a 61 MB static library; `HRTFDatabase::loadFromMemory` already exists and is exactly what OSD ships with; a name-based `getNamedResource` makes the KEMAR-only (`OFF`) build need no `#if` in the lookup.

Three latent defects will surface in the dedicated tests and DATA-01's user-supplied files make them reachable: the audio-thread `resize` at `BinauralRenderer.cpp:162` fires today for shipped KEMAR (IR 558 > 512 pre-allocation); the 64-sample ITD delay line (`kITDBufferSize = 64`) wraps absolute onset delays (SADIE reports up to 123 samples at 48 kHz); and the byte-exact FNV golden is fragile across build types (HUTUBS Debug vs Release differ by 4.8e-7 on a 6.15 peak, ~1 ulp). The first is cheap to fix in Phase 3 (size the scratch buffers in `setProfile()` on the worker thread); the second is a decision for the user (Open Question 2).

**Primary recommendation:** Land the tests first (convolver oracle, steady-state block-size match, moving-source and profile-switch continuity metrics, Simple distinguishability metric), watch them fail red on the current tree for the reasons measured here, then make four changes: (a) sample-based warm-up/crossfade floors in `PartitionedConvolver` and the engine renderer crossfade, (b) a blend-weighted fixed-biquad cue bank on `renderSimpleBinauralWoodworth`, (c) `SpatialCoreHRTFData` embedding plus a `HRTFProfileResolver`, (d) a worker-thread `setHRTFProfile()` that publishes through a claim-by-audio-thread mailbox and an opt-in `RenderBlockContext` flag that derives `useHRTF` from the active renderer and crossfades Simple transitions.

## Architectural Responsibility Map

This is a native audio library, not a web app; tiers map to threads and layers.

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Simple-mode spectral cues (rear/up/down filter bank) | Audio thread, `RenderEngine::renderSimpleBinauralWoodworth` | `Core` pure function for weights/targets | Filters are stateful, so they cannot live in the stateless frozen `SpatializationAlgorithm` (DR-7); weights are a pure function of position |
| Convolver warm-up/crossfade timing | Audio thread, `PartitionedConvolver::process` | — | Timing is a property of the slot state machine |
| Renderer crossfade + Simple path crossfade | Audio thread, `RenderEngine::renderDirectBinauralHRTF` / `renderBlock` | — | Must see both signals in the same block |
| SOFA resolution chain (shared folder -> embedded -> error) | Worker thread, new `HRTFProfileResolver` | `HRTFDatabase::loadFromBinaryData/loadFromMemory/loadFromFile` | Does file IO and a 36 MB parse; never the audio or message thread |
| Embedded profile bytes | Link-time data (`SpatialCoreHRTFData` static lib) | `src/Binaural/EmbeddedHRTF.cpp` (only TU that includes the generated header) | Keeps the generated header out of public headers so consumers need no include path |
| Profile request / latest-wins / status | Message thread (caller), worker (executor) | Atomic status word read from any thread | D-05, D-06; status must be readable without lock/alloc |
| Publish of a loaded renderer | Audio thread claims a mailbox | Worker sets it | Keeps the active-renderer index single-writer on the audio side and removes the swap-ack race (Pitfall 3) |
| Shared folder path | `HRTFProfileResolver` (platform switch) | Test seam to override | `/Library/Application Support` is root-owned, so tests cannot write it |
| Doc surfaces | CLAUDE.md, README, integration guide, 2 skills | PROJECT.md External Dependencies | HRTF wording is stale in 5 files (list in Pattern 7) |

## Standard Stack

No new third-party dependency is introduced.

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 9.0.0 [VERIFIED: `CMakeLists.txt:25` `GIT_TAG 9.0.0`; test run banner "JUCE v9.0.0"] | `juce_add_binary_data`, `juce::Thread`, `juce::File::getSpecialLocation`, `juce::dsp::IIR::Coefficients` | Already the framework; `SpatialCoreUIFontData` is the in-repo precedent for embedding |
| libmysofa | v1.3.2 [VERIFIED: `CMakeLists.txt:49` `GIT_TAG v1.3.2`] | SOFA parse; `mysofa_open_data` is what `loadFromMemory` calls | Existing. See Open Question 4 on v1.3.5 hardening |
| Catch2 | v3.7.1 [VERIFIED: `tests/CMakeLists.txt:5`] | Tests; `WARN()` prints without failing (used for D-14 record-only) | Existing |

### Supporting
| Item | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| `juce::dsp::IIR::Coefficients<float>::makeHighShelf/makePeakFilter` | JUCE 9.0.0 | Build the fixed cue-bank biquads once in `prepare()` | D-01 cue bank; `getMagnitudeForFrequency` doubles as the test oracle for the design |
| `juce::dsp::FFT` (`SharedFFTCache`) | JUCE 9.0.0 | Third-octave analysis in tests | Metric helper (Code Examples) |
| `juce::Thread` + `juce::WaitableEvent` (juce_core) | JUCE 9.0.0 | The single profile-loader worker | `juce_core` is already linked PUBLIC in standalone mode and INTERFACE in consumed mode [VERIFIED: `CMakeLists.txt:134-144`] |
| ITU-R BS.1770-4 K-weighting, 48 kHz coefficients | Rec. BS.1770-4 | D-14 loudness | Test-local biquads; valid only at 48 kHz [CITED: ITU-R BS.1770-4 via search result; coefficients below match the spec table] |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Cue bank as fixed biquads + blend weights | Brown & Duda 32-tap sparse pinna FIR with angle-dependent fractional delays | The FIR is time-varying (delay-line interpolation as the source moves), only defined for the frontal half-space (their elevation range is -90..+90 on a frontal-hemisphere measurement set, p.482), and has no rear model. A fixed-coefficient bank has no zipper noise and an exact identity at front/ear-level |
| 3-renderer rotation (TripleBufferIndex style) | Mailbox + two renderers | A third renderer costs another ~40 MB for SADIE (parsed SOFA + KD-tree) [ASSUMED size]; the mailbox gives the same safety with the existing two. Revisit only if a "supersede while crossfading" requirement appears |
| `juce::ThreadPool` | one `juce::Thread` | One job type, latest-wins, so a single long-lived worker is simpler and joins cleanly in `~RenderEngine` |

**Installation:** none. New CMake surface (verified to configure and build in `/tmp/scratch/build-embed`, then reverted):
```cmake
option(SPATIALCORE_EMBED_ALL_HRTF "Embed all 5 HRTF profiles (OFF: KEMAR only)" ON)
if(SPATIALCORE_EMBED_ALL_HRTF)
    set(_sc_hrtf_sources HRTF/mit_kemar_large_pinna.sofa HRTF/sadie_d2_ku100.sofa
        HRTF/cipic_subject_003.sofa HRTF/hutubs_pp2.sofa HRTF/bernschuetz_ku100.sofa)
else()
    set(_sc_hrtf_sources HRTF/mit_kemar_large_pinna.sofa)
endif()
juce_add_binary_data(SpatialCoreHRTFData
    HEADER_NAME "SpatialCoreHRTFData.h"
    NAMESPACE   SpatialCoreHRTFData
    SOURCES     ${_sc_hrtf_sources})
target_link_libraries(SpatialCore PRIVATE SpatialCoreHRTFData)   # static lib: link-only to consumers
target_compile_definitions(SpatialCore PUBLIC SPATIALCORE_EMBEDS_ALL_HRTF=$<BOOL:${SPATIALCORE_EMBED_ALL_HRTF}>)
```
**Version verification:** n/a (no registry packages). JUCE and libmysofa are pinned by tag in `CMakeLists.txt`; the tool versions on this machine are in Environment Availability.

## Package Legitimacy Audit

This phase installs **no external packages** (no npm / PyPI / crates). JUCE, libmysofa and Catch2 are existing pinned FetchContent dependencies. `package-legitimacy check` is not applicable. The only candidate bump (libmysofa v1.3.2 -> v1.3.5) is an Open Question, not a recommendation.

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

## Architecture Patterns

### System Architecture Diagram

```
 message thread                     worker thread (1, owned by RenderEngine)        audio thread (renderBlock)
 ──────────────                     ─────────────────────────────────────────       ─────────────────────────
 setHRTFProfile(n) ──► requested_ (atomic, latest wins) ──► wake ─┐
 getHRTFStatus()  ◄── statusWord_ (atomic, packed enum)           │
                                                                  ▼
                              n == 0 ? mark Simple ready : resolve(n):
                                1. sharedFolder()/<file>  exists?
                                     yes -> loadFromFile ──ok──► use (source = Shared)
                                                           └fail─► note "shared unreadable"
                                2. embedded getNamedResource(<file>)
                                     found -> loadFromMemory ──ok──► use (source = Embedded[, warning])
                                3. else status = Failed(NotFound) ; keep current ; STOP
                                                                  │
                          wait until a renderer is FREE           │ (never touch active / xfade-from)
                          renderer.prepare(sr, maxBlock); db.load…; renderer.setProfile(n)
                          size scratch buffers for irLen (kills BinauralRenderer.cpp:162 resize)
                                                                  ▼
                          readyMailbox_.store(idx, release)  ──────────────────────────────┐
                          status = Loaded(n, source)                                       ▼
                                                                         top of renderDirectBinauralHRTF / renderBlock:
                                                                           if (!xfading) idx = readyMailbox_.exchange(-1)
                                                                           idx>=0 -> activeRendererIndex.store(idx)
                                                                                     start crossfade from old renderer
                                                                           xfade done -> rendererFree_[old] = true (release)
                                                                         Simple<->HRTF: render both paths, crossfade (Pattern 4)
                                                                         Simple path: x -> [rear|up|down cue bank, blend weights] -> pan gains
```

### Recommended Project Structure
```
include/SpatialCore/Binaural/
├── HRTFDatabase.h            # + loadFromBinaryData(int profile, float sr)  (thin wrapper, REQUIREMENTS.md DATA-01)
├── HRTFProfile.h             # NEW: profile table (index -> filename, display name), status enums/struct, shared-folder API
├── HRTFProfileResolver.h     # NEW: resolve(profile, HRTFDatabase&, sr) -> result; injectable shared folder
include/SpatialCore/Core/
├── SimpleBinauralCues.h      # NEW: pure weights(az, el) + filter target constants (no state)
src/Binaural/
├── EmbeddedHRTF.cpp          # NEW: only TU that includes <SpatialCoreHRTFData.h>; getNamedResource lookup
├── HRTFProfileResolver.cpp   # NEW
src/Engine/RenderEngine.cpp   # loader worker, mailbox, cue bank, Simple<->HRTF crossfade
tests/Binaural/
├── PartitionedConvolverTests.cpp   # NEW (EXTR-02)
├── BinauralRendererTests.cpp       # NEW (EXTR-02)
├── HRTFEmbeddedTests.cpp           # NEW (embedding + resolution chain)
├── BinauralCueTests.cpp            # NEW (BUG-01; both paths)
├── ProfileLoudnessTests.cpp        # NEW (D-14, record-only)
tests/Engine/ProfileSwitchTests.cpp # NEW (criterion 4)
tests/Binaural/BinauralMetrics.h    # NEW shared helpers (third-octave metric, K-weighting, click metric)
```
`tests/CMakeLists.txt` lists sources by hand; every new test file is one line there, so plans that add files must not run in the same wave without one owner for that file.

### Pattern 1: Simple-mode cue bank (BUG-01, D-01)

**What:** Per source, on the mono signal *before* the pan gains, run three fixed full-strength filter branches and blend them with smoothed weights: `y = x + wRear*(R(x)-x) + wUp*(U(x)-x) + wDown*(D(x)-x)`. All three filters are unity at DC. With every weight 0 the output is bit-identical to today (`y == x`), so the ear-level front sound and, because the weights below are also 0 there, the lateral sound are unchanged.

**Weights** (pure function of the sanitised position, computed once per block, then linearly interpolated per sample exactly like `prevBinauralGains`):
```
wRear = max(0, -cos(az) * cos(el))        // 1 directly behind at ear level; 0 at front, at the sides, overhead, underfoot
wUp   = max(0,  sin(el))
wDown = max(0, -sin(el))
```
The stored per-source previous weights live next to `prevBinauralGains` in `RenderEngine.h`; filter state is 2 floats per biquad per source, preallocated, reset in `prepare()`. Run the filters every block for every live source even at zero weight so the state is never stale.

**Why blend instead of re-computing coefficients:** changing biquad coefficients every block excites the state (zipper noise); a fixed-coefficient filter with an interpolated mix cannot. The magnitude at an intermediate weight is `|1 + w(H-1)|`, not dB-linear, which is fine for a cue (measured below).

**Target curves: measured, not guessed.** I loaded all five shipped SOFA files and measured the mean-power response of the median-plane HRIRs in 1/3-octave bands (L and R averaged) relative to the front direction (az 0, el 0). Mean over the 5 profiles, dB [VERIFIED: scratch probe this session, `HRTFDatabase::getInterpolatedHRIR`, 48 kHz]:

| Band Hz | 200 | 250 | 315 | 400 | 500 | 630 | 800 | 1k | 1.25k | 1.6k | 2k | 2.5k | 3.15k | 4k | 5k | 6.3k | 8k | 10k | 12.5k | 16k |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| back (az180) - front | -0.6 | -0.8 | -0.8 | -0.5 | -0.2 | -0.2 | 0.9 | 1.9 | 2.5 | -0.5 | -2.3 | -2.7 | -2.6 | -4.2 | -4.7 | -3.3 | -1.2 | -4.6 | -9.3 | -13.7 |
| up (el90) - front | 0.2 | 0.1 | -0.3 | -0.5 | -1.3 | -2.8 | -2.0 | -0.8 | 1.2 | -1.5 | -3.8 | -3.7 | -3.0 | -3.8 | -1.9 | 2.4 | 8.8 | 1.8 | -6.5 | -5.4 |
| down (el-45) - front | -1.1 | -1.2 | -1.0 | -0.3 | 0.8 | 0.8 | 1.5 | 2.4 | 2.5 | 1.5 | -0.2 | -1.6 | -2.1 | -3.6 | -3.7 | -5.8 | 3.6 | 0.9 | -3.8 | -1.9 |

These agree with the literature: Blauert's directional bands put "front" at 200-500 Hz and 3-5 kHz, overhead at a 1/4-octave region around 7-9 kHz, and rear at ~1 kHz and above 10 kHz [CITED: Blauert 1997 as summarised by search results; primary not read, LOW]; Hebrank & Wright 1974 report a 7-9 kHz peak and ~10 kHz cut-off for higher elevations [CITED: JASA 56(6):1829-1834 as summarised by search results; primary not read, LOW]. The shipped-data table is the authority for the numbers below. Brown & Duda 1998 is the structural precedent (a pinna model whose reflection delay grows as the source moves below, Table I coefficients rho = 0.5, -1, 0.5, -0.25, 0.25, A = 1,5,5,5,5, B = 2,4,7,11,13 samples at 44.1 kHz, `tau = A*cos(theta/2)*sin(D*(90-phi)) + B`) [VERIFIED: read from the paper PDF this session, pp.484-485]. Their model is frontal-hemisphere only and their reported mean elevation-matching error was 24.3 deg vs 12.0 deg for measured HRIRs (Table II), so it is not a better target than the shipped HRTFs. The Brown-Duda *head-shadow* filter is a lateral/ILD cue that is symmetric front/back, so it cannot supply the rear cue; the rear cue must come from the pinna-shadow high-frequency loss.

**Starting values (validated analytically with `getMagnitudeForFrequency`; executor tunes against the tests):**

| Branch | Stages (48 kHz design; recompute in `prepare()` for the actual rate) | Band response dB (200 Hz ... 16 kHz) | RMS design / max band | RMS error vs measured mean |
|---|---|---|---|---|
| Rear R | high-shelf -4 dB @ 2.5 kHz Q 0.7; high-shelf -8 dB @ 10 kHz Q 0.7; peak +2 dB @ 1.2 kHz Q 0.8 | 0.1 .. +1.7 (1.2k) .. -4.0 (5k) .. -11.6 (16k) | 4.49 / 11.6 | 1.70 |
| Up U | peak +10 dB @ 8 kHz Q 2.0; peak -4 dB @ 3 kHz Q 0.8; high-shelf -5 dB @ 11 kHz Q 0.7 | 0 .. -3.5 (3k) .. +8.4 (8k) .. -4.3 (16k) | 2.70 / 8.4 | 1.45 |
| Down D | peak -6 dB @ 5.5 kHz Q 1.5; peak +2.5 dB @ 1.2 kHz Q 0.7 | 0 .. +2.3 (1.25k) .. -5.2 (5k) .. -0.1 (16k) | 2.04 / 5.2 | 1.86 |

DC gain of each is 0.000 +/- 0.001 dB [VERIFIED: probe]. A first, weaker "Up" (peak +8 dB, no high shelf) measured only 2.20 dB RMS, too close to the 2.0 dB test threshold, which is why the v2 values above include the shelf. Record these values and the table's source in the plan (Claude's-discretion requirement).

**What not to do:** do not apply the Woodworth `leftDelaySamples/rightDelaySamples`. They are computed by `DirectBinauralAlgorithm.cpp:54-62` but `renderSimpleBinauralWoodworth` (`RenderEngine.cpp:389-421`) reads only `leftGain/rightGain` [VERIFIED: grep shows no reader of `DelaySamples` in `src/Engine`]. Wiring the ITD in would be a second audible change D-01 did not ask for.

### Pattern 2: Resolution chain (DATA-01, D-08..D-11)

```cpp
// include/SpatialCore/Binaural/HRTFProfile.h  (names are discretionary)
inline constexpr int kNumHRTFProfiles = 5;            // profiles 1..5; 0 = Simple (D-07)
struct HRTFProfileInfo { const char* file; const char* resource; const char* name; };
inline constexpr HRTFProfileInfo kHRTFProfiles[6] = {   // index == D-07 numbering
    { nullptr,                      nullptr,                    "Simple" },
    { "sadie_d2_ku100.sofa",        "sadie_d2_ku100_sofa",        "Immersive" },
    { "cipic_subject_003.sofa",     "cipic_subject_003_sofa",     "Natural" },
    { "hutubs_pp2.sofa",            "hutubs_pp2_sofa",            "Precise" },
    { "bernschuetz_ku100.sofa",     "bernschuetz_ku100_sofa",     "Spatial" },
    { "mit_kemar_large_pinna.sofa", "mit_kemar_large_pinna_sofa", "Studio Reference" } };
```
Filenames and display names verbatim from OSD `Source/PluginProcessor.cpp:20-27` and `:1519-1528` [VERIFIED: read this session; e.g. `case 1:  sofaData = HRTFData::sadie_d2_ku100_sofa;` at :1519 and `case 5:  sofaData = HRTFData::mit_kemar_large_pinna_sofa;` at :1527]. The generated symbol for each file is its filename with `.` -> `_` [VERIFIED: generated `SpatialCoreHRTFData.h` in the scratch build].

```cpp
// Resolve order per D-08/D-09/D-11; folder re-checked on EVERY call (no caching).
Result resolve (int profile, HRTFDatabase& db, float sr, const juce::File& sharedFolder)
{
    const auto& info = kHRTFProfiles[profile];
    bool sharedBroken = false;
    const juce::File f = sharedFolder.getChildFile (info.file);      // exact filename (D-08); case policy: Open Question 5
    if (f.existsAsFile())
    {
        if (db.loadFromFile (f, sr))      return { Source::SharedFolder, Problem::None };
        sharedBroken = true;                                          // corrupt / LFS stub (D-09); loadFromBytes() left db unloaded
    }
    int size = 0;
    if (const char* d = SpatialCoreHRTFData::getNamedResource (info.resource, size))   // in EmbeddedHRTF.cpp only
        if (db.loadFromMemory (d, size, sr))
            return { Source::Embedded, sharedBroken ? Problem::SharedFileUnreadableUsedBuiltIn : Problem::None };
    return { Source::None, sharedBroken ? Problem::SharedUnreadableNoBuiltIn : Problem::NotFound };
}
```
A missing *folder* is just `existsAsFile() == false` (no warning), which is exactly the D-09 "missing folder falls back silently" rule. `loadFromBytes` calls `unload()` first and leaves the database unloaded on any failure [VERIFIED: `src/Binaural/HRTFDatabase.cpp:43-62`], so the embedded retry on the same `HRTFDatabase` is clean. libmysofa rejected every corrupt input I tried without crashing (LFS stub 74 B, zero length, truncated at 100 KB and 50%, 200 KB random, six 40-bit-flip mutations): all returned `false` [VERIFIED: probe this session, Debug build, no sanitizer].

`getNamedResource` returns `nullptr` for a name not compiled in, so with `SPATIALCORE_EMBED_ALL_HRTF=OFF` (KEMAR only) profiles 1-4 fall out of the chain to `NotFound` with no preprocessor branching [VERIFIED: generated header lists `getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes)`; OFF case is a source-list change only, not built, so tag the OFF link-test as a Wave-0 check].

### Pattern 3: Background loader + publish protocol (D-04, D-05, D-06)

**Why the existing `swapActiveRenderer()` is not enough.** `swapActiveRenderer()` (`RenderEngine.h:302-306`) flips `prepareRendererIndex_` immediately, but the audio thread only *notices* the swap at the top of its next block (`RenderEngine.cpp:245-254`) and keeps using the old renderer as the crossfade source for `kRendererXfadeBlocks` blocks. `rendererXfadeActive_` is still `false` in the window between the store and that next block. OSD's timer guards on `rendererXfadeActive_` only (`PluginProcessor.cpp:1581-1582`), so there is a small window where the loader could overwrite the renderer the audio thread is about to use as the fade source. A background loader that runs for seconds makes that window wider, so the protocol must close it:

1. `std::atomic<int> readyRenderer_ {-1}` (mailbox) and `std::atomic<bool> rendererFree_[2]` (init: `[1]=true`, `[0]=false`).
2. Worker, per request: pick a free renderer `i` (`rendererFree_[i]`), set `rendererFree_[i]=false`, `prepare` + load + `setProfile`, then `readyRenderer_.store(i, release)`. If a newer request arrives while a result is still unclaimed, the worker takes the slot back with `readyRenderer_.exchange(-1)`: if that returns `i`, it still owns `i` and reloads into it; if it returns `-1` the audio thread already claimed it, so wait for a free renderer. This is the "most recent request wins" mechanism and it works while the audio is idle (a test with no blocks rendering still completes).
3. Audio thread, once per block before rendering, **only when not already crossfading**: `int r = readyRenderer_.exchange(-1, acquire); if (r >= 0) { activeRendererIndex.store(r, release); begin crossfade from the previous active; }`. When the crossfade ends: `rendererFree_[old].store(true, release)`.
4. Only the audio thread writes `activeRendererIndex` in this mode; `swapActiveRenderer()` and `getPrepareRendererIndex()` keep working for legacy consumers but mixing them with `setHRTFProfile` is unsupported (document it).

Also required:
- `prepare()` must quiesce the worker (stop, join or cancel the in-flight load) before it calls `binauralRenderers[i].prepare()`, then re-request the current profile at the new sample rate. Otherwise a sample-rate change leaves IRs resampled for the old rate and races the worker. `~RenderEngine` joins the worker.
- The worker sizes the renderer's scratch buffers for the loaded IR length inside `setProfile()` (see Pitfall 4) and publishes only after the database and all 24 convolvers are fully prepared. The release store on `readyRenderer_` / acquire exchange on the audio side is the happens-before edge that makes the non-atomic `HRTFDatabase::loaded/irLength` safe to read.
- Status: a single `std::atomic<uint32_t>` packing `{requested profile, active profile, state (Idle/Loading/Loaded/Failed), source (None/Simple/Shared/Embedded), problem (None/InvalidIndex/NotFound/SharedUnreadableUsedBuiltIn/SharedUnreadableNoBuiltIn)}` plus a free function `describe(status)` that builds the human string on the *reader's* thread. Nothing on the audio thread logs, allocates or locks (DR-1). Failure behaviour: `Failed` leaves `readyRenderer_` untouched, so the audio thread keeps the current profile (Simple if none), satisfying D-06.
- Test hooks: `waitForHRTFProfileIdle(int timeoutMs)` (message-thread/test only) and an injectable shared folder (`setSharedHRTFFolderForTesting(const juce::File&)`), because `/Library/Application Support` is `root:admin 755` on this machine [VERIFIED: `ls -ld`] and does not contain `Spatial Media Lab` [VERIFIED].

### Pattern 4: Click-free crossfades, including Simple (criterion 4)

Two prototypes were applied to the tree, measured, and reverted (`git checkout`; tree clean). The full diff is in `/tmp/scratch/prototype-sample-based-xfade.diff` (not committed) and is summarised here.

*PartitionedConvolver (`src/Binaural/PartitionedConvolver.cpp`):* add `int stateSampleCount`; Warmup ends when `stateBlockCount >= kWarmupBlocks && stateSampleCount >= irLen`; the crossfade length is `max(kCrossfadeBlocks * blockSize, 2048)` samples, progress `= stateSampleCount / length`, finish on `stateSampleCount >= length`. **Fix the length once at the Warmup->Crossfading transition and keep it in a member**, and base it on the call size rather than the prepared size (the prototype used the prepared `blockSize`, which would slow the fade for a host that prepares at 2048 but calls at 256, and a per-call-recomputed length lets `progress` go backwards when call sizes vary).

*Engine renderer crossfade (`src/Engine/RenderEngine.cpp:332-374`):* same idea, `max(kRendererXfadeBlocks * numSamples, 4096)` samples; 4096 equals today's length at 512, so behaviour at >= 512 is unchanged.

Measured, Debug, KEMAR, 0.5-amplitude sine, max sample-to-sample step after settling [VERIFIED: probe this session]:

| Scenario | block 32 | 64 | 128 | 512 |
|---|---|---|---|---|
| Moving source (180 deg over 2 s), current tree | 0.0688 | 0.0192 | 0.0167 | 0.0149 |
| Moving source, convolver prototype | 0.0150 | 0.0150 | 0.0150 | 0.0149 |
| Profile swap KEMAR -> SADIE, ratio of worst step to max(pre, post) steady, current tree | 8.19 | 3.23 | 1.15 | 1.00 |
| Profile swap, both prototypes | 1.00 | 1.00 | 1.00 | 1.00 |
| Simple -> KEMAR (consumer-style `useHRTF = !isSimpleMode()`), current tree | — | 15.45 | — | 19.59 |
| KEMAR -> Simple, current tree: steps are fine but RMS dips to 0.083 vs 0.158 at 512 (a dropout) | — | no dip | — | dip |

The full existing suite still passed under both prototypes (208 passed, 1 failed: the known HUTUBS Debug golden).

**Simple transitions need a design the prototypes do not provide.** Today the consumer derives `useHRTF = !activeRenderer.isSimpleMode()` per block (`OSD PluginProcessor.cpp:4451`), which flips the render *path* instantly while the renderer crossfade only blends two HRTF renderers. Because the new API makes the swap asynchronous the consumer can no longer know which renderer is active when the block starts (the tear class SC-16 fixed for layouts). Recommended, following the SC-13/SC-16 opt-in pattern: a new default-`false` `RenderBlockContext` flag (e.g. `engineSelectsHRTF`) that makes the engine derive `useHRTF` from its own active renderer, and, while a Simple<->HRTF crossfade is in progress, render **both** the Woodworth path (into a scratch buffer) and the HRTF path and blend with the existing cos/sin gains. The Woodworth path must run exactly once per block when it participates (it advances `prevBinauralGains` and the cue-bank state). With the flag left `false`, behaviour is byte-for-byte today's.

### Pattern 5: Platform folder resolution

```cpp
juce::File sharedHRTFFolder()   // D-10; resolved on every call (D-11)
{
   #if JUCE_MAC
    return juce::File ("/Library/Application Support/Spatial Media Lab/HRTF");
   #elif JUCE_WINDOWS
    return juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)   // CSIDL_COMMON_APPDATA = C:\ProgramData
               .getChildFile ("Spatial Media Lab").getChildFile ("HRTF");
   #else
    return {};   // Linux: embedded only (no D-10 folder defined); an empty File never exists
   #endif
}
```
Do **not** build the macOS path from `commonApplicationDataDirectory`: on the Mac it resolves to `/Library`, not `/Library/Application Support` [VERIFIED: `juce_Files_mac.mm:211` `case commonApplicationDataDirectory: resultPath = "/Library"; break;`]; on Windows it maps to `CSIDL_COMMON_APPDATA` [VERIFIED: `juce_Files_windows.cpp:722`]; on Linux it is `/opt` [VERIFIED: `juce_Files_linux.cpp:137`]. A literal macOS path keeps D-10 exact. An empty `juce::File` is safe: `existsAsFile()` is false.

### Pattern 6: Moving the LFS guard into CMake

The only LFS-stub guard today is a CI shell step (`.github/workflows/ci.yml:19-35`); a freshly cloned consumer that skipped `git lfs pull` would silently embed 130-byte pointer text because `juce_add_binary_data` embeds whatever the file contains. Add a configure-time check when embedding is on: read the first 8 bytes as hex and require `894844460d0a1a0a` (the HDF5 signature; verified on `HRTF/mit_kemar_large_pinna.sofa`: `8948 4446 0d0a 1a0a`) and `message(FATAL_ERROR ...)` naming the file and `git lfs pull`.
```cmake
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${f}" _sig LIMIT 8 HEX)
if(NOT _sig STREQUAL "894844460d0a1a0a")
    message(FATAL_ERROR "SpatialCore: ${f} is not a real SOFA/HDF5 file (Git LFS pointer?). Run: git lfs pull")
endif()
```

### Pattern 7: Doc surfaces that go stale (D-discretion "Doc correction")
- `CLAUDE.md:87` (HRTF data / bundle-relative), `CLAUDE.md:112` (Critical Rules, "not BinaryData"), and the Key Interfaces section (add the new profile call and the opt-in flag).
- `README.md:35` HRTF Profiles bullet (add resolution chain, CMake option).
- `docs/integration-guide.md` (HRTF profile loading rows near :280 and the `spatialcore::HRTFDatabase hrtfDb;` sample at :83).
- `docs/development-roadmap.md:3` (status note says "runtime Git-LFS HRTF loading (NOT BinaryData)").
- `.claude/skills/spatial-audio-dsp/SKILL.md` (:147/:168-174 Path B, :259 "No spectral coloring" is now false for Simple, :530) and `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` (:91, :137 "background threaded (60Hz timer)").
- `PROJECT.md` External Dependencies: OSD release-note item for the Simple-mode change (D-02) and "delete OSD `HRTFData` + glue at migration".
- Also stale: `REQUIREMENTS.md` BUG-01 cites `DirectBinauralAlgorithm.cpp:26`; the line is now `:34` after Phase 2's guard (see F3).

### Anti-Patterns to Avoid
- **Re-computing biquad coefficients per block from the live position** (zipper noise); use fixed filters plus interpolated weights.
- **Putting cue state in `DirectBinauralAlgorithm`.** DR-7 freezes the interface and the algorithm is stateless.
- **Letting the loader decide when the audio thread switches.** Only the audio thread claims the mailbox.
- **Caching the shared-folder result at startup** (violates D-11).
- **Including `<SpatialCoreHRTFData.h>` from a public header.** Consumers do not get that include path (PRIVATE link), and it would leak the generated namespace.
- **Counting crossfades in calls.** Every block-count constant in this phase's code is a latent small-block bug.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Embedding the files | A custom `xxd`/`objcopy` rule | `juce_add_binary_data` (repo precedent, `CMakeLists.txt:236`) | Proven for fonts; handles per-file `.cpp`, namespace, `getNamedResource` |
| SOFA parse / validation | Own HDF5 reader or "is this a SOFA" heuristics | `mysofa_open_data` through `HRTFDatabase::loadFromBytes` | Rejected all 11 corrupt inputs tried; a pre-check would only duplicate it |
| Platform shared folder | `getenv("PROGRAMDATA")` string-building | `juce::File::getSpecialLocation` on Windows, literal path on macOS | JUCE already maps `CSIDL_COMMON_APPDATA`; `commonApplicationDataDirectory` is wrong on macOS |
| Biquad coefficients | RBJ cookbook by hand | `juce::dsp::IIR::Coefficients::makeHighShelf/makePeakFilter` | Also gives `getMagnitudeForFrequency` for the design test |
| Third-octave spectrum | A new analyser | `juce::dsp::FFT` and a band-mean (Code Examples) | 25 lines; the probe used it |
| Latest-request-wins queue | A mutex + condition variable + queue | `std::atomic<int>` request + `juce::WaitableEvent` + the mailbox | Fits DR-1 (no locks on the audio side) and has no queue to drain |
| Loudness | A new psychoacoustic model | BS.1770 K-weighting biquads (2 stages) | D-14 explicitly names it |

**Key insight:** every hard part here already exists in the tree in some form (embedding precedent, `loadFromMemory`, dual-slot crossfade, opt-in context flags, triple-buffer layout handoff). The work is making the existing time constants sample-based, closing one race window, and adding a bounded amount of new DSP.

## Common Pitfalls

### Pitfall 1: Calling the BUG-02 fix "already done"
**What goes wrong:** the in-tree comment at `PartitionedConvolver.cpp:122-126` says the decoupling fixed OSD#234, and the standalone convolver really is clean (2e-7). Pinning only that with a test would mark the defect closed while moving sources still step 4.6x at 32 samples.
**Why:** warm-up/crossfade are counted in blocks (`PartitionedConvolver.h:83-84`), so at 32 samples `1 + 4` blocks = 160 samples < the 128-558 sample IRs: the incoming slot is faded in before it has convolved a full IR.
**Avoid:** put the *moving-source continuity* test (Validation row BUG-02-c) next to the steady-state match test. Both must be red before the fix.
**Warning signs:** a max step at block boundaries above the 512-block reference.

### Pitfall 2: Comparing block sizes without a settle window
**What goes wrong:** the first N samples legitimately differ: the first block's gain/ITD interpolation spans `numSamples` (so it ramps over 32 vs 512 samples) and the renderer crossfade is block-count based on today's tree. Measured on the current tree: early max |diff| 0.3 (Simple) and 0.6-1.2 (KEMAR), steady-state 0 (Simple) and 3.3e-5 (KEMAR).
**Avoid:** assert after `settle = 16384` samples on the current tree, or after `4096 + 512` once the crossfade is sample-based; assert the steady-state tolerance below. A longer-than-tolerance difference *during* the first 4096 samples is not a defect.

### Pitfall 3: Overwriting a renderer the audio thread still needs
See Pattern 3. The `rendererXfadeActive_` guard alone leaves a window. The mailbox plus `rendererFree_` flags remove it.

### Pitfall 4: `BinauralRenderer.cpp:162` audio-thread `resize` fires for shipped KEMAR
**What goes wrong:** `prepare()` pre-allocates `max(maxBlockSize, 512)` floats (`BinauralRenderer.cpp:17`), KEMAR's IR is 558 samples, so for any `maxBlockSize <= 557` the first `updateSourceHRIR` hits `jassertfalse` and resizes on the audio thread. Reproduced: four `JUCE Assertion failure in BinauralRenderer.cpp:162` in one run of a 512-block KEMAR probe. DATA-01's user-supplied files make longer IRs reachable.
**Avoid:** size `convTmpL/R` to `max(blockSize, irLen)` inside `setProfile()` (worker thread). This closes the `:162` site only; the `:207` site and the `jassertfalse`-guarded pattern are RTSF-01 (Phase 5).

### Pitfall 5: ITD delay line shorter than the delays it is given
`kITDBufferSize = 64` (`BinauralRenderer.h:97`) and the read index wraps with `& (kITDBufferSize - 1)` (`BinauralRenderer.cpp:266`). `getAlignedHRIR` returns the *absolute* onset delay, not the interaural difference: measured max across a direction sweep [VERIFIED: probe]: SADIE 123 samples @48k (245 @96k, 490 @192k); KEMAR 63 @48k, 126 @96k; CIPIC 58, HUTUBS 50, Bernschuetz 43 @48k. At 48 kHz SADIE's delays all wrap by 64, preserving the *difference* only while both ears sit on the same side of a multiple of 64, and KEMAR is exactly at the limit. At 88.2 kHz and above even KEMAR wraps. This is pre-existing shipping behaviour (OSD's sound), so Phase 3 should **pin it with a characterisation test at 44.1/48 kHz and not change it** (Open Question 2).

### Pitfall 6: Byte-exact golden checksums
`HutubsPP2Tests.cpp:47` compares an FNV hash of raw float bytes. HUTUBS az 90 HRIR differs between Debug and Release by 4.8e-7 on a 6.147 peak (~1 ulp, 7.8e-8 relative) [VERIFIED: probe]. Replace the hash for HUTUBS (ideally all five, identical pattern) with a tolerance fingerprint: delays (integers 18 and 48 for HUTUBS az 90 aligned), energy L/R and peak value/index with `WithinRel(1e-5)` (~100x the observed difference).

### Pitfall 7: Windows/Linux and the embedded vs shared test seam
Tests cannot write `/Library/Application Support/...`. Use the injected folder with a temp directory. On Linux CI `sharedHRTFFolder()` is empty, so every shared-folder test must use the injected folder, not the platform default.

### Pitfall 8: Duplicate embedding in the OSD transition
While OSD keeps its own `HRTFData` and also links SpatialCore, the binary carries 2 x 58 MB. The namespaces differ (`HRTFData` vs `SpatialCoreHRTFData`), so it links, but note the size in the OSD migration follow-up.

### Pitfall 9: Debug libmysofa trace noise
Debug builds print libmysofa parse traces (`.../dataobject.c:1095: OHDR message type ...`) to stderr for every load, hundreds of lines. Filter in test greps (`grep -v '^/Users'`); do not treat as failure. Also expect `JUCE Assertion failure in juce_LeakedObjectDetector.h:116` at exit (the known `SharedFFTCache` false positive, Deferred Items).

## Code Examples

### Third-octave distinguishability metric (the BUG-01 test oracle)
```cpp
// Source: scratch probe this session (tests/Binaural/ZZScratchResearch2.cpp, deleted; copy in /tmp/scratch/tests-saved)
// Impulse response of the engine at a fixed direction: settle, then one impulse. LTI once the
// per-block gain/ITD ramps and the crossfade have finished (settle = 12 blocks of 512).
std::vector<float> engineIR (RenderEngine& eng, float az, float el, bool hrtf /*useHRTF*/);   // helper: loop renderBlock
// Metric: 1/3-octave band powers of the IR (200 Hz .. 16 kHz, 20 bands, FFT 4096 @ 48 kHz).
float rmsBandDiffDb (const std::vector<float>& a, const std::vector<float>& b, float& maxBandDb);
```
Threshold rule: "distinguishable" = RMS band difference >= 2.0 dB **and** max band difference >= 4.0 dB. Measured on the current tree [VERIFIED: probe]:

| Path | front vs back RMS / max dB | front vs up90 RMS / max dB | front vs up45 RMS / max dB |
|---|---|---|---|
| Simple (today) | 0.00 / 0.00 | 0.00 / 0.00 | 0.00 / 0.00 |
| SADIE | 2.90 / 6.24 | 3.05 / 7.09 | 3.04 / 5.73 |
| CIPIC | 5.32 / 17.35 | 4.45 / 13.32 | 5.29 / 12.28 |
| HUTUBS | 4.26 / 9.70 | 4.16 / 11.10 | 4.76 / 11.34 |
| Bernschuetz | 5.86 / 20.56 | 4.12 / 11.32 | 4.29 / 15.72 |
| KEMAR | 6.30 / 19.01 | 3.59 / 8.61 | 7.97 / 18.57 |

So the 2.0 / 4.0 thresholds sit below the weakest shipped HRTF pair (2.90 / 5.73) and above the 1 dB spectral-level difference commonly used as a just-noticeable level change [ASSUMED: ~1 dB level JND from profile-analysis literature; not verified this session]. The Simple cue bank at full weight designs to 4.49 / 11.6 (rear) and 2.70 / 8.4 (up). The time-domain L1 difference front vs back on Simple is 6e-8, i.e. byte-identical, which is the SpatialCore#15 defect stated numerically. OpenSpatialPanner's `[binaural][sc12]` pin asserts exactly this and must be inverted there (consumer-side, Deferred).

### K-weighting for D-14 (48 kHz)
```cpp
// Source: ITU-R BS.1770-4 Table 1/2 coefficients (48 kHz) [CITED via search; matches the spec]
struct Bq { double b0,b1,b2,a1,a2,z1=0,z2=0; double p(double x){ double y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y; return y; } };
Bq shelf { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 };
Bq rlb   { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };
// loudness (LKFS) = -0.691 + 10*log10( mean(L_k^2 + R_k^2) ), channel weights 1.0 for L and R
```
Preview of what D-14 will print (192 renders, 8 azimuths x 4 elevations, 1 s of deterministic Kellet pink noise after a 1 s settle, engine path, `distGain = 1`, Release) [VERIFIED: probe]:

| Profile | Mean LKFS | Min | Max |
|---|---|---|---|
| Simple (not comparable: includes its own distance gain) | -22.17 | -23.33 | -20.70 |
| 1 SADIE | -14.08 | -16.27 | -11.95 |
| 2 CIPIC | -13.15 | -16.14 | -10.42 |
| 3 HUTUBS | -13.56 | -17.14 | -10.98 |
| 4 Bernschuetz | -12.37 | -14.27 | -11.02 |
| 5 KEMAR | -11.73 | -14.78 | -9.97 |

Spread among the five built-ins: **2.35 LU** (SADIE quietest, KEMAR loudest, KEMAR includes the +12 dB low-shelf compensation). The real test should use fewer renders (e.g. 12 directions, 0.5 s) to stay in a few seconds. Use `WARN()` to print without failing and assert only finiteness and a deliberately loose sanity bound until the user approves a tolerance (D-14).

### Steady-state block-size match (D-12)
Reference = 512-sample blocks. Plans: `{32}`, `{64}`, `{128}`, `{37}`, `{1,7,300,512,3,64}`, `{32,512,32,64,512}`. Static source (az 50, el 20), 60000 samples of seeded noise, compare after the settle window. Measured max |diff| after 16384 samples [VERIFIED: probe, Debug]: Simple **0**, KEMAR **3.2e-5 to 3.3e-5** on all six plans. Standalone convolver vs double-precision direct convolution (IR 558 random taps, 20000 samples): **1.6e-7 to 2.1e-7** on all seven plans. Suggested bounds: convolver 2e-6 (10x), engine 1e-4 (3x; re-measure in Release, the K-weighted LF shelf at 200 Hz on a float biquad is the likely source of the 3e-5).

### Skeleton: profile-switch continuity metric
```cpp
// max |x[i+1]-x[i]| around the swap vs max(pre-swap steady, post-swap steady). Post-swap level
// legitimately differs between profiles (KEMAR -> SADIE is louder at 440 Hz), so compare against
// the larger steady value, not the pre-swap one. Pass: ratio <= 1.5 at 32/64/128/512 and RMS never
// dips below 0.7 x min(preRms, postRms) in the window (the Simple -> HRTF dip was 0.53).
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Block-counted crossfade/warm-up | Sample-based with a floor | This phase | Removes the small-block artifact class |
| Consumer glue loads SOFA on the message thread (OSD `timerCallback`) | Engine-owned worker + status | This phase | UI never blocks on a 36 MB parse |
| Raw `HRTFData` per plugin | Shared `SpatialCoreHRTFData` + shared folder | This phase | One embed, user override possible |
| libmysofa 1.3.2 | 1.3.5 "Harden HDF/SOFA parser against malformed input" [CITED: github.com/hoene/libmysofa/releases, fetched this session] | 2026 | Relevant once user-supplied files are in scope (Open Question 4) |

**Deprecated/outdated:** CLAUDE.md lines 87 and 112, the `docs/development-roadmap.md:3` status note, and `REQUIREMENTS.md` BUG-01's `DirectBinauralAlgorithm.cpp:26` (now `:32-34`).

## Runtime State Inventory

Not a rename/refactor/migration phase in the data sense, but DATA-01 moves where HRTF data comes from. Explicit answers:

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None in SpatialCore. OSD persists the profile *index* in session state (`hrtfProfile`), numbering preserved by D-07 | none (code only); do not renumber |
| Live service config | None | none — verified, SpatialCore has no external services |
| OS-registered state | The shared folder `/Library/Application Support/Spatial Media Lab/HRTF/` does not exist on the dev machine [VERIFIED: `ls`] and is `root:admin`-owned parent | none to create; installer is SUITE-01 |
| Secrets/env vars | None | none |
| Build artifacts | `build/` and `build-release/` carry stale objects from scratch probes (gitignored). `OSD`'s own `HRTFData` target remains until its migration | `cmake -S . -B build` re-run is enough; OSD removal is OSD-side |

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | ~1 dB is the commonly cited spectral-level JND, so 2.0 dB RMS / 4.0 dB max is "clearly distinguishable" | Code Examples (metric) | Thresholds may be stricter or looser than perception; they are anchored to measured HRTF values, so the test is still meaningful |
| A2 | A third renderer would cost ~40 MB for SADIE | Alternatives | Only affects rejecting the 3-renderer option |
| A3 | Blauert / Hebrank-Wright figures as summarised by search snippets (primary papers not read) | Pattern 1 | Design values come from the measured shipped data; literature is corroboration only |
| A4 | World-readable `/Library/Application Support/...` is readable from sandboxed AU/VST3 hosts | Pattern 5 | A sandboxed host could fail to read the shared folder and fall back to embedded; v1 embeds, so impact is nil until SUITE-01 |
| A5 | Windows `%ProgramData%` subfolders created by an installer are not user-writable by default | Security Domain | A standard user could plant a same-name SOFA; mitigation is the same parse hardening |
| A6 | Case-insensitive filesystems (macOS default, Windows) match `Hutubs_PP2.sofa` to `hutubs_pp2.sofa` via `existsAsFile` | Open Question 5 | D-08 says "exactly matches"; behaviour differs by filesystem |
| A7 | Release-build tolerances will be no looser than Debug's | Code Examples | Re-measure in `build-release`; bounds above carry 3-10x margin |
| A8 | The Simple<->HRTF dual-path crossfade is in scope of criterion 4 | Pattern 4 | If the user scopes criterion 4 to numbered profiles only, drop the dual-path render and the `engineSelectsHRTF` flag |

## Open Questions (RESOLVED)

All six were answered after this research by the user (CONTEXT.md D-13, D-15..D-18) or by the
plans; each carries its resolution below.

1. **Is Simple (profile 0) part of "switching HRTF profile" for criterion 4?** — **RESOLVED by D-15**
   - What we know: D-07 numbers Simple as profile 0; consumers' dropdown includes it; today Simple->HRTF steps 15-20x and HRTF->Simple dips ~5 dB.
   - Unclear: whether the user wants the dual-path crossfade (extra render work for ~85 ms per transition, a new opt-in flag).
   - Recommendation: include it (A8). It is the only way the roadmap sentence "no click, pop, or dropout" is true for every entry in the dropdown.
   - **Resolution:** D-15 — yes, Simple <-> HRTF switching crossfades the two render paths behind a new default-off `RenderBlockContext` flag (`engineSelectsHRTF`, Plan 03-09).

2. **The 64-sample ITD line (Pitfall 5) and KEMAR's audio-thread allocation (Pitfall 4).** — **RESOLVED by D-16**
   - Known: ITD wrap is pre-existing OSD behaviour; fixing it changes shipped timing. The allocation fix is internal and safe.
   - Recommendation: fix the allocation here; pin the ITD behaviour with a 44.1/48 kHz characterisation test and file a SpatialCore issue for >= 88.2 kHz (Phase 5 or v2). Needs the user's yes/no.
   - **Resolution:** D-16 — the user said yes: pin, don't fix (characterisation test in Plan 03-01, issue filed after approval in Plan 03-11); the KEMAR audio-thread resize is fixed in Plan 03-03.

3. **Loader worker and the Phase 5 `easyHandle` race (RTSF-03).** — **RESOLVED by the Plan 03-06 invariant and the Plan 03-07 proof**
   - Known: the worker writes only a free renderer's `HRTFDatabase`, and the audio thread reads it only after the mailbox acquire, so the race is not widened. The *pre-existing* race (a legacy consumer calling `setProfile` on the active renderer) is untouched.
   - Recommendation: state this invariant in the header and test it under TSAN if available (Phase 5 owns the TSAN gate).
   - **Resolution:** adopted as recommended, within D-05 ("must not widen the existing easyHandle race"): Plan 03-06 makes the loader write only a free renderer (must-have truth and header doc), and Plan 03-07 proves it with a live render thread and a ThreadSanitizer run; the pre-existing race stays with Phase 5.

4. **libmysofa v1.3.2 vs v1.3.5.** — **RESOLVED by D-17**
   - Known: v1.3.5 hardens the parser against malformed input; DATA-01 makes user files reachable (admin-installed in v1).
   - Unclear: whether a bump moves any golden. 
   - Recommendation: keep v1.3.2 in Phase 3 (zero-regressions), add the bump as a separate evaluated task or a Phase 5 item, and record the exposure.
   - **Resolution:** D-17 supersedes the recommendation — upgrade to v1.3.5 in Phase 3, evaluated in a disposable copy, with user sign-off on any moved golden before re-baselining (Plan 03-08).

5. **Case sensitivity of "same filename wins" (D-08).** Pick: compare case-sensitively against the directory listing (`findChildFiles`) for identical behaviour on macOS and Windows, or accept the filesystem's rules. Recommendation: exact case via directory listing, since D-08 says "exactly matches". — **RESOLVED by D-18:** follow the filesystem (a normal file-exists lookup; no directory-listing exact-case comparison), superseding the recommendation and the word "exactly" in D-08 (Plan 03-05).

6. **OSD#234 comment (D-13).** — **RESOLVED by D-13 and the Plan 03-11 approval gate:** the exact text is shown to the user and posted only on approval; the issue stays open. Text must be confirmed with the user at posting time. Draft: "SpatialCore's PartitionedConvolver is verified clean at 32/64/128 and irregular block sizes (direct-convolution oracle, 2e-7), and its IR warm-up/crossfade is now sample-based. The delay line and pitch shifter named above are still unchecked."

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | all builds | yes | 4.4.4 | — |
| Apple clang | build | yes | 21.0.0 (arm64, M4 Pro) | — |
| git-lfs | real `.sofa` bytes | yes | 3.8.0; all 5 files are real HDF5 [VERIFIED: signature + sizes 1.2-36.6 MB] | CMake guard (Pattern 6) fails the configure otherwise |
| JUCE 9.0.0 / libmysofa / Catch2 sources | offline configure | yes, in `build/_deps` | — | `FETCHCONTENT_SOURCE_DIR_*` as in 02-VALIDATION |
| gh CLI | OSD#234 comment, issue lookups | yes, logged in as AndrewRahman | 2.102.0 | — |
| Python 3 | Phase 2 reference oracles only | yes | 3.14.8 | not needed in Phase 3 |
| `/Library/Application Support/Spatial Media Lab/HRTF/` | shared-folder runtime | no (does not exist, parent is root-owned) | — | injected folder in tests; the missing folder is the normal state |
| Windows toolchain | `%ProgramData%` path | no | — | implemented, unverified (D-11, PROJECT.md Out of Scope) |

**Missing dependencies with no fallback:** none.
**Missing dependencies with fallback:** shared folder (tests inject), Windows (unverified by design).

## Validation Architecture

(`workflow.nyquist_validation` is absent from `.planning/config.json`, so it is enabled.)

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.7.1 via FetchContent; CTest via `catch_discover_tests` |
| Config file | `tests/CMakeLists.txt` (explicit source list, new files added by hand) |
| Quick run command | `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target SpatialCoreTests -j8 && ./build/tests/SpatialCoreTests "[tag]"` |
| Full suite command | `./build/tests/SpatialCoreTests` (Debug) and `cmake --build build-release --target SpatialCoreTests -j8 && ctest --test-dir build-release/tests --output-on-failure` (Release; the `/tests` directory matters, per 02-VALIDATION) |

Baseline before this phase: Debug 208/209 (only `HutubsPP2Tests.cpp:47`), Release 209/209 [VERIFIED: STATE.md; Debug reproduced this session].

### Phase Requirements -> Test Map
| Req / Criterion | Behavior | Test Type | Automated Command | File Exists? |
|---|---|---|---|---|
| BUG-01 / C1 (Simple) | Simple: front vs up90 and front vs back each >= 2.0 dB RMS and >= 4.0 dB max by the engine-IR metric; plus up45, down45; front/ear-level unchanged (`y == x` bit-exact at az 0, el 0 and at az 90) | engine integration | `./build/tests/SpatialCoreTests "[bug01][simple]"` | Wave 0: `tests/Binaural/BinauralCueTests.cpp` |
| BUG-01 / C1 (HRTF) | Same metric on all 5 profiles (measured minimum 2.90 / 5.73) | engine integration | `./build/tests/SpatialCoreTests "[bug01][hrtf]"` | Wave 0 same file |
| BUG-01 | Cue weights: pure function table (front, back, side, up, down, NaN guard); DC gain of each branch within 0.01 dB; no non-finite output for az/el sweep | unit | `"[bug01][weights]"` | Wave 0 |
| BUG-01 | Replace the "No elevation or front/back assertion" comment case in `PanningLawTests.cpp:737-742` | unit | `"[panning-law][directbinaural]"` | exists, edit |
| BUG-02 / C2 | Convolver vs double-precision direct convolution <= 2e-6 for plans {32},{64},{128},{512},{37},{1,7,300,512,3,64},{32,512,32,64,512} | unit | `"[convolver][blocksize]"` | Wave 0: `tests/Binaural/PartitionedConvolverTests.cpp` |
| BUG-02 / C2 | Engine steady-state match vs 512 reference <= 1e-4 after settle, Simple and KEMAR | engine integration | `"[bug02][steady]"` | Wave 0: `tests/Engine/ProfileSwitchTests.cpp` or `BinauralRendererTests.cpp` |
| BUG-02 / C2 | Moving-source continuity: max step at 32/64/128 <= 1.2 x the 512 value (red today: 4.6x / 1.3x / 1.1x) | engine integration | `"[bug02][moving]"` | Wave 0 |
| BUG-02 | IR update during Warmup/Crossfading defers and applies (pending IR), irregular block sizes | unit | `"[convolver][pending-ir]"` | Wave 0 |
| DATA-01 / C3 | Embedded bytes == on-disk bytes (size + FNV) for each embedded profile | unit | `"[hrtf-embed][bytes]"` | Wave 0: `tests/Binaural/HRTFEmbeddedTests.cpp` |
| DATA-01 / C3 | All 5 profiles render through the engine with shared folder = nonexistent dir and `SPATIALCORE_HRTF_DIR` unused (IR lengths 256, 218, 279, 128, 558) | engine integration | `"[hrtf-resolve][embedded]"` | Wave 0 |
| DATA-01 / C3 | Same-name file in injected folder wins (copy CIPIC as `hutubs_pp2.sofa`, expect IR length 218 not 279, source = Shared) | integration | `"[hrtf-resolve][shared]"` | Wave 0 |
| DATA-01 / C3 | Missing folder falls back silently (source = Embedded, problem = None) | integration | `"[hrtf-resolve][missing]"` | Wave 0 |
| DATA-01 / D-09 | LFS-stub / truncated / garbage same-name file falls back to embedded and reports `SharedUnreadableUsedBuiltIn` | integration | `"[hrtf-resolve][broken]"` | Wave 0 |
| DATA-01 / D-06 | Unresolvable profile (index 9; or OFF build profile 1) -> `Failed`, audio continues on previous profile, output finite and non-silent | engine integration | `"[hrtf-switch][failure]"` | Wave 0 |
| DATA-01 / D-11 | File dropped after the first load is used on the next switch with no restart | integration | `"[hrtf-resolve][d11]"` | Wave 0 |
| DATA-01 / D-05 | Latest request wins: request 1, 2, 3, 4 back-to-back -> final active profile is 4; call returns in < 50 ms | engine integration | `"[hrtf-switch][latest-wins]"` | Wave 0 |
| DATA-01 | `prepare()` while a load is in flight does not crash and the profile ends up loaded at the new rate | engine integration | `"[hrtf-switch][prepare]"` | Wave 0 |
| C4 | Profile swap 1<->5, 5<->3 at block 32/64/128/512: step ratio <= 1.5 and no RMS dip < 0.7x (red today: 8.2 / 3.2 at 32/64) | engine integration | `"[hrtf-switch][click]"` | Wave 0 |
| C4 | Simple <-> each HRTF profile, same metric, with the opt-in flag (red today: 15-20x) | engine integration | `"[hrtf-switch][simple]"` | Wave 0 |
| C4 | Concurrent: one thread renders a moving sine, the main thread requests profiles; finite output, ratio bound, no deadlock; run under TSAN when available | engine stress | `"[hrtf-switch][threads]"` | Wave 0 |
| EXTR-02 / C5 | `BinauralRenderer`: profile 0 leaves `sourceConvReady` false and passes silence; `setProfile` normalisation (`avgRMS` -> `targetRMS`); KEMAR shelf active only for profile 5; `invalidateSources`/`reset` clear ITD and shelf state; ITD characterisation at 44.1/48 kHz | unit | `"[renderer]"` | Wave 0: `tests/Binaural/BinauralRendererTests.cpp` |
| EXTR-02 / D-14 | Loudness spread printed with `WARN()`; assert finite and spread < 20 LU only | engine integration | `"[loudness]"` | Wave 0: `tests/Binaural/ProfileLoudnessTests.cpp` |
| Housekeeping | HUTUBS (and the other 4) goldens as tolerance fingerprints; passes Debug and Release | unit | `"[binaural][golden]"` | exists, edit |
| Housekeeping | Full suite, Debug and Release | — | commands above | — |

### Sampling Rate
- **Per task commit:** the single tag of the area touched (all under ~10 s Debug except `[loudness]` and the 5-profile sweeps, which should stay under ~30 s; use KEMAR/HUTUBS for quick loops, SADIE only where the 36 MB parse is the point).
- **Per wave merge:** full Debug suite.
- **Phase gate:** full suite green in Debug **and** Release (`build-release/tests`), then `/gsd-verify-work`.

### Wave 0 Gaps
- [ ] `tests/Binaural/BinauralMetrics.h` — third-octave metric, impulse-response helper, K-weighting, click/ratio metric (shared by 5 test files)
- [ ] `tests/Binaural/PartitionedConvolverTests.cpp`, `BinauralRendererTests.cpp`, `HRTFEmbeddedTests.cpp`, `BinauralCueTests.cpp`, `ProfileLoudnessTests.cpp`, `tests/Engine/ProfileSwitchTests.cpp`
- [ ] `tests/CMakeLists.txt` — one owner plan for the source-list edits (the current mechanism is a hand-maintained list)
- [ ] Verify `SPATIALCORE_EMBED_ALL_HRTF=OFF` configures and links (KEMAR-only), and that tests guarded by `SPATIALCORE_EMBEDS_ALL_HRTF` compile out cleanly
- [ ] No test framework install needed

## Security Domain

`security_enforcement` is absent from config, so it is enabled.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | — |
| V3 Session Management | no | — |
| V4 Access Control | yes (folder trust) | Only the system folder is read (D-10); no per-user folder in v1 removes the unprivileged-user write path on macOS (`/Library/Application Support` is `root:admin`); never create the folder from SpatialCore |
| V5 Input Validation | yes | The SOFA bytes are untrusted input: `mysofa_open_data` return/`err` are the validation (all 11 malformed inputs rejected); validate the profile index (0..5) at the API boundary; do not trust the filename list (D-08 exact names only); cap file size read from the shared folder (e.g. reject > 256 MB) before `loadFileAsData` |
| V6 Cryptography | no | — |
| V12 Files and Resources | yes | Path is built from a fixed table, never from user input, so no traversal; refuse symlinks that leave the folder only if cheap (Open) |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Malformed SOFA/HDF5 exploiting the parser (libmysofa has a CVE history in 0.5-1.1, fixed in 1.2; v1.3.5 adds malformed-input hardening) | Tampering / Elevation | Parse on the worker thread (a failure never reaches the audio thread); keep embedded fallback; consider the v1.3.5 bump (Open Question 4); fuzz-style bit-flip test already shown to be rejected |
| Same-name file planted by a non-admin user (Windows `ProgramData`, [ASSUMED] A5) | Tampering | Installer owns ACLs; SpatialCore only reads |
| Git LFS pointer embedded into a release | Tampering (supply chain) | CMake configure-time HDF5 signature check (Pattern 6) plus the CI guard |
| Oversized file exhausting memory | DoS | Size cap before read; load runs off the audio thread |
| Status text injection | Information disclosure | Status is an enum; the string is built from a static table, never from file contents |

## Project Constraints (from CLAUDE.md)

- NEVER allocate memory in any function called from `processBlock` (the cue bank, mailbox, crossfade and status word must be allocation-free; the worker may allocate).
- NEVER modify the `SpatializationAlgorithm` interface without a major bump (hence the cue bank is engine-side; `BinauralGains` unchanged).
- ALWAYS maintain backward compatibility with existing plugins: every new `RenderBlockContext` field defaults `false`; escape hatches stay public.
- ALWAYS run the full test suite before tagging a release (Debug and Release).
- Lock-free audio path; stateless algorithms; facade boundary (SC-13): stereo-variant gains stay consumer-side.
- Semantic versioning: additive API (profile call, status, opt-in flag, CMake option) is a **minor** bump.
- Fast-math is a build error in guarded TUs (`src/Core/FloatSemanticsGuard.h`); any new `.cpp` with `std::isfinite` guards must include it.
- HRTF profiles are LFS-tracked `.sofa` files; CLAUDE.md lines 87 and 112 must be rewritten when DATA-01 lands (they currently say the opposite).
- Outward actions (OSD#234 comment, any issue edit) are confirmed with the user first (D-13).
- Project skills (`spatial-audio-dsp`, `juce-best-practices`, `dsp-cookbook`) apply: Path B in `spatial-audio-dsp/SKILL.md` becomes inaccurate ("no spectral coloring").

## Sources

### Primary (HIGH confidence)
- This tree, read this session: `src/Algorithms/DirectBinauralAlgorithm.cpp`, `src/Binaural/{PartitionedConvolver,BinauralRenderer,HRTFDatabase}.cpp` and headers, `src/Engine/RenderEngine.cpp` and header, `CMakeLists.txt`, `tests/CMakeLists.txt`, `tests/Binaural/*`, `.github/workflows/ci.yml`.
- OpenSpatialDelay (`~/conductor/repos/openspatialdelay`, `Source/PluginProcessor.cpp` 20-27, 1501-1590, 4451; `CMakeLists.txt:48-58`): profile table, load glue, `useHRTF` derivation.
- JUCE 9.0.0 sources in `build/_deps/juce-src`: `JUCEUtils.cmake:460-533` (`juce_add_binary_data`), `juce_Files_mac.mm:211`, `juce_Files_windows.cpp:722`, `juce_Files_linux.cpp:137`, `juce_File.h:915-925`.
- libmysofa v1.3.2 source in `build/_deps/mysofa-src/src/hrtf/easy.c` (delay units, `mysofa_open_data`).
- Brown & Duda, "A Structural Model for Binaural Sound Synthesis", IEEE Trans. Speech Audio Process. 6(5):476-488, 1998 (PDF read: pp.478-486, Table I, Eq. 6-8, Table II).
- Direct measurements this session (probe harness, deleted from the tree; copy in `/tmp/scratch/tests-saved`): HRIR band tables, engine IR metrics, convolver oracle, block-size equivalence, moving-source and switch continuity, loudness preview, corrupt-input robustness, Debug-vs-Release HRIR diff, BinaryData build cost.
- `gh api repos/Spatial-Media-Lab/OpenSpatialDelay/issues/234` (symptom text and three candidate causes); `gh issue view 15 -R AndrewRahman/SpatialCore`.

### Secondary (MEDIUM confidence)
- github.com/hoene/libmysofa/releases (fetched): v1.3.5 note "Harden HDF/SOFA parser against malformed input".
- ITU-R BS.1770-4 K-weighting coefficients, confirmed by search against the published 48 kHz table.

### Tertiary (LOW confidence)
- Blauert (1997) directional bands and Hebrank & Wright (1974) spectral cues: search-result summaries only (ResearchGate / JASA abstracts), primary texts not read. Classified LOW by the `classify-confidence` seam (`websearch` -> LOW; `webfetch --verified` -> LOW).

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new dependencies; versions read from the build files.
- Architecture: HIGH for the convolver/crossfade/loader findings (measured, prototyped, reverted); MEDIUM for the Simple<->HRTF dual-path design (specified, not prototyped).
- Pitfalls: HIGH — each was reproduced or read in source this session.
- Literature values: MEDIUM — the design is anchored to measurements of the shipped HRTFs; Blauert / Hebrank-Wright are corroboration from summaries.

**Research date:** 2026-10-04
**Valid until:** 2026-11-03 (stable codebase; re-measure if `PartitionedConvolver` or `RenderEngine` change before planning)

## RESEARCH COMPLETE
