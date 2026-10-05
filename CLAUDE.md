# SpatialCore — Project Context for Claude

## Identity
- **Library:** SpatialCore — the shared spatial audio rendering engine for the Spatial Media Library
- **Organization:** Spatial Media Lab (spatialmedialab.org)
- **What it does:** Takes audio objects with 3D positions and renders them to any output format (binaural, stereo, surround, Ambisonics) via 8 spatialization algorithms
- **License:** GPL-3.0 + commercial (dual license)
- **GitHub:** `https://github.com/AndrewRahman/SpatialCore` — the development remote, and the URL to clone or submodule today. `Spatial-Media-Lab/SpatialCore` is the post-proof destination and **does not resolve yet** (DR-18); do not substitute it. See `docs/integration-guide.md` "Remote topology".

## Origin
SpatialCore was extracted from OpenSpatialDelay v1.0, where 68% of the codebase was marked as reusable spatial audio infrastructure. The extraction separated framework code (algorithms, HRTF, I/O, OSC, trajectories, UI) from delay-specific code (delay line, pitch shift, feedback, wobble).

## Architecture

### Core Components
| Component | Headers | Description |
|-----------|---------|-------------|
| Algorithms | `Algorithms/*.h` | 8 spatialization algorithms: ConstantPower, VBAP, VBIP, KNN, DBAP, MDAP, Ambisonics, DirectBinaural. `AllAlgorithms.h`'s `AllAlgorithmTypes` list is the source of truth for the count |
| Binaural | `Binaural/*.h` | SharedFFTCache (process-global FFT singleton), HRTFDatabase (SOFA/libmysofa; `loadFromBinaryData` reads the embedded profile set `SpatialCoreHRTFData`), `HRTFProfile.h` (the profile table, indices 0-5, 0 = Simple), `HRTFProfileResolver` (shared folder, then embedded copy, then a reported error), PartitionedConvolver (FFT overlap-save), BinauralRenderer (12 per-source convolvers) |
| Engine | `Engine/RenderEngine.h` | `RenderEngine` — the consumer-facing render facade. Owns the 5 render paths (direct-binaural HRTF, simple binaural Woodworth, stereo variants, Ambisonics HOA, discrete surround), the glitch-free three-slot output-format layout handoff, the double-buffered HRTF-renderer swap, engine-owned HRTF profile switching (`setHRTFProfile`, an engine-owned background loader, a lock-free status) with the opt-in `engineSelectsHRTF` flag, a Simple-path cue bank (rear, up and down filter branches ahead of the Woodworth gains), a per-block layout snapshot with opt-in dispatch derivation (`engineDerivesDispatch`, SC-16) and an opt-in click-free format-switch fade (`engineFadesFormatSwitch`, SC-20), and (opt-in, SC-13) per-object gain computation via `RenderBlockContext::engineComputesGains` |
| Core | `Core/SpatialMath.h` | `softClip()`, `outputLimiter()` (tanh soft ceiling), `distanceAttenuation()`, plus the shared position/gain types in `Core/Types.h` |
| I/O | `IO/*.h` | OutputFormatRegistry (23 formats), SpeakerLayout (15 ITU-R layouts), AmbisonicsCodec (SH eval, decode matrices) |
| OSC | `OSC/*.h` | ADM-OSC Receive (parse /adm/obj/N/): type-checks every argument, rejects non-finite values, clamps ranges, and reports a position message with no arguments as a query through `Listener::admPositionQueried`. ADM-OSC Send keeps its own 30 Hz clock in `tick` (any timer of 30 Hz or more gives 30 Hz), stays silent while objects are still, sends an object's first position and every enabled object on connect and re-enable, and answers queries with `queueReply`, sent to its configured destination |
| Trajectory | `Trajectory/*.h` | 13 shapes, origin-point architecture, forward/reverse |
| UI | `UI/*.h` | SpatialMapComponent (loads its own embedded JetBrains Mono, so it renders SML fonts under any look-and-feel), SMLLookAndFeel, ReverseSlider, IndicatorToggle, StyledButton |

### Key Interfaces

**`RenderEngine` — the consumer-facing render surface.** A consumer plugin drives
rendering through this facade only:
```cpp
class RenderEngine {
public:
    void prepare(double sampleRate, int maxBlockSize);
    void renderBlock(const RenderSources& sources,
                      const RenderBlockContext& blockCtx,
                      float* const* outChannels, int numOutCh);
    void setOutputFormat(OutputFormat format);
    // ...
};
```
`RenderBlockContext::engineComputesGains` (default `false`, SC-13) is the opt-in flag
that lets `RenderEngine` compute `objChannelGains`/`objGains` internally instead of
requiring the consumer to precompute them. SC-18 part 2: on a stereo-variant block the
same flag also makes the engine fill `objGainL`, `objGainR` and `stereoMode` (the five
stereo modes, scaled by the block's distance gain); with the flag `false` the consumer still
supplies them. The stereo math lives in the engine (`computeStereoModeGains`), not in a
`SpatializationAlgorithm`.

`RenderEngine::setAlgorithmIndex (int)` / `getAlgorithmIndex()` (SC-18) select the speaker
algorithm that computes `objChannelGains` when `engineComputesGains` is set. The index map is
OpenSpatialDelay's 12-entry saved-preset map (`kAlgorithmIndexAmbisonics` 0, `...ConstantPower` 1,
`...DBAP` 2, `...KNN` 3, `...MDAP` 4, `...VBAP` 5, `...VBIP` 6, then the stereo modes 7..11; never
renumber). The default is VBAP, so a consumer that never calls it renders as before. The setter is
lock-free (one relaxed atomic store, clamped) and the engine reads it once per block. On the Stereo
format indices 7..11 select Equal Power, Stereo VBAP, XY Pair, MS Encode and Blumlein (a speaker
index renders as Equal Power there); on a speaker layout a stereo index renders as VBAP.

`RenderBlockContext::engineFadesFormatSwitch` (default `false`, SC-20, SpatialCore#27) makes an
output-format switch click-free. A block that sees a newly published layout renders once more
with the layout the engine already holds and fades its output linearly to zero (last sample
exactly 0); the next block takes the newest published layout, starts every render path's gain
interpolation at that block's own targets and fades in from zero (first sample exactly 0). The
new format is heard one block later and a switch costs a two-block dip (about 21 ms at 512
samples / 48 kHz); several `setOutputFormat()` calls between two blocks give one fade-out and
one fade-in, of the last format. The first block after `prepare()` never fades, and
`getBlockLayout()` reports the layout the block actually rendered (the held one during the
fade-out block). Only the channels the block's path writes are faded. It rests on the
reader-side `TripleBufferIndex::hasFresh()` peek (one relaxed load); wait-free, no allocation,
lock or logging. It is opt-in, so a consumer that does not set it renders byte-for-byte as
before. A consumer that wants the fade should also avoid republishing the format that is
already active (each publish costs a dip).

`RenderBlockContext::engineDerivesDispatch` (default `false`, SC-16) is the opt-in flag
that lets `RenderEngine` derive its own dispatch from the layout it renders against.
`renderBlock()` acquires the engine's layout once per block; when the flag is set it
overwrites five fields of the context from that snapshot's format — `activeFormat`,
`isStereoVariant`, `isBinaural`, `isAmbiOutput` and `ambiOrder` — and the consumer's
values for them are ignored. `useHRTF` and `sampleRate` stay consumer-supplied (and `stereoMode`,
`objGainL` and `objGainR` too, unless `engineComputesGains` fills them on a stereo-variant block). Set it (together with `engineComputesGains`) instead
of deriving the dispatch flags from your own format state: a consumer that keeps its own
copy of the format can read a different switch than the engine's layout holds, which
pairs one format's dispatch with another format's layout and renders a torn, silent
block when a format switch lands mid-block. With the flag left `false` the consumer's
flags are honoured verbatim, so existing consumers are unaffected.

**HRTF profile switching (DATA-01).** `RenderEngine::setHRTFProfile (int profileIndex)` is the one
call a consumer makes to change profile (0 = Simple, 1-5 the SOFA profiles of `HRTFProfile.h`). It is
message-thread-only and returns at once. An engine-owned worker loads the profile into the renderer the
audio thread is not using, the audio thread claims it at a block boundary and crossfades, and the latest
request wins. A failed request (unknown index, file nowhere to be found) keeps the current profile playing
and is reported by `getHRTFProfileStatus()` (lock-free, any thread, returns an `HRTFProfileStatus`);
turn it into text on the message thread with the free function
`spatialcore::describeHRTFProfileStatus (status)`. Requests made before the first `prepare()` are held.
`waitForHRTFProfileIdle (int timeoutMs)` and `setSharedHRTFFolderForTesting (const juce::File&)` exist
for tests. `RenderBlockContext::engineSelectsHRTF` (default `false`, D-15) lets the engine choose the
binaural path from its own active renderer and blend Simple and HRTF during a switch, so profile 0 fades
like any other; the consumer's `useHRTF` is ignored. On a binaural, non-stereo-variant block the flag fills
`objGains` itself, which the Woodworth path needs during an HRTF fade, so that case is safe on its own. Still
set it together with `engineComputesGains`: `objChannelGains` for every non-binaural format stay yours
unless the engine computes them. Do not mix `setHRTFProfile`
with `swapActiveRenderer` / `getPrepareRendererIndex` / `getBinauralRenderer` loads on one engine; those
remain only as the legacy escape hatch. The Simple path (profile 0) is no longer spectrally flat: it
applies a position-blended rear head-shadow and up/down pinna cue bank (`Core/SimpleBinauralCues.h`)
before the Woodworth gains, and the ear-level front half is bit-identical to before (BUG-01, D-02).

**`SpatializationAlgorithm` — engine-internal.** Not a consumer-facing interface;
reached only through `RenderEngine`, never dispatched directly by a consumer:
```cpp
// Abstract base — all algorithms implement this
class SpatializationAlgorithm {
    virtual void computeGains(const SourcePosition& source,
                              const LayoutContext& ctx,
                              float* outputGains, int numSpeakers) const = 0;
    virtual bool supportsBinauralDirect() const { return false; }
    virtual bool supportsSurround() const { return true; }
    virtual bool supportsSHDomain() const { return false; }
    virtual juce::String getName() const = 0;
};
```

### Design Principles
- **Lock-free audio path:** No malloc, locks, or logging in any function called from processBlock
- **Stateless algorithms:** All computation state in context structs, algorithms are pure functions
- **Three-slot wait-free layout handoff (SC-16):** `RenderEngine` keeps three `LayoutState` slots and a `TripleBufferIndex` that decides which slot each thread may touch. The audio thread's slot is never written, so any number of back-to-back `setOutputFormat()` calls between blocks is safe and the next block renders the last one. `setOutputFormat()` is single-writer, message-thread-only and allocating. `getActiveLayout()` / `getActiveOutputFormat()` are the writer-thread view (the most recently published slot) and must not be called from the audio thread: a call made on the render thread while a different thread is the writer is counted (`getWriterViewOnRenderThreadCount()`) and asserts in debug builds (SC-17). The audio thread acquires its layout once per block inside `renderBlock()`, and **audio-thread code reads it through `getBlockLayout()` after `renderBlock()`** (SC-17, SpatialCore#24): it returns the snapshot that block rendered with and never acquires a newer one, so a channel map chosen from it cannot tear against the samples. **Migration:** OpenSpatialDelay must replace its `processBlock()` call to `getActiveLayout()` with `getBlockLayout()` before bumping its SpatialCore pin past `ab60c25`
- **Profile loader and mailbox (DATA-01):** with `setHRTFProfile` the audio thread is the only writer of the active renderer index. The loader thread writes only a renderer the audio thread is not using, and hands it over through a one-slot atomic mailbox that the audio thread claims at the top of a block. No lock, allocation or file access is ever on the audio side of this handoff (the remaining defensive `jassertfalse` plus `resize` guards in `BinauralRenderer` and `renderDirectBinauralHRTF` are RTSF-01, Phase 5, and are reachable only on a prepare-contract violation). The SOFA load (up to 36 MB built in, up to 256 MB from the shared folder) runs on the loader only
- **Sample-based crossfades with floors:** the convolver crossfade is never shorter than `PartitionedConvolver::kMinCrossfadeSamples` (2048), and the renderer crossfade lasts `max(8 blocks, RenderEngine::kMinRendererXfadeSamples = 4096)` samples, fixed when the fade starts, so a 32-sample host block still gets at least 85 ms at 48 kHz
- **Per-source HRTF:** 12 independent PartitionedConvolvers for direct binaural rendering
- **Self-calibrating normalization:** `targetRMS = 1/sqrt(irLen)` ensures consistent levels across HRTF profiles
- **Facade boundary (SC-13):** consumers drive rendering through `RenderEngine` and do not dispatch algorithms or build `LayoutContext`s themselves — including the stereo-variant gains (`objGainL`/`objGainR`/`stereoMode`), which the engine computes itself when `engineComputesGains` is set (SC-18 part 2); with the flag false the consumer supplies them

## Build System
- **Framework:** JUCE 9.0.0, C++17, CMake 3.22+
- **Dependencies:** libmysofa v1.3.5 (FetchContent; a consumer that already provides a `mysofa-static` target, as OpenSpatialDelay does with its own v1.3.2 pin, keeps its own copy), zlib (system)
- **HRTF data:** the 5 Git-LFS-tracked SOFA files are embedded at build time as BinaryData (`SpatialCoreHRTFData`, linked privately into `SpatialCore`), so a consumer that links SpatialCore needs no install step and no path. `SPATIALCORE_EMBED_ALL_HRTF` (default ON) embeds all five; OFF embeds only `mit_kemar_large_pinna.sofa` (profile 5) for the future installer (SUITE-01). A file with the same name as a built-in, placed in `/Library/Application Support/Spatial Media Lab/HRTF/` (macOS) or `%ProgramData%\Spatial Media Lab\HRTF\` (Windows, implemented but not verified), overrides that built-in at the next profile switch; no per-user folder is consulted and SpatialCore never creates or writes the folder. Configure stops with "git lfs pull" if an HRTF file is still an LFS pointer. The tests read the source-tree files through `SPATIALCORE_HRTF_DIR`. While OpenSpatialDelay still embeds its own `HRTFData` the two sets are both linked (about 2 x 58 MB) until its migration deletes its copy.
- **Tests:** Catch2 v3.7.1 via FetchContent. Two executables: `SpatialCoreTests` and `SpatialCoreUITests` (the only target that links `SpatialCoreUI`; its ctest names start with `ui:`). The local gate builds every target in Debug and runs both executables, then builds every target in Release and runs `ctest --test-dir build-release/tests`. CI runs zero tests until Phase 6 (CI-01), so this local gate is the only gate. The exact commands are in `README.md` "Local test gate"
- **Demo:** `SpatialCoreDemo` (in `examples/`) sits behind `SPATIALCORE_BUILD_EXAMPLES` (default OFF, top-level builds only). It is the worked example of wiring ADM-OSC, trajectories and the map into `RenderEngine`, with `--screenshots <dir>` and `--selftest` modes

## Versioning
Semantic versioning: `vMAJOR.MINOR.PATCH`
- Major: Breaking API changes (new virtual methods, removed functions)
- Minor: New features (new algorithm, output format, UI widget)
- Patch: Bug fixes

## Consumer Plugins
Plugins link SpatialCore as a git submodule and a CMake subdirectory. `add_subdirectory(SpatialCore)` exposes two link targets: `SpatialCore` (DSP) and `SpatialCoreUI` (shared spatial map + SML look-and-feel + font BinaryData).
```cmake
add_subdirectory(SpatialCore)
target_link_libraries(MyPlugin PRIVATE SpatialCore SpatialCoreUI)
```
A SpatialCore change reaches a consumer only after: commit + push here, then bump the consumer's submodule pointer (`git add SpatialCore && git commit` in the consumer). Consumers today: OpenSpatialDelay (live) and OpenSpatialPanner (next).

## Skills
15 JUCE/DSP skills are available at project level in `.claude/skills/` — they load automatically in this repo.

## Critical Rules
- NEVER allocate memory in any function called from processBlock
- NEVER modify the SpatializationAlgorithm interface without bumping the major version
- ALWAYS maintain backward compatibility with existing plugins when adding features
- ALWAYS run the full test suite before tagging a release
- HRTF profiles are embedded BinaryData built from the Git-LFS `.sofa` files in `HRTF/`. Adding or removing one means editing `kHRTFProfiles` in `Binaural/HRTFProfile.h` and the source list in the top-level `CMakeLists.txt`. Profile indices are persisted in consumers' saved sessions and are NEVER renumbered, and a display name never changes (D-07)
- Adding a spatialization algorithm means editing `AllAlgorithmTypes` in `Algorithms/AllAlgorithms.h`, not just adding an `#include`. That type list is the single source of truth for `NUM_ALGORITHMS` — the count is derived from it, so a header that is included but not listed compiles fine and is silently uncounted. Adding to the list moves the count and deliberately fails the build until every doc surface named in the `static_assert` message is updated with it (D-04)
