# Phase 5: Realtime Safety Hardening - Pattern Map

**Mapped:** 2026-10-06
**Files analyzed:** 17 new/modified
**Analogs found:** 15 / 17 (2 have no in-repo analog: the malloc trap and the lock-trap dylib)

All analog paths below were checked as git-tracked source (no `.gsd` capability mirrors involved).
`tests/CMakeLists.txt` is CRLF: every edit must preserve CRLF (use a CRLF-aware edit, then check `git diff --stat` shows only the added lines). Root `CMakeLists.txt` and README are LF.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `src/Binaural/BinauralRenderer.cpp` (+ `.h` counter and helper) | service (DSP) | transform, audio-thread guard | `RenderEngine::getActiveLayout()` counter+assert (`src/Engine/RenderEngine.cpp:1695-1713`) | role-match (pattern source for counter) |
| `src/Engine/RenderEngine.cpp` oversized-block sites (1135, 1148, 1209), chunking (D-11) | service | transform / batch (chunk loop) | `PartitionedConvolver::process` chunking (`src/Binaural/PartitionedConvolver.cpp:181-196`) | exact (pattern) |
| `src/Engine/RenderEngine.cpp` NFC coefficients (:1533) | service | transform | same line, in-place assign (Research Pattern 5) | self |
| `src/Engine/RenderEngine.cpp` + `.h` legacy-route misuse counter (D-09) | service | request-response | `getWriterViewOnRenderThreadCount` (`RenderEngine.h:415-421,758`, `.cpp:1695-1724`) | exact |
| `include/SpatialCore/Trajectory/TrajectoryEngine.h` + `src/Trajectory/TrajectoryEngine.cpp` (RTSF-02) | model/state | cross-thread handoff | `active_` (`std::atomic<bool>` array, `.h:116`) | exact |
| `include/SpatialCore/Binaural/HRTFDatabase.h` / `src/Binaural/HRTFDatabase.cpp` (RTSF-03 contract + in-use detection) | service | CRUD (load vs lookup) | SC-17 counter pattern; ownership via mailbox (CLAUDE.md DATA-01) | role-match |
| `src/Binaural/PartitionedConvolver.cpp` / `.h` (D-08 doc; test seam if needed) | utility | singleton cache | itself (`getOrCreate`, lines 7-29) | self |
| `tests/Support/RealtimeGuard.h` (alloc trap, canary) | test utility | event-driven hook | none in repo (Research Pattern 1) | no analog |
| `tests/Support/rtguard/RTGuard.cpp` (lock-trap dylib) | test utility | interposition | none in repo (Research Pattern 2) | no analog |
| `tests/Engine/RealtimeSafetyTests.cpp` | test | request-response over matrix | `tests/Engine/ProfileSwitchTests.cpp`, `tests/Support/RouteRenderRig.h` | role-match |
| `tests/Engine/MultiInstanceTests.cpp` | test | concurrent | `ProfileSwitchTests.cpp` `[threads]`/churn tests + `runWithWatchdog` | role-match |
| `tests/Binaural/SharedFFTCacheTests.cpp` | test | concurrent | `tests/Core/VBAPTripletSelectionTests.cpp:215-243` (`runWithWatchdog`) | role-match |
| `tests/Trajectory/TrajectoryTests.cpp` (append `[trajectory][threads]`) | test | concurrent | same file | exact |
| `tests/UI/MultiMapTests.cpp` | test | UI paint | `tests/UI/MapRouteTests.cpp`, `tests/UI/UITestSupport.h` | exact |
| `tests/Engine/RenderEngineTests.cpp:118-` CR-03 oversized test (change to assert chunking) | test | transform | itself | self |
| `tests/CMakeLists.txt` (CRLF) | config | build | itself lines 9-91 | self |
| `README.md` "Local test gate" (+ `CLAUDE.md`, integration guide) | docs | -- | README lines 110-130 | exact |
| root `CMakeLists.txt` (`build-tsan` option / flags) | config | build | existing LF top-level file | role-match |

## Pattern Assignments

### `src/Binaural/BinauralRenderer.cpp` guards (RTSF-01; D-03, D-04)

**Sites to replace:** `:178-183` (`updateSourceHRIR`) and `:223-228` (`renderSourceBuffers`). Current code:
```cpp
// :178
if ((int) tmpL.size() < storedIRLength)
{
    jassertfalse;  // Audio thread allocation -- should have been pre-allocated in prepare()
    tmpL.resize (static_cast<size_t> (storedIRLength));
    tmpR.resize (static_cast<size_t> (storedIRLength));
}
// :223 (after the memsets of outL/outR at :219-221)
if (convTmpL.size() < static_cast<size_t> (numSamples))
{
    jassertfalse;
    convTmpL.resize (static_cast<size_t> (numSamples));
    convTmpR.resize (static_cast<size_t> (numSamples));
}
```
Also delete the stale comment at `:174-177` ("dealt with in Phase 5; deliberately left as is").

**Counter + assert pattern to copy** (`src/Engine/RenderEngine.cpp:1695-1724`, header decl `RenderEngine.h:421`, member `:758` `mutable std::atomic<int> ... { 0 };`):
```cpp
writerViewOnRenderThreadCount_.fetch_add (1, std::memory_order_relaxed);
jassertfalse;
...
int RenderEngine::getWriterViewOnRenderThreadCount() const noexcept
{
    return writerViewOnRenderThreadCount_.load (std::memory_order_relaxed);
}
```
**Target shape** (Research Pattern 4, one shared helper; counter name discretionary):
```cpp
bool BinauralRenderer::scratchFits (size_t needed) noexcept
{
    if (convTmpL.size() >= needed && convTmpR.size() >= needed) return true;
    scratchTooSmallCount_.fetch_add (1, std::memory_order_relaxed);
    jassertfalse;
    return false;
}
// updateSourceHRIR:    if (! scratchFits ((size_t) storedIRLength)) return;  // keeps previous HRIR
// renderSourceBuffers: if (! scratchFits ((size_t) numSamples)) return;       // after the memset
```
Test seam: unreachable via public API, so add a TEST-ONLY seam (precedent `setLoaderFailureForTesting`). Expose a public const `getScratchTooSmallCount()` (additive, minor bump). Note `jassertfalse` aborts in Debug, so the guard test must be written to run only where asserts are off (Release/RelWithDebInfo) or the seam must suppress the assert; decide in the plan.

### `src/Engine/RenderEngine.cpp` oversized-block sites (D-11, chunking)

**Sites (verbatim today):**
- `:1130-1137` `jassertfalse; sourceAccumStorage_.assign (MAX_SOURCES * numSamples, 0.0f);` then re-points `sourceAccumBufPtrs[src]`.
- `:1145-1149` `wetBufL_.resize (numSamples, 0.0f); wetBufR_.resize (...)` (no assert).
- `:1209` `if (xfadeWetL_.size() < ns) { xfadeWetL_.resize (ns, 0.0f); xfadeWetR_.resize (ns, 0.0f); }`

**Chunk precedent to copy:** `src/Binaural/PartitionedConvolver.cpp:181-196` (WR-02, process in prepared-size chunks). Read it at plan time; the structure is a loop advancing pointers by `blockSize`. Apply at the `renderBlock` entry (or at `renderDirectBinauralHRTF`'s caller) so each chunk is at most the prepared max, with per-block state (layout acquire, fades, smoothers) advanced once, not per chunk. Careful: the layout snapshot is acquired once per block (SC-16/17) and format-switch fades are per block; chunking must stay below that level or fade timing changes.

**Existing test to change:** `tests/Engine/RenderEngineTests.cpp:118-` ("survives an oversized block without heap corruption", `[engine][renderblock][oversized]`). Currently prepares at `kBlockSize` and renders `kBlockSize * 4`, asserting only finiteness. Change it to assert full-length correct output (compare against a render of the same input at the prepared block size in pieces) and zero allocations via the trap.

### `src/Engine/RenderEngine.cpp:1533` Ambisonics NFC (F1; D-10 bit-identical)

Current:
```cpp
*nfcFilters[t][ord].coefficients =
    juce::dsp::IIR::Coefficients<float> (b0 / a0, b1 / a0, 1.0f, a1 / a0);
```
Replace with the in-place array assign (Research Pattern 5):
```cpp
*nfcFilters[t][ord].coefficients = std::array<float, 4> { b0 / a0, b1 / a0, 1.0f, a1 / a0 };
```
Proof: trap reports 0 allocs with moving distance on formats 17-22 from block 1, plus a before/after bit comparison (capture a pre-fix render to a test fixture or compare in the same commit via the Phase 3 golden approach). Fix the stale comment at `:1499-1500` too. Do this first: it is the only red result today.

### Legacy-route misuse detection (D-09)

**Analog:** the SC-17 pattern above (thread-hash fields `renderThreadHash_`, `writerThreadHash_`, `currentThreadHash()` at `RenderEngine.cpp:1726-1730`). Methods to cover: `getBinauralRenderer` (`RenderEngine.h:429`), `getPrepareRendererIndex` (`:438`), `swapActiveRenderer` (`:439`). Doc comment style to extend: `RenderEngine.h:484-485` ("Mixing these calls ... is unsupported"). Copy the shape: `if (renderer != 0 && me == renderer) { legacyRouteMisuseCount_.fetch_add (1, relaxed); jassertfalse; }` plus a `const noexcept` getter next to `getWriterViewOnRenderThreadCount`. These methods are inline in the header, so the check must move out-of-line or the counter be reachable from inline code; keep signatures unchanged (D-09 reversibility).

### `TrajectoryEngine` (RTSF-02)

**Analog:** `active_` at `TrajectoryEngine.h:116` and its accessor at `:98`:
```cpp
bool  isActive (int i) const { return active_[i].load (std::memory_order_relaxed); }
std::atomic<bool> active_[kMaxObjects] = {};
```
Change `:113-115` to `std::atomic<float> finalAz_[kMaxObjects] = {};` (and El, Dist; `is_always_lock_free` verified) and accessors `:95-97` to `.load (std::memory_order_relaxed)`.
Writers: `TrajectoryEngine.cpp:76-81` write `finalAz_[t]` twice (then wraps it) and `:88-90`, `:143-145`. Pattern: compute into locals, then store each once (compute-then-store), so a reader never sees the pre-wrap azimuth:
```cpp
float az = input.originAz + az_; ... az = wrapAzimuth (az);
finalAz_[t].store (az, std::memory_order_relaxed);
```
Also audit other direct users of the public `finalAz_` arrays (header comment says "read by processBlock"; grep consumers incl. `examples/`, tests) since the type change breaks plain reads. Torn triples across az/el/dist are accepted by D-discretion only if not audible; document.
**Test:** append to `tests/Trajectory/TrajectoryTests.cpp` (tags style `"[trajectory][edge]"`, `using namespace spatialcore;`, Catch2 `TEST_CASE`) a `[trajectory][threads]` test with a ticking thread and a reading thread; it only proves a race under the TSan tree (show the plain-float version reports a race for D-07).

### `HRTFDatabase` (RTSF-03)

Members `HRTFDatabase.h:126-129` (`MYSOFA_EASY* easyHandle`, `bool loaded`); `isLoaded()` at `:94` reads `loaded` plainly. Lookup `mysofa_getfilter_float` at `HRTFDatabase.cpp:289`. Contract (Research F5): lookups write scratch inside the handle, so one database is single-thread; safety is ownership plus mailbox (CLAUDE.md "Profile loader and mailbox"). Plan: document the thread contract in the header; add an in-use flag or misuse counter following the SC-17 shape; do NOT add a lock. Make `loaded` `std::atomic<bool>` only if the TSan run reports it. D-10: any fix must pass the Phase 3 binaural goldens unchanged.

### `SharedFFTCache` (VERIFY-02, D-08)

**Source:** `src/Binaural/PartitionedConvolver.cpp:7-29`:
```cpp
std::shared_ptr<juce::dsp::FFT> SharedFFTCache::getOrCreate (int fftOrder)
{
    juce::SpinLock::ScopedLockType sl (lock);
    auto it = cache.find (fftOrder);
    if (it != cache.end()) return it->second;
    auto ptr = std::make_shared<juce::dsp::FFT> (fftOrder);
    cache[fftOrder] = ptr;
    return ptr;
}
SharedFFTCache& getSharedFFTCache() { static SharedFFTCache instance; return instance; }
```
Add the D-08 comment ("create-once-never-destroy, deliberate permanent allocation, not a leak"); fix the comment's issue reference (it says OSD#131; Research traced the cache to OSD PR #142 / #96). Callers reaching it: `PartitionedConvolver::prepare` and `HRTFDatabase::convertToMinPhase` (`HRTFDatabase.cpp:376`), both non-audio threads. Only lock in the audio-path inventory is this SpinLock (not seen by the lock trap), so the source-scan test covers it.

### `tests/Support/RealtimeGuard.h` and `tests/Support/rtguard/RTGuard.cpp` (no analog)

No in-repo analog. Use Research Patterns 1 and 2 verbatim (`malloc_logger` hook counting only `pthread_self()` armed, NOT `thread_local`; dyld `__DATA,__interpose` tuples in a SHARED lib under `if(APPLE)`). Header-only style precedent: `tests/Support/RouteRenderRig.h` (`namespace spatialcore::test`, `inline`/header helpers). Every use must first REQUIRE the canary (a deliberate malloc and a CriticalSection lock are seen); non-Apple builds `SKIP`. The trap is blind under TSan, so `[rt-alloc]` tests must `SKIP` (or be excluded by tag) in `build-tsan`.

### `tests/Engine/RealtimeSafetyTests.cpp` (matrix)

**Analog:** `tests/Engine/ProfileSwitchTests.cpp` (helpers `timeoutScale()` lines 106-122 reading `SPATIALCORE_TEST_TIMEOUT_SCALE`; temp-folder fixture; engine setup via `engine.prepare (kSampleRate, kBlockSize); engine.setOutputFormat (OutputFormat::Binaural);` as in `RenderEngineTests.cpp:118-135`) and `tests/Support/RouteRenderRig.h` for the rendering rig. Matrix to cover (Research F3 "not covered" list): 23 formats, block sizes 32/64/256/512/1024, profiles 0-5 with `engineSelectsHRTF`, all three flags on/off in combination, `setHRTFProfile` and format switches with and without `engineFadesFormatSwitch` including across a renderer crossfade, `engineComputesGains=false`, trajectories animating (drive positions from `TrajectoryEngine::tick`), arm the trap from the first block after `prepare()` and move az/el/dist every block.

### `tests/Engine/MultiInstanceTests.cpp`, `tests/Binaural/SharedFFTCacheTests.cpp`

**Analog:** `runWithWatchdog` at `tests/Core/VBAPTripletSelectionTests.cpp:215-243` (copy or lift to `tests/Support/`; it spawns a worker, polls `done`, detaches on timeout):
```cpp
template <typename Fn>
bool runWithWatchdog (Fn fn, std::chrono::milliseconds limit)
{
    auto done = std::make_shared<std::atomic<bool>> (false);
    std::thread worker ([fn = std::move (fn), done]() mutable { fn(); done->store (true); });
    ...
}
```
Use it around every test run against the mutated (D-07) library, since the unlocked cache spins forever. Tests are deterministic invariants (cache identity, retention, per-instance HRTF isolation for OSD#96), not a reproduction of the vDSP crash (did not reproduce on this machine). Tag `[multi]`, `[fftcache]`, plus `[tsan]` for the subset the TSan tree runs (use `ctest -L`/Catch tag filter). Each Catch2 test runs in its own process under `catch_discover_tests`, which makes cold-start tests meaningful. Scale timing with `SPATIALCORE_TEST_TIMEOUT_SCALE` (10 under TSan).

### `tests/UI/MultiMapTests.cpp` (D-06, D-12)

**Analog:** `tests/UI/MapRouteTests.cpp` (includes `catch2/catch_test_macros.hpp`, `"UITestSupport.h"`, `using spatialcore::SpatialMapComponent;`, tags `"[ui][route][map][tracer]"`) and `tests/UI/UITestSupport.h` (headless component building, `makeMouseEvent`, paint-into-`juce::Image` helper at line ~40; GUI initialiser held by `UITestMain.cpp`). Copy: build N `SpatialMapComponent`s with no window, `setSize`, paint each into an Image, REQUIRE no crash and that the typeface objects are shared (compare `juce::Typeface::Ptr` identity or the typeface-count invariant). For provenance/source-scan style tests see `tests/UI/FontProvenanceTests.cpp:294-343` (comment stripper). No SMLLookAndFeel changes (D-12).

### `tests/CMakeLists.txt` (CRLF)

Edit points (from the file):
```cmake
add_executable(SpatialCoreTests
    ...
    Engine/ControlRouteTests.cpp     # line ~35: append new Engine/Binaural/Support-backed files here
)
target_compile_definitions(SpatialCoreTests PRIVATE
    SPATIALCORE_HRTF_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../HRTF"
    JUCE_UNIT_TESTS=1
    JUCE_MODAL_LOOPS_PERMITTED=1
)                                    # add SPATIALCORE_SOURCE_DIR here for the source-scan rule
add_executable(SpatialCoreUITests    # add UI/MultiMapTests.cpp after UI/FontProvenanceTests.cpp
```
New `add_library(SpatialCoreRTGuard SHARED ...)` under `if(APPLE)` linked PRIVATE into `SpatialCoreTests`. The TSan tree uses `FETCHCONTENT_SOURCE_DIR_JUCE/_MYSOFA/_CATCH2` to reuse fetched sources. Never add `-ffast-math` (build error via `FloatSemanticsGuard.h`).

### `README.md` "Local test gate" (lines 110-130)

Copy the fenced-bash block style (Debug then Release then `ctest ... --output-on-failure`); add a third block for `build-tsan` (`-DCMAKE_BUILD_TYPE=RelWithDebInfo` plus `-fsanitize=thread`, run the tagged subset with `SPATIALCORE_TEST_TIMEOUT_SCALE=10`) and the `[rt-alloc]` pass. State "proved on macOS (Apple Silicon); Windows and Linux are checked in Phase 6" (D-13). Update `CLAUDE.md` Design Principles (RTSF-01 note currently says it is Phase 5) and the integration guide.

## Shared Patterns

### Lock-free counter + Debug assert (SC-17)
**Source:** `src/Engine/RenderEngine.cpp:1695-1724`, `include/SpatialCore/Engine/RenderEngine.h:415-421,758`
**Apply to:** BinauralRenderer scratch guard, legacy-route misuse, HRTFDatabase in-use detection.
```cpp
mutable std::atomic<int> writerViewOnRenderThreadCount_ { 0 };
int getWriterViewOnRenderThreadCount() const noexcept;   // relaxed load, any thread
// at the site: counter.fetch_add (1, std::memory_order_relaxed); jassertfalse;
```
Every new counter: relaxed atomic int, `const noexcept` getter, doc comment saying which thread may read it, additive API only.

### Cross-thread float handoff
**Source:** `TrajectoryEngine.h:98,116` (`std::atomic`, relaxed). **Apply to:** `finalAz_/finalEl_/finalDist_`.

### Mutation evidence (D-07)
**Source:** Phase 3 SUMMARYs (03-06, 03-07). **Apply to:** every regression test: temporarily reintroduce the bug, show red, restore, record in SUMMARY. Wrap any run against a mutated cache in `runWithWatchdog`.

### Bit-identity oracle (D-10)
**Source:** Phase 3 binaural goldens (03-02). **Apply to:** NFC fix, any HRTF lookup change. If not bit-identical: stop and ask, never re-baseline.

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| `tests/Support/RealtimeGuard.h` | test utility | hook | no allocation trap exists; use Research Pattern 1 |
| `tests/Support/rtguard/RTGuard.cpp` | test utility | interposition | no dylib or interposition in the repo; use Research Pattern 2 |

## Metadata

**Analog search scope:** `src/Engine`, `src/Binaural`, `src/Trajectory`, `include/SpatialCore/{Engine,Trajectory,Binaural}`, `tests/{Engine,Core,Trajectory,UI,Support}`, `tests/CMakeLists.txt`, `README.md`
**Files scanned:** about 20 (targeted reads and greps; RESEARCH.md read through line 380 of 751, the sections after it, Validation Architecture and open questions, were not re-read here)
**Pattern extraction date:** 2026-10-06
