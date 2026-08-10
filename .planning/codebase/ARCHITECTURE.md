<!-- refreshed: 2026-08-10 -->
# Architecture

**Analysis Date:** 2026-08-10

**Scope note:** Analyzed on branch `gsd-remap` (HEAD `8d1868e`, a merge of `origin/spatialcore-v2-extraction` code + `origin/main` planning docs). All claims below are verified against the tree at that commit via `ls`/`grep`/`git show`. `docs/` and `CLAUDE.md` contain some stale claims (e.g. "8 spatialization algorithms" in `CLAUDE.md` table header text is actually correct — 8 confirmed below — but `CLAUDE.md`'s omission of `Engine/` as a component and `SpatialCoreUI` as a second link target is out of date; both are documented here from the tree).

## System Overview

```text
┌──────────────────────────────────────────────────────────────────────┐
│                    CONSUMER (e.g. OpenSpatialDelay/Panner)           │
│  owns: delay line, feedback, pitch-shift, Doppler, tap-fade ramps,   │
│  dry/wet mix, output limiter, stereo-variant gain math (D-06)        │
└───────────────────────────┬────────────────────────────────────────┘
                             │  fills RenderSources + RenderBlockContext
                             ▼
┌──────────────────────────────────────────────────────────────────────┐
│              spatialcore::RenderEngine  (facade, SC-13)              │
│  `include/SpatialCore/Engine/RenderEngine.h` /                       │
│  `src/Engine/RenderEngine.cpp`                                       │
│  renderBlock() dispatches to one of 5 verbatim-transplanted paths:   │
│  stereo / direct-binaural-HRTF / simple-binaural-Woodworth /         │
│  ambisonics / discrete-surround. Owns glitch-free output-format      │
│  double-buffer swap, gain-interpolation state, and (opt-in) gain     │
│  computation via computeObjectGains().                               │
└───────┬───────────────────┬───────────────────────┬──────────────────┘
        │                   │                       │
        ▼                   ▼                       ▼
┌──────────────────┐ ┌──────────────────────┐ ┌───────────────────────┐
│ Algorithms        │ │ Binaural              │ │ IO                    │
│ `Algorithms/*.h`  │ │ `Binaural/*.h`        │ │ `IO/*.h`               │
│ 8 concrete         │ │ BinauralRenderer (12  │ │ OutputFormatRegistry, │
│ SpatializationAlgo │ │ per-source            │ │ SpeakerLayout (14     │
│ implementations,   │ │ PartitionedConvolvers)│ │ ITU-R layouts),       │
│ stateless          │ │ HRTFDatabase (SOFA)   │ │ AmbisonicsCodec       │
│ computeGains()     │ │ SharedFFTCache        │ │                       │
└──────────────────┘ └──────────────────────┘ └───────────────────────┘
                             │
                             ▼
                 float* outChannels[numOutCh]
                 (consumer-owned output buffer;
                  RAW WET signal only — no dry/wet
                  mix, no limiter, no PDC)
```

Two independent, orthogonal subsystems also exist and are NOT wired through RenderEngine:

- **OSC** (`OSC/*.h`, `src/OSC/*.cpp`) — ADM-OSC receive/send, parses/broadcasts object positions; the consumer reads/writes `ObjectState` that eventually flows into `RenderSources::objects`.
- **Trajectory** (`Trajectory/*.h`, `src/Trajectory/*.cpp`) — `TrajectoryEngine` (13 shapes) and `DopplerVelocity` (Doppler/pitch estimation) run consumer-side, upstream of `RenderEngine::renderBlock` (per the ENGINE BOUNDARY comment in `RenderEngine.h:33-43`, Doppler/pitch-shift is explicitly OUTSIDE the engine).
- **UI** (`UI/*.h`, `src/UI/*.cpp`) — compiled into a *separate* CMake target, `SpatialCoreUI` (see Entry Points / Two Link Targets below), not linked into `SpatialCore` itself.

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| RenderEngine | Consumer-facing render facade; 5-branch format dispatch; owns gain-interpolation, HRTF-renderer double-buffer, output-layout double-buffer, optional gain computation | `include/SpatialCore/Engine/RenderEngine.h`, `src/Engine/RenderEngine.cpp` |
| SpatializationAlgorithm (+ 8 impls) | Pure-function per-speaker gain computation from a source position + layout context | `include/SpatialCore/Algorithms/SpatializationAlgorithm.h`, `include/SpatialCore/Algorithms/{VBAP,VBIP,KNN,DBAP,MDAP,Ambisonics,DirectBinaural,ConstantPower}Algorithm.h` |
| BinauralRenderer | Per-source HRTF convolution engine (12 parallel L/R convolver pairs), ITD buffering, low-shelf compensation | `include/SpatialCore/Binaural/BinauralRenderer.h`, `src/Binaural/BinauralRenderer.cpp` |
| HRTFDatabase | SOFA file loading (libmysofa) and HRIR lookup by azimuth/elevation | `include/SpatialCore/Binaural/HRTFDatabase.h`, `src/Binaural/HRTFDatabase.cpp` |
| PartitionedConvolver | Single-source FFT overlap-save convolution primitive | `include/SpatialCore/Binaural/PartitionedConvolver.h`, `src/Binaural/PartitionedConvolver.cpp` |
| SharedFFTCache | Process-global FFT plan singleton shared across all convolvers | `include/SpatialCore/Binaural/SharedFFTCache.h` |
| OutputFormatRegistry | Single source of truth mapping `OutputFormat` enum → channel count / name / LFE / height flags | `include/SpatialCore/IO/OutputFormat.h`, `include/SpatialCore/IO/OutputFormatRegistry.h`, `src/IO/OutputFormatRegistry.cpp` |
| SpeakerLayout | 14 ITU-R/SMPTE speaker layouts + VBAP triplet builder | `include/SpatialCore/IO/SpeakerLayout.h`, `src/IO/SpeakerLayout.cpp` |
| AmbisonicsCodec | Spherical-harmonic evaluation and Ambisonics decode-matrix construction | `include/SpatialCore/IO/AmbisonicsCodec.h`, `src/IO/AmbisonicsCodec.cpp` |
| ADMOSCReceiver / ADMOSCSender | Parse `/adm/obj/N/...` OSC messages; broadcast object state at 30 Hz | `include/SpatialCore/OSC/ADMOSCReceiver.h`, `include/SpatialCore/OSC/ADMOSCSender.h` |
| TrajectoryEngine | 13 named trajectory shapes, origin-point-relative, forward/reverse playback | `include/SpatialCore/Trajectory/TrajectoryEngine.h`, `src/Trajectory/TrajectoryEngine.cpp` |
| DopplerVelocity | Per-object velocity/Doppler-semitone smoothing, independent `kMaxObjects` sizing | `include/SpatialCore/Trajectory/DopplerVelocity.h`, `src/Trajectory/DopplerVelocity.cpp` |
| UI widgets | SpatialMapComponent, SMLLookAndFeel, ReverseSlider, IndicatorToggle, StyledButton, GlobalTapDrawer, PresetBrowser, IOSectionComponent, OSCSectionComponent, ObjectPanel | `include/SpatialCore/UI/*.h`, `src/UI/*.cpp` |

## Pattern Overview

**Overall:** Layered DSP-library-as-static-archive, with a single narrow facade (`RenderEngine`) added on top of previously-independent leaf modules. Algorithms are stateless strategy objects; the engine is the only stateful orchestration layer exposed to consumers.

**Key Characteristics:**
- Consumer owns everything upstream (delay, feedback, pitch-shift, Doppler, dry/wet mix) and downstream (output gain, limiter, PDC) of `RenderEngine::renderBlock`.
- Algorithms are pure functions over `SourcePosition` + `LayoutContext` — no allocation, no locks, callable from the audio thread (`Algorithms/SpatializationAlgorithm.h:9-13`, "FROZEN (D-02) — moved verbatim... Do not add, remove, or change any virtual method signature").
- Lock-free, glitch-free live reconfiguration via dual-buffered state with atomic index swap, used in two places: HRTF-profile swap (`RenderEngine.h:326-336`, `binauralRenderers[2]` + `activeRendererIndex`) and output-format/layout swap (`RenderEngine.h:378-380`, `layoutBuffers[2]` + `activeLayoutIndex`).
- Opt-in architecture change (SC-13, commit `8ba19fc`): `RenderBlockContext::engineComputesGains` (default `false`) lets the engine take over per-object gain computation instead of requiring the consumer to hand-build a `LayoutContext` and call an algorithm itself. Default-off preserves byte-identical behavior for existing callers.

## Layers

**Engine (facade):**
- Purpose: single per-block entry point (`renderBlock`) reproducing the original 5-branch `OpenSpatialDelayProcessor::processBlock` dispatch, plus engine-owned persistent DSP/interpolation state and format-switching machinery.
- Location: `include/SpatialCore/Engine/RenderEngine.h`, `src/Engine/RenderEngine.cpp`
- Contains: `RenderEngine` class, `RenderSources`/`RenderBlockContext` hand-off structs.
- Depends on: Algorithms (`VBAPAlgorithm`, `DirectBinauralAlgorithm` for SC-13 gain computation), Binaural (`BinauralRenderer[2]`), IO (`OutputFormat`, `SpeakerLayout`, `AmbisonicsCodec` decode matrices).
- Used by: the consumer plugin's audio-thread `processBlock`.

**Algorithms (strategy layer):**
- Purpose: compute per-speaker or per-ear gains from a source position.
- Location: `include/SpatialCore/Algorithms/*.h`, `src/Algorithms/*.cpp`
- Contains: `SpatializationAlgorithm` abstract base + 8 concrete implementations (VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural, ConstantPower).
- Depends on: Core (`Types.h` structs only).
- Used by: RenderEngine (SC-13 path) and, historically, the consumer directly (pre-SC-13 callers still build `LayoutContext` themselves).

**Binaural (convolution layer):**
- Purpose: HRTF-based binaural rendering — SOFA loading, per-source FFT convolution, ITD application.
- Location: `include/SpatialCore/Binaural/*.h`, `src/Binaural/*.cpp`
- Contains: `SharedFFTCache` (process-global singleton), `HRTFDatabase`, `PartitionedConvolver`, `BinauralRenderer` (array of `MAX_SOURCES` convolver pairs).
- Depends on: libmysofa (via `HRTFDatabase.cpp`), JUCE dsp module.
- Used by: RenderEngine's `renderDirectBinauralHRTF` path.

**IO (format/layout layer):**
- Purpose: describe and resolve output formats to concrete speaker geometry / Ambisonics decode matrices.
- Location: `include/SpatialCore/IO/*.h`, `src/IO/*.cpp`
- Contains: `OutputFormat` enum (25 values), `OutputFormatRegistry`, `SpeakerLayout`/`LayoutID` (14 named layouts + `NUM_LAYOUT_DEFS` sentinel), `AmbisonicsCodec`.
- Used by: RenderEngine's `activateLayout`/`computeAmbiDecodeForLayout`.

**OSC layer:** parses/emits ADM-OSC object-position messages; consumer-side glue feeds results into `RenderSources::objects`. Independent of RenderEngine.

**Trajectory layer:** shape generation + Doppler/velocity smoothing; consumer-side, upstream of RenderEngine per the documented engine boundary.

**UI layer:** JUCE GUI components; compiled into the separate `SpatialCoreUI` target (see below), never linked into the DSP-only `SpatialCore` target.

**Core (shared types):**
- Purpose: plain data structs and constants shared across all layers.
- Location: `include/SpatialCore/Core/Types.h`, `SourcePosition.h`, `BinauralGains.h`, `SpatialMath.h`/`.cpp`
- Contains: `MAX_SOURCES`, `MAX_SPEAKERS`, `ObjectState`, `BinauralGains`, `SourcePosition`, `LayoutContext`, `BinauralContext`, `BinauralProfile` + `kDefaultBinauralProfiles[5]`.

## Data Flow

### Primary Render Path (per audio block)

1. Consumer's `processBlock` runs delay/feedback/pitch-shift/Doppler (outside SpatialCore), producing per-object mono buffers.
2. Consumer populates `RenderSources` (mono buffers, tap-fade envelope, distance-gain trajectory, `ObjectState` positions, live flags) and `RenderBlockContext` (target gains per path, active format, `engineComputesGains` flag) — `include/SpatialCore/Engine/RenderEngine.h:72-172`.
3. Consumer calls `RenderEngine::renderBlock(sources, blockCtx, outChannels, numOutCh)` (`RenderEngine.h:218-221`).
4. If `blockCtx.engineComputesGains == true`, `RenderEngine::computeObjectGains()` fills `objChannelGains` (via internal `VBAPAlgorithm surroundAlgorithm_`) and `objGains` (via internal `DirectBinauralAlgorithm binauralAlgorithm_` + `kDefaultBinauralProfiles`) before dispatch (`RenderEngine.h:298-304`, `src/Engine/RenderEngine.cpp`).
5. `renderBlock` dispatches on `blockCtx` flags to exactly one of 5 private methods, in this fixed priority order (`RenderEngine.h:199-206`):
   - `isStereoVariant` → `renderStereoVariant`
   - `isBinaural && useHRTF` → `renderDirectBinauralHRTF` (routes through `BinauralRenderer`)
   - `isBinaural` → `renderSimpleBinauralWoodworth`
   - `isAmbiOutput` → `renderAmbisonicsOutput` (uses `AmbisonicsCodec` + NFC-HOA IIR filters)
   - else → `renderDiscreteSurround`
6. Selected path writes RAW WET signal directly into `outChannels` — no dry/wet mix, output gain, limiter, or PDC (explicitly out of scope; consumer applies these after `renderBlock` returns).

### Output-Format Switching (message thread)

1. Consumer calls `RenderEngine::setOutputFormat(format)`.
2. Engine computes the new `LayoutState` (speaker layout, Ambisonics decode matrix, VBAP triplets) into the inactive `layoutBuffers[]` slot via `activateLayout()`.
3. Atomic swap of `activeLayoutIndex` makes the new layout visible to the audio thread with no glitch (`RenderEngine.h:378-380`).

### HRTF Profile Swap (message thread)

1. Consumer loads a new SOFA profile into the inactive `binauralRenderers[]` slot via the `getPrepareRendererIndex()`/`getBinauralRenderer()` escape hatches.
2. Consumer calls `swapActiveRenderer()`, which atomically flips `activeRendererIndex` and begins an 8-block crossfade (`kRendererXfadeBlocks`) to avoid a pop (`RenderEngine.h:267-274`, `330-336`).

**State Management:**
- All engine-owned block-to-block state (gain-interpolation "previous" arrays, NFC-HOA filter state, LFE filter, double-buffered renderer/layout state) lives as private members of `RenderEngine` (`RenderEngine.h:317-390`), not in `RenderSources`/`RenderBlockContext` which are re-supplied fresh every call.

## Key Abstractions

**SpatializationAlgorithm:**
- Purpose: stateless strategy for per-speaker/per-ear gain computation.
- Examples: `include/SpatialCore/Algorithms/VBAPAlgorithm.h`, `VBIPAlgorithm.h`, `KNNAlgorithm.h`, `DBAPAlgorithm.h`, `MDAPAlgorithm.h`, `AmbisonicsAlgorithm.h`, `DirectBinauralAlgorithm.h`, `ConstantPowerAlgorithm.h` (8 total, verified via `grep -l "public SpatializationAlgorithm"`).
- Pattern: abstract base with `computeGains()` pure virtual (frozen signature, D-02) plus optional-capability virtuals (`supportsBinauralDirect`, `supportsSurround`, `supportsSHDomain`) defaulting to base-class behavior.

**RenderSources / RenderBlockContext (hand-off structs):**
- Purpose: the full parameter surface `RenderEngine::renderBlock` needs, split into per-sample source data (`RenderSources`) and per-block format/gain context (`RenderBlockContext`).
- Location: `include/SpatialCore/Engine/RenderEngine.h:72-172`.
- Pattern: C-style flat array layout (`float* monoBuffers[MAX_SOURCES]`, `ObjectState objects[MAX_SOURCES]`, etc.), explicitly documented as PROVISIONAL — "flagged for ergonomic review in the Phase 10+ post-extraction refactor... do not redesign it now" (`RenderEngine.h:47-50`).

**Consumer- vs. engine-filled fields (RenderSources — always consumer-filled):**
| Field | Filled by |
|---|---|
| `monoBuffers[MAX_SOURCES]` | Consumer (already delayed/feedback/Doppler/pitch-shifted) |
| `sourceEnabledBlock[MAX_SOURCES]` | Consumer (unused by renderBlock itself; retained for symmetry) |
| `tapFadeGainPerSample[MAX_SOURCES]` | Consumer (pre-advanced per-sample ramp) |
| `distGainPerSample[MAX_SOURCES]` | Consumer (pre-interpolated block-start→block-end) |
| `objects[MAX_SOURCES]` (ObjectState) | Consumer |
| `objectLive[MAX_SOURCES]` | Consumer |
| `numSamples` | Consumer |

**Consumer- vs. engine-filled fields (RenderBlockContext):**
| Field | Filled by | Notes |
|---|---|---|
| `objGains[MAX_SOURCES]` (BinauralGains) | Consumer by default; **ENGINE** if `engineComputesGains == true` | via internal `DirectBinauralAlgorithm` + `kDefaultBinauralProfiles` |
| `objGainL/objGainR[MAX_SOURCES]`, `stereoMode` | Always Consumer | stereo-variant gain math is explicitly NOT a `SpatializationAlgorithm` concern (D-06); never engine-computed even with the flag set |
| `objChannelGains[MAX_SOURCES][MAX_SPEAKERS]` | Consumer by default; **ENGINE** if `engineComputesGains == true` | via internal fixed `VBAPAlgorithm` (not runtime-selectable — that is a separate future concern, SPAT-01) |
| `ambiOrder`, `sampleRate` | Always Consumer | |
| `activeFormat`, `isStereoVariant`, `isBinaural`, `isAmbiOutput`, `useHRTF` | Always Consumer (pre-resolved via `getActiveLayout()`) | kept explicit per-block rather than cached, so `renderBlock` stays a pure function of its inputs for testability |
| `engineComputesGains` | Consumer (opt-in flag) | defaults `false` |

**RenderEngine ↔ SpatializationAlgorithm ↔ BinauralRenderer relationship:**
- `RenderEngine` does NOT implement `SpatializationAlgorithm` and is not itself an algorithm. It *owns and calls* two hardcoded algorithm instances (`VBAPAlgorithm surroundAlgorithm_`, `DirectBinauralAlgorithm binauralAlgorithm_`) only when `engineComputesGains` is set, inside the private `computeObjectGains()` method (`RenderEngine.h:387-389`, `304`).
- For all other cases (the default), gain arrays (`objGains`, `objChannelGains`) arrive pre-computed from the consumer, meaning the consumer may call any of the 8 `SpatializationAlgorithm` implementations directly and hand results to `RenderEngine` — the engine has no compile-time or runtime dependency forcing use of a particular algorithm outside the SC-13 opt-in path.
- `BinauralRenderer` is a separate, lower-level subsystem RenderEngine owns two instances of (`binauralRenderers[2]`) for double-buffered HRTF profile swap; it performs FFT convolution, not gain-vector computation — `SpatializationAlgorithm::computeGains()` and `BinauralRenderer`'s convolution are architecturally parallel, not layered on each other. `DirectBinauralAlgorithm::computeBinauralGains()` (an algorithm capability) produces the target `BinauralGains` (gain values), while `BinauralRenderer` separately does the actual per-sample HRIR convolution using those/HRTF-derived gains.

## `MAX_SOURCES` Constant and Sizing

`spatialcore::MAX_SOURCES = 12` is defined once, in `include/SpatialCore/Core/Types.h:8`. `MAX_SPEAKERS = 16` is defined immediately after (`Types.h:9`).

**Arrays sized by `MAX_SOURCES`** (verified via `grep -rn "MAX_SOURCES\]"` across `include/SpatialCore`):
- `Engine/RenderEngine.h`: `RenderSources::{monoBuffers, sourceEnabledBlock, tapFadeGainPerSample, distGainPerSample, objects, objectLive}`; `RenderBlockContext::{objGains, objGainL, objGainR, objChannelGains[][MAX_SPEAKERS]}`; engine-private `sourceAccumBufPtrs`, `prevBinauralGains`, `prevStereoGainL/R`, `prevChannelGains[][MAX_SPEAKERS]`, `prevSHCoeffs[][kMaxAmbiChannels]`, `nfcFilters[][kMaxAmbiOrder]`, `smoothedNfcDistance`, `prevNfcDistance`.
- `Binaural/BinauralRenderer.h`: `sourceConvL/R`, `cachedSourceAz/El`, `sourceConvReady`, `currentITDL/R`, `targetITDL/R`, `itdBufferL/R[][kITDBufferSize]`, `itdWritePos`, `lfShelfStateL/R[][2]`.
- `OSC/ADMOSCSender.h`: `prevAz`, `prevEl`, `prevDist`.
- `UI/SpatialMapComponent.h`: `objectColours[MAX_SOURCES]`.

**Independent, non-tracking constant:** `include/SpatialCore/Trajectory/DopplerVelocity.h:17` defines its own `static constexpr int kMaxObjects = 12;`, **not** derived from or referencing `spatialcore::MAX_SOURCES`. It happens to equal 12 today but is a textually separate literal — changing `MAX_SOURCES` in `Types.h` will NOT change `DopplerVelocity::kMaxObjects`, and vice versa. Arrays sized by it: `prevAz_`, `prevEl_`, `prevDist_`, `smoothedVelocity_`, `rawSemitones_`, `smoothedSemitones_`, `prevSmoothedSemitones_`, `intermediateSmoothed_`, `posChanged_` (all `DopplerVelocity.h:51-61`). This is a latent drift risk: bumping `MAX_SOURCES` without also updating `DopplerVelocity::kMaxObjects` would silently create a mismatched per-object array bound between the trajectory/Doppler subsystem and everything else.

## Entry Points

**`spatialcore::RenderEngine::renderBlock`:**
- Location: `include/SpatialCore/Engine/RenderEngine.h:218`, implemented `src/Engine/RenderEngine.cpp`
- Triggers: consumer plugin's audio-thread `processBlock`, once per audio block.
- Responsibilities: dispatches to exactly one of 5 render paths, writes raw wet output; no dry/wet, gain, or limiting.

**`SpatialCore.h` (umbrella header, DSP-only):**
- Location: `include/SpatialCore/SpatialCore.h`
- Includes: Core, Algorithms (all 8 + `AllAlgorithms.h`), Binaural, IO, OSC, Trajectory, and `Engine/RenderEngine.h`.
- Explicitly excludes UI headers — comment at top: "UI headers... are deliberately NOT included here; they land in a separate SpatialCoreUI umbrella in Phase 9. Consumers that need UI widgets include `<SpatialCore/UI/*.h>` directly for now." (No separate `SpatialCoreUI.h` umbrella header was found in the tree as of this analysis — only the CMake target exists; consumers include individual `UI/*.h` files.)

**Two CMake link targets (confirmed, `CMakeLists.txt`):**
1. `SpatialCore` (STATIC) — DSP-only: Algorithms, Core, Binaural, IO, OSC, Trajectory, Engine `.cpp` sources. Links `juce::juce_core`, `juce_audio_basics`, `juce_audio_formats`, `juce_dsp`, `juce_osc` (INTERFACE-only when consumed via `add_subdirectory`, PUBLIC when built standalone), plus `mysofa-static` and `ZLIB::ZLIB` (PRIVATE).
2. `SpatialCoreUI` (STATIC) — GUI-linked: `src/UI/{SMLLookAndFeel,ReverseSlider,IndicatorToggle,StyledButton,SpatialMapComponent,GlobalTapDrawer,PresetBrowser,IOSectionComponent,OSCSectionComponent,ObjectPanel}.cpp`. Separate target so a DSP-only consumer never pulls GUI modules transitively (per `CMakeLists.txt` comment "Pattern 4, 09-RESEARCH.md"). Confirmed by commit `3da7d89` ("docs: clarify consumer integration (link SpatialCore + SpatialCoreUI...)") which fixed docs that previously showed only the `SpatialCore` target, causing UI-consuming plugins to fail to link.

Consumers must `add_subdirectory(SpatialCore)` and `target_link_libraries(MyPlugin PRIVATE SpatialCore SpatialCoreUI)` if they use any UI widget — linking only `SpatialCore` is valid for DSP-only consumers.

## Architectural Constraints

- **Threading:** Audio-thread path (`renderBlock` and everything it calls — Algorithms, `BinauralRenderer` convolution) must be lock-free/alloc-free. Message-thread-only APIs are explicitly marked in comments: `setBinauralProfileIndex`, `getPrepareRendererIndex`/`swapActiveRenderer`, HRTF profile loading via `getBinauralRenderer()` escape hatch (`RenderEngine.h:234-239`, `260-266`).
- **Double-buffered lock-free swap:** two independent instances of the pattern — `binauralRenderers[2]`/`activeRendererIndex` (HRTF profile) and `layoutBuffers[2]`/`activeLayoutIndex` (output format) — both use `std::atomic<int>` index swap, never mutate the active slot in place.
- **Global state:** `SharedFFTCache` (`include/SpatialCore/Binaural/SharedFFTCache.h`) is described in `CLAUDE.md` as a "process-global FFT singleton" — verify singleton implementation in `SharedFFTCache.h` before relying on this claim in downstream planning, as it was not directly re-verified line-by-line in this pass beyond file existence.
- **Frozen interface:** `SpatializationAlgorithm`'s virtual method set is marked FROZEN (D-02) in `SpatializationAlgorithm.h:9-13` — adding/removing/changing any virtual signature requires a major version bump per `CLAUDE.md`'s versioning policy.
- **Independent per-object constant drift:** see `MAX_SOURCES` section above — `DopplerVelocity::kMaxObjects` does not track `spatialcore::MAX_SOURCES`.
- **Float-associativity sensitivity:** `CMakeLists.txt` deliberately does NOT apply `-ffast-math`/`/fp:fast` to the `SpatialCore` target (unlike the consumer's own targets), because per-translation-unit `-ffast-math` reassociation broke a bit-identical regression harness gate (documented at length in `CMakeLists.txt` around the `TrajectoryEngine.cpp` note).

## Anti-Patterns

### Provisional flat-array hand-off treated as permanent

**What happens:** `RenderSources`/`RenderBlockContext` use raw C arrays and pointer arrays sized to `MAX_SOURCES`, mirroring the pre-extraction `OpenSpatialDelayProcessor` member layout verbatim.
**Why it's wrong:** The struct comments explicitly flag this as "the least-diff shape against the existing call site... PROVISIONAL and flagged for ergonomic review in the Phase 10+ post-extraction refactor" — it is a known-temporary shape, not an idiomatic public API, and any new consumer integrating today inherits that awkwardness.
**Do this instead:** Treat `RenderSources`/`RenderBlockContext` as unstable/low-ergonomics by design; do not build additional public API surface that assumes this exact shape is permanent. Any refactor of this hand-off is a locked-decision item requiring the Phase 10+ process, not an ad hoc change.

### Independently-declared per-object size constants

**What happens:** `DopplerVelocity.h` declares its own `kMaxObjects = 12` instead of referencing `spatialcore::MAX_SOURCES` from `Core/Types.h`.
**Why it's wrong:** Two sources of truth for "max concurrent objects" that happen to agree today but have no compiler-enforced link; changing one without the other silently creates mismatched bounds between the Doppler/trajectory subsystem and every other `MAX_SOURCES`-sized array (Engine, Binaural, OSC, UI).
**Do this instead:** New code needing a per-object bound should reference `spatialcore::MAX_SOURCES` directly; if `DopplerVelocity` cannot be changed immediately, any future change to `MAX_SOURCES` must include an explicit grep-and-check step for `kMaxObjects`.

## Error Handling

**Strategy:** Not fully audited in this pass. `computeGains`/`renderBlock` are `void`-returning hot-path functions consistent with a no-exceptions, no-allocation audio-thread policy implied by `CLAUDE.md`'s "NEVER allocate memory in any function called from processBlock" rule. HRTF/SOFA file loading (`HRTFDatabase`) is message-thread-only and more likely to use return-value/bool-success patterns — not verified line-by-line here.

**Patterns:**
- Optional-capability virtuals default to safe no-ops (`SpatializationAlgorithm::computeBinauralGains` returns `{}` by default; `supportsBinauralDirect`/`supportsSHDomain` default `false`).

## Cross-Cutting Concerns

**Logging:** None found in the audio-thread-facing headers reviewed (`RenderEngine.h`, `SpatializationAlgorithm.h`) — consistent with the lock-free audio-path rule in `CLAUDE.md`.
**Validation:** `OSC/OSCPortValidation.h`/`.cpp` exists as a dedicated validation module for OSC port configuration (message-thread, not audio-thread).
**Authentication:** Not applicable — this is an embedded DSP library, no network auth surface beyond OSC receive/send.

---

*Architecture analysis: 2026-08-10*
</content>
