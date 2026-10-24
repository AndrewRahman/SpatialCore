# Phase 3: Binaural Defects & HRTF Packaging - Pattern Map

**Mapped:** 2026-10-04
**Files analyzed:** 28 (new and modified)
**Analogs found:** 25 / 28 (3 partial or none, see end)

All analog paths below were checked with `git ls-files` and are tracked. No gitignored mirror paths are used. Line numbers are from the working tree at mapping time (HEAD 284ad28).

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `CMakeLists.txt` (modify: option, `juce_add_binary_data(SpatialCoreHRTFData)`, LFS signature guard, new sources, libmysofa v1.3.5) | config | batch | `CMakeLists.txt:233-236` (`SpatialCoreUIFontData`) and `:46-56` (mysofa pin) | exact |
| `tests/CMakeLists.txt` (modify: 7 new sources) | config | batch | `tests/CMakeLists.txt:9-30` | exact |
| `include/SpatialCore/Binaural/HRTFProfile.h` (new) | model/config (profile table, status enums) | transform | `include/SpatialCore/Core/BinauralGains.h` (small POD header) plus `kDefaultBinauralProfiles` use in `PanningLawTests.cpp:742` | role-match |
| `include/SpatialCore/Binaural/HRTFProfileResolver.h` + `src/Binaural/HRTFProfileResolver.cpp` (new) | service | file-I/O, request-response | `src/Binaural/HRTFDatabase.cpp:36-66` (`loadFromFile` / `loadFromBytes`) | role-match |
| `src/Binaural/EmbeddedHRTF.cpp` (new) | utility | file-I/O (read-only resource) | `SpatialCoreUIFontData::` use in `SMLLookAndFeel` (UI target) | partial |
| `include/SpatialCore/Binaural/HRTFDatabase.h` + `src/Binaural/HRTFDatabase.cpp` (modify: `loadFromBinaryData`) | service | file-I/O | same file, `loadFromMemory` / `loadFromFile` | exact |
| `include/SpatialCore/Core/SimpleBinauralCues.h` (new, pure weights and filter targets) | utility | transform | `include/SpatialCore/Core/BinauralGains.h`, `src/Core/SpatialMath.cpp` (pure functions with FloatSemanticsGuard) | role-match |
| `src/Engine/RenderEngine.cpp` + `include/SpatialCore/Engine/RenderEngine.h` (modify: cue bank in `renderSimpleBinauralWoodworth`, sample-based renderer xfade, mailbox, loader worker, status word, `setHRTFProfile`, new opt-in ctx flag, Simple<->HRTF dual-path xfade) | service | event-driven, streaming | same file: `renderDirectBinauralHRTF` (:237-380), `renderSimpleBinauralWoodworth` (:389-421), `engineDerivesDispatch` (h:176-186, cpp:156-166) | exact |
| `src/Binaural/PartitionedConvolver.cpp` + `.h` (modify: sample-based warm-up and crossfade) | service | streaming | same file (state machine, `kWarmupBlocks`, `kCrossfadeBlocks`) | exact |
| `src/Binaural/BinauralRenderer.cpp` (modify: size scratch buffers in `setProfile()`, fix `:162` resize) | service | streaming | same file `prepare()` :10-20, `updateSourceHRIR` :150-170 | exact |
| `tests/Binaural/BinauralMetrics.h` (new shared helpers) | utility (test) | transform | `tests/Binaural/BinauralTestUtilities.h` | exact |
| `tests/Binaural/PartitionedConvolverTests.cpp` (new) | test | streaming | `tests/Binaural/HutubsPP2Tests.cpp` (structure), `tests/Engine/RenderEngineTests.cpp` (fixtures) | role-match |
| `tests/Binaural/BinauralRendererTests.cpp` (new) | test | streaming | `tests/Binaural/MitKemarLargePinnaTests.cpp` | role-match |
| `tests/Binaural/HRTFEmbeddedTests.cpp` (new) | test | file-I/O | `tests/Binaural/SadieD2KU100Tests.cpp` | role-match |
| `tests/Binaural/BinauralCueTests.cpp` (new) | test | transform | `tests/Engine/RenderEngineTests.cpp:168-195`, `tests/Algorithms/PanningLawTests.cpp:737+` | role-match |
| `tests/Binaural/ProfileLoudnessTests.cpp` (new) | test | batch | `tests/Binaural/SadieD2KU100Tests.cpp` (per-profile load) plus engine fixture | partial |
| `tests/Engine/ProfileSwitchTests.cpp` (new) | test | event-driven | `tests/Engine/RenderEngineTests.cpp` (320+ layout swap test) | role-match |
| `tests/Binaural/{Sadie,Cipic,Hutubs,Bernschuetz,MitKemar}Tests.cpp` (modify: tolerance fingerprint replaces FNV golden) | test | transform | `tests/Binaural/HutubsPP2Tests.cpp:14-17, 38-57` | exact |
| `tests/Algorithms/PanningLawTests.cpp` (modify :737-742 add assertions) | test | transform | same file | exact |
| `CLAUDE.md`, `README.md`, `docs/integration-guide.md`, `docs/development-roadmap.md`, two `.claude/skills` docs, `.planning/PROJECT.md` (modify) | docs | n/a | RESEARCH Pattern 7 list | exact (list) |

## Pattern Assignments

### `CMakeLists.txt` (config, batch)

**Analog:** `CMakeLists.txt:233-240` (embedded data) and `:46-56` (pinned dependency guard).

Embed precedent (copy the shape, change names):
```cmake
juce_add_binary_data(SpatialCoreUIFontData
    HEADER_NAME "SpatialCoreUIFontData.h"
    NAMESPACE SpatialCoreUIFontData
    SOURCES
        fonts/DM_Sans-Regular.ttf
        ...
```
Apply the RESEARCH "Installation" block verbatim (option, source list branch on `SPATIALCORE_EMBED_ALL_HRTF`, `SpatialCoreHRTFData`, `target_link_libraries(SpatialCore PRIVATE SpatialCoreHRTFData)`, `SPATIALCORE_EMBEDS_ALL_HRTF` compile definition). The existing link block is `CMakeLists.txt:145-149`:
```cmake
target_link_libraries(SpatialCore
    PRIVATE
        mysofa-static
        ZLIB::ZLIB
)
```
Add `SpatialCoreHRTFData` beside it. New `.cpp` files go into the `add_library(SpatialCore STATIC` list under the `# Binaural` comment (`:68-71`), one line each: `src/Binaural/HRTFProfileResolver.cpp`, `src/Binaural/EmbeddedHRTF.cpp`.

libmysofa bump (D-17): edit only `GIT_TAG v1.3.2` at `:50` to `v1.3.5`. The `if(NOT TARGET mysofa-static)` guard at `:46` must stay (OSD coexistence). The `HRTFData` namespace in OSD does not collide with `SpatialCoreHRTFData`.

LFS signature guard (Pattern 6 in RESEARCH): configure-time `file(READ ... LIMIT 8 HEX)` check. The existing guard to complement (not replace) is the CI shell step `.github/workflows/ci.yml:19-35` (greps for `version https://git-lfs.github.com/spec/v1`, size under 100000 bytes).

---

### `tests/CMakeLists.txt` (config, batch)

**Analog:** itself, `tests/CMakeLists.txt:9-30`. Plain hand-maintained list; add one line per new test file, in the same grouping style:
```cmake
    Binaural/WoodworthFallbackTests.cpp
    Engine/RenderEngineTests.cpp
    Engine/TripleBufferIndexTests.cpp
```
`SPATIALCORE_HRTF_DIR` (`:45-48`) stays for source-tree comparison; embedded tests must NOT depend on it for resolution (only to compare bytes). Single owner plan for this file (RESEARCH note).

---

### `src/Binaural/HRTFProfileResolver.cpp` + header (service, file-I/O)

**Analog:** `src/Binaural/HRTFDatabase.cpp:36-66`.

Failure contract to rely on (`loadFromBytes` unloads first and leaves the database unloaded on failure):
```cpp
bool HRTFDatabase::loadFromBytes (const void* data, int dataSize, float targetSampleRate)
{
    unload();
    ...
    easyHandle = mysofa_open_data (static_cast<const char*> (data), static_cast<long> (dataSize),
                                   targetSampleRate, &filterLength, &err);
    if (easyHandle == nullptr || err != MYSOFA_OK)
    {
        DBG ("HRTFDatabase: Failed to load SOFA data, error code: " + juce::String (err));
        easyHandle = nullptr;
        loaded = false;
        return false;
    }
```
Copy the resolver body from RESEARCH Pattern 2 (shared file via `existsAsFile()` then `loadFromFile`, else embedded `loadFromMemory`, else NotFound). D-18: use `existsAsFile()` only, no directory listing. Platform folder: RESEARCH Pattern 5 (`#if JUCE_MAC` literal path, `JUCE_WINDOWS` via `commonApplicationDataDirectory`). Start the `.cpp` like other guarded TUs:
```cpp
#include "../Core/FloatSemanticsGuard.h"   // WR-04: no fast-math in this TU
```
(as at `src/Engine/RenderEngine.cpp:2`). Namespace `spatialcore`. Size cap before `loadFileAsData` (Security row V5).

---

### `src/Binaural/EmbeddedHRTF.cpp` (utility)

**Analog:** no in-tree non-UI consumer of `getNamedResource`; closest is the font-data embed at `CMakeLists.txt:233-236`. Only this TU includes `<SpatialCoreHRTFData.h>`; expose a small function (for example `const char* getEmbeddedHRTF(const char* resourceName, int& size)`) declared in a public-header-free internal header under `src/Binaural/`. Never include the generated header from `include/`. Use RESEARCH Pattern 2 table (`kHRTFProfiles`) for resource names (`sadie_d2_ku100_sofa`, etc.).

---

### `include/SpatialCore/Binaural/HRTFProfile.h` (model)

**Analog:** `include/SpatialCore/Core/BinauralGains.h` (small header-only value types in namespace `spatialcore`). Content comes from RESEARCH Pattern 2 (`kHRTFProfiles[6]`, index equals D-07 numbering) and Pattern 3 (status packed into one `std::atomic<uint32_t>`, `describe(status)` building a string on the reader thread; status is an enum, never file contents).

---

### `src/Engine/RenderEngine.cpp` / `RenderEngine.h` (service, event-driven + streaming)

**Analog:** same file; four sub-patterns to copy.

**(a) Opt-in ctx flag (D-15, new `engineSelectsHRTF` or similar).** Copy the declaration and doc-comment style of `engineDerivesDispatch` (`RenderEngine.h:176-186`): default `false`, comment names which fields it overrides, "additive, not a major bump". Copy the dispatch hook at `RenderEngine.cpp:156-166`:
```cpp
const RenderBlockContext* dispatchCtx = &blockCtx;
if (blockCtx.engineComputesGains || blockCtx.engineDerivesDispatch)
{
    gainScratch_ = blockCtx;                       // assignment into existing member, no allocation
    if (blockCtx.engineDerivesDispatch)
        deriveDispatchFromLayout (layout, gainScratch_);
    if (blockCtx.engineComputesGains)
        computeObjectGains (src, layout, gainScratch_);
    dispatchCtx = &gainScratch_;
}
```
Extend the condition with the new flag; set `gainScratch_.useHRTF` from the active renderer's `isSimpleMode()`. The dispatch chain below it (`:174-180`: `isBinaural && useHRTF` then `isBinaural`) is selected once and must not be duplicated.

**(b) Renderer crossfade made sample-based (BUG-02/criterion 4).** Existing block-count logic to change, `RenderEngine.cpp:332-374`:
```cpp
++rendererXfadeBlockCount_;
float progress = static_cast<float> (rendererXfadeBlockCount_)
               / static_cast<float> (kRendererXfadeBlocks);
if (progress > 1.0f) progress = 1.0f;
constexpr float halfPi = juce::MathConstants<float>::halfPi;
float fadeOutGain = std::cos (progress * halfPi);
float fadeInGain  = std::sin (progress * halfPi);
float fadeOutInc = (fadeOutGain - prevRxFadeOut_) / static_cast<float> (numSamples);
```
Replace the counter by an elapsed-sample counter with length `max(kRendererXfadeBlocks * numSamples, 4096)` fixed once at start (RESEARCH Pattern 4). Keep the cos/sin equal-power law and the per-sample ramp loop.

**(c) Swap detection and mailbox claim.** Existing detection, `RenderEngine.cpp:245-254`:
```cpp
int currentActiveIdx = activeRendererIndex.load (std::memory_order_acquire);
if (currentActiveIdx != prevActiveRendererIdx_ && ! rendererXfading_) { rendererXfading_ = true; ... }
```
Add the `readyRenderer_.exchange(-1, acquire)` claim immediately before this, only when not crossfading, per RESEARCH Pattern 3. Keep `swapActiveRenderer()` / `getPrepareRendererIndex()` (`RenderEngine.h:301-307`) public and unchanged. Atomic members sit beside `std::atomic<int> activeRendererIndex { 0 }` and `std::atomic<bool> rendererXfadeActive_ { false }` (`RenderEngine.h:389-398`).

**(d) Simple-path cue bank.** Insert in `renderSimpleBinauralWoodworth` (`RenderEngine.cpp:389-421`). Existing per-sample loop to modify:
```cpp
float objMono = (sources.monoBuffers[t] != nullptr) ? sources.monoBuffers[t][s] : 0.0f;
if (tapFade <= 0.0f) continue;
float gL = prevBinauralGains[t].leftGain + frac * (blockCtx.objGains[t].leftGain - prevBinauralGains[t].leftGain);
...
wetL += objMono * gL * tapFade;
```
Apply the cue bank to `objMono` before the pan gains. Interpolate the blend weights with the same `frac` method and store previous weights next to `prevBinauralGains[MAX_SOURCES]` (`RenderEngine.h:418`); end-of-function carry-forward mirrors `:417-419` (`prevBinauralGains[t] = blockCtx.objGains[t]`). Do not touch the ITD delays (RESEARCH: no reader of `DelaySamples` in `src/Engine`). `continue` on `tapFade <= 0` must still not leave filter state stale: RESEARCH says run filters every block for live sources.

**(e) Allocation discipline.** Buffers sized in `prepare()` (`RenderEngine.cpp:23+`), never in the render path; the new loader worker may allocate, but only `juce::Thread` on the worker side. Include `FloatSemanticsGuard.h` already present at `:2`.

---

### `src/Binaural/PartitionedConvolver.cpp` + `.h` (service, streaming)

**Analog:** same file. Block-count constants to convert, `PartitionedConvolver.h:83-91`:
```cpp
enum class State { Idle, Warmup, Crossfading };
static constexpr int kCrossfadeBlocks = 4;
static constexpr int kWarmupBlocks = 1;
int stateBlockCount = 0;
```
Add `stateSampleCount`; warm-up ends when `stateBlockCount >= kWarmupBlocks && stateSampleCount >= irLen`; crossfade length `max(kCrossfadeBlocks * numSamples, 2048)` fixed once at Warmup->Crossfading and stored in a member (RESEARCH Pattern 4, caution about per-call recompute). Keep the pending-IR deferral at `PartitionedConvolver.cpp:100-110`. The overlap-add in `processSlot` (`:114-168`) is already correct and must not change. Guards to preserve: `jassert (numSamples > 0 && numSamples <= blockSize);` and WR-02 runtime guard in `process()`.

---

### `src/Binaural/BinauralRenderer.cpp` (service, streaming)

**Analog:** same file. Audio-thread resize to eliminate, `:158-166`:
```cpp
if ((int) tmpL.size() < storedIRLength)
{
    jassertfalse;  // Audio thread allocation -- should have been pre-allocated in prepare()
    tmpL.resize (static_cast<size_t> (storedIRLength));
```
Fix per RESEARCH Pitfall 4: in `setProfile()` (worker thread) size `convTmpL/R` to `max(blockSize, irLen)`. Existing sizing to extend lives at `prepare()` `:10-20`:
```cpp
size_t preAllocSize = static_cast<size_t> (std::max (maxBlockSize, 512));
convTmpL.resize (preAllocSize, 0.0f);
```
Leave the `:207` site and the ITD buffer (`kITDBufferSize = 64`) alone (D-16, Phase 5).

---

### `include/SpatialCore/Core/SimpleBinauralCues.h` (utility, transform)

**Analog:** `src/Core/SpatialMath.cpp` style (pure functions in `namespace spatialcore`, radians in, `std::cos/sin`) and `include/SpatialCore/Core/BinauralGains.h` for a small struct return. Weights from RESEARCH Pattern 1:
```
wRear = max(0, -cos(az) * cos(el));  wUp = max(0, sin(el));  wDown = max(0, -sin(el))
```
Header-only inline, no state (DR-7). Sanitise non-finite input at the call site in the `.cpp` (FloatSemanticsGuard TU). Convention note: az 0 = front, +el up (see the `evalSH` header comment, `src/Core/SpatialMath.cpp:12-17`).

---

### `tests/Binaural/BinauralMetrics.h` (test utility)

**Analog:** `tests/Binaural/BinauralTestUtilities.h` (57 lines): `#pragma once`, doc comment block, `namespace spatialcore::test`, `inline` functions, `kTestSampleRate`. Reuse `fnv1aHash` (`:27-38`), `getSofaFile` (`:51-54`), and `kTestSampleRate` rather than redefining. Third-octave metric, K-weighting biquads (RESEARCH Code Examples) and click/step ratio go here. Numeric tolerance helpers already exist at `tests/TestNumerics.h` (tracked), include it as `RenderEngineTests.cpp:5` does.

---

### Engine-level tests: `tests/Engine/ProfileSwitchTests.cpp`, `tests/Binaural/BinauralCueTests.cpp`, `tests/Binaural/ProfileLoudnessTests.cpp`, BUG-02 steady-state/moving tests

**Analog:** `tests/Engine/RenderEngineTests.cpp`.

Header/using block (`:1-15`):
```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <SpatialCore/Engine/RenderEngine.h>
#include <SpatialCore/Core/SpatialMath.h>
#include "../Binaural/BinauralTestUtilities.h"
#include "../TestNumerics.h"
using namespace spatialcore;
using Catch::Matchers::WithinAbs;
```
Source fixture (`:28-55`, `SourceFixture::makeSources`): one live object at slot 0 with `monoBuffers`, `tapFadeGainPerSample`, `distGainPerSample`, `objectLive`, `objects[0]`. Reuse the same field set, but the fixture's `kBlockSize` is file-local, so parametrise block size in the new tests.

Engine setup and HRTF dispatch (`:120-162`):
```cpp
RenderEngine engine;
engine.prepare (kSampleRate, kBlockSize);
engine.setOutputFormat (OutputFormat::Binaural);
RenderBlockContext ctx;
ctx.sampleRate = kSampleRate;
ctx.isBinaural = true;
ctx.useHRTF = true;
float* outPtrs[2] = { outL.data(), outR.data() };
engine.renderBlock (sources, ctx, outPtrs, 2);
```
Helper style `allFinite`/`anyNonzero` (`:57-79`). Tag style: `"[engine][renderblock][...]"`; new tags per RESEARCH Validation map (`[bug01]`, `[bug02]`, `[hrtf-switch]`, `[loudness]`).

Synchronous HRTF load for pre-`setHRTFProfile` tests: `engine.getBinauralRenderer(i)` (`RenderEngine.h:290`) then `HRTFDatabase::loadFromFile` using `getSofaFile`, per the LANDMINE note at `BinauralTestUtilities.h:3-11`.

For D-14 use `WARN()` for printed numbers; assert only finiteness and a loose bound.

---

### Per-profile unit tests: `PartitionedConvolverTests.cpp`, `BinauralRendererTests.cpp`, `HRTFEmbeddedTests.cpp`

**Analog:** `tests/Binaural/HutubsPP2Tests.cpp` (65 lines), structure: anonymous-namespace constants, three `TEST_CASE`s (sync load, golden, negative control), tags `[binaural][hutubs][...]`.
```cpp
HRTFDatabase db;
REQUIRE_FALSE (db.isLoaded());
bool ok = db.loadFromFile (getSofaFile (kSofaFile), static_cast<float> (kTestSampleRate));
REQUIRE (ok);
CHECK (db.getIRLength() == kGoldenIRLength);
```
For `BinauralRendererTests.cpp` also read `tests/Binaural/MitKemarLargePinnaTests.cpp` (102 lines, KEMAR is profile 5 with the LF shelf). Negative-control pattern (`:53-65`) is the template for "profile 0 leaves `sourceConvReady` false and passes silence".

---

### Golden fingerprint edit: the 5 profile test files (esp. `HutubsPP2Tests.cpp:14-17, 38-57`)

**Analog:** the same file. Replace the byte-exact comparison:
```cpp
uint64_t checksum = hashHRIRPair (irL.data(), irR.data(), db.getIRLength());
CHECK (checksum == kGoldenChecksum);
CHECK (std::abs (delayL) < std::abs (delayR));
```
with delays (integers), per-ear energy and peak value/index using `Catch::Matchers::WithinRel (1e-5)` (RESEARCH Pitfall 6). Remember D-17: if the libmysofa bump moves any value, surface before/after numbers to the user before re-baselining.

---

### `tests/Algorithms/PanningLawTests.cpp` (modify, `:737-742`)

**Analog:** same test. Property test with lambda `at`:
```cpp
TEST_CASE ("DirectBinaural: property checks at the horizon (Discretion)", "[panning-law][directbinaural]")
{
    // ... No elevation or front/back assertion -- BUG-01 changes those in Phase 3
    DirectBinauralAlgorithm algo;
    const BinauralContext bctx { 1, 48000.0, kDefaultBinauralProfiles };
    auto at = [&] (float azDeg, float distance)
    { return algo.computeBinauralGains ({ juce::degreesToRadians (azDeg), 0.0f, distance }, bctx); };
```
Update the comment; the distinguishability assertion itself lives in the engine-level `BinauralCueTests.cpp` (cue state is engine-side, algorithm stays stateless; `BinauralGains` unchanged).

---

## Shared Patterns

### Fast-math guard in every new `.cpp` with `std::isfinite`
**Source:** `src/Core/FloatSemanticsGuard.h` included at `src/Engine/RenderEngine.cpp:2`, `src/Core/SpatialMath.cpp:2`.
**Apply to:** `HRTFProfileResolver.cpp`, `EmbeddedHRTF.cpp`, any cue-bank source, `RenderEngine.cpp`, `PartitionedConvolver.cpp`.
```cpp
#include "FloatSemanticsGuard.h"   // WR-04: no fast-math in this TU
```

### Opt-in, default-false context flags (backward compatibility)
**Source:** `include/SpatialCore/Engine/RenderEngine.h:176-186` (`engineDerivesDispatch`), `RenderEngine.cpp:156-166`.
**Apply to:** the D-15 Simple<->HRTF flag and any other new `RenderBlockContext` field.

### Wait-free handoff with a single audio-side writer
**Source:** `include/SpatialCore/Engine/TripleBufferIndex.h` (tracked) plus `RenderEngine.h:389-398` atomics.
**Apply to:** the `readyRenderer_` mailbox and `rendererFree_` flags. Release store by the worker, acquire exchange by the audio thread; no locks or allocation on the audio side (DR-1).

### Status readable without lock or allocation
**Source:** pattern from `rendererXfadeActive_` (`std::atomic<bool>` with `memory_order_acquire` reads, `RenderEngine.h:307`).
**Apply to:** the packed `std::atomic<uint32_t>` HRTF status word; strings built from a static table on the reader thread.

### Embedded-data naming
**Source:** `CMakeLists.txt:233-236`. Name `SpatialCoreHRTFData` (header `SpatialCoreHRTFData.h`, namespace identical) to avoid OSD's `HRTFData`.

### Test file structure and tags
**Source:** `tests/Binaural/HutubsPP2Tests.cpp` (per-profile three-test shape) and `tests/Engine/RenderEngineTests.cpp` (engine fixture). Tags are lowercase bracket groups; new files must be added to `tests/CMakeLists.txt` by hand.

### Discretion decisions the planner must record
Cue-filter values (RESEARCH Pattern 1 starting table), API name (`setHRTFProfile`), status shape, `OFF` KEMAR-only behaviour (name-based `getNamedResource`, no `#if` in lookup), SharedFFTCache leak report (here or Phase 5), CLAUDE.md lines 87 and 112 rewrite.

## No Analog Found

| File | Role | Data Flow | Reason |
|---|---|---|---|
| Loader worker thread (inside `RenderEngine`) | service | event-driven | No `juce::Thread` or worker exists in `src/`; use RESEARCH Pattern 3 (`juce::Thread` + `juce::WaitableEvent` + atomic latest-request) |
| Simple<->HRTF dual-path crossfade | service | streaming | Existing renderer xfade blends two HRTF renderers only (`RenderEngine.cpp:332-374`); reuse its cos/sin ramp loop but the two-path render is new (RESEARCH Pattern 4, MEDIUM confidence, not prototyped) |
| `EmbeddedHRTF.cpp` | utility | file-I/O | No in-tree DSP-side `getNamedResource` consumer; only fonts in `SpatialCoreUI` |

## Metadata

**Analog search scope:** `src/`, `include/SpatialCore/{Binaural,Engine,Core}`, `tests/{Binaural,Engine,Algorithms}`, `CMakeLists.txt`, `tests/CMakeLists.txt`, `.github/workflows/ci.yml`
**Files scanned:** about 30 (targeted reads and greps; `RenderEngine.cpp` read in the ranges above only)
**Tracked-source check:** `git ls-files` listing used for `src/`, `include/`, `tests/`, `HRTF/`, and `tests/TestNumerics.h`
**Pattern extraction date:** 2026-10-04
