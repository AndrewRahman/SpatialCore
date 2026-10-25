# Phase 5: Realtime Safety Hardening - Research

**Researched:** 2026-10-06
**Domain:** Realtime-safety proof tooling (ThreadSanitizer, allocation and lock trapping) and four targeted fixes in a JUCE 9 / C++17 / CMake audio library, on macOS arm64 (Apple clang 21)
**Confidence:** HIGH for the tooling findings and the code sites (every one was run or read this session); MEDIUM for the Windows/Linux claims (not runnable here)

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** **A separate check, run at every phase end gate and before any release tag.** It is a
  third build tree (optimised with debug info, i.e. RelWithDebInfo, plus ThreadSanitizer) and a
  no-allocation test pass. The everyday Debug + Release local gate stays as it is today. README's
  "Local test gate" documents the extra step. Phase 3 (03-07) found an -O0 instrumented SADIE load
  exceeds the engine's 15 s `stopThread`, so the race-checker build must be optimised.
- **D-02:** **The proof walks every render path, every output format and every switch.** All 5
  render paths, all 23 output formats, plus mid-play HRTF profile switches (`setHRTFProfile`,
  including 0 <-> SOFA), output-format switches with and without `engineFadesFormatSwitch`, all three
  opt-in engine flags (`engineComputesGains`, `engineDerivesDispatch`, `engineSelectsHRTF`), and
  trajectories animating. "Whole audio path" means whole; a format-specific allocation must not slip
  through.
- **D-03:** **Keep the last good sound; never allocate.** If an audio-thread buffer is found too
  small (reachable only on a prepare-contract violation), skip just that update: in
  `updateSourceHRIR` the source keeps its previous HRIR; in `renderSourceBuffers` the affected
  work is skipped for that block rather than resized. No silence-by-design, no allocation. Prefer one
  shared guarded helper over two copies (REQUIREMENTS RTSF-01).
- **D-04:** **Count it and assert in Debug.** A lock-free counter readable from any thread (tests and
  plugins can read it) plus a Debug-build assert, matching the SC-17
  `getWriterViewOnRenderThreadCount()` pattern. Shipped builds stay silent apart from the counter.
  Public API addition is additive only (minor bump).
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

### Claude's Discretion

- **Allocation detector mechanism (D-01/D-02):** the user said "you decide". Options: a test-only
  allocation trap (operator new/malloc hook armed around `renderBlock`) that works with the current
  toolchain, Clang's RealtimeSanitizer (`-fsanitize=realtime`) where the toolchain supports it, or
  both. Recommendation recorded during discussion was both. Locks must be detected too, not only
  allocations.
- RTSF-02 fix shape: atomic floats vs an atomically swapped struct (REQUIREMENTS allows either);
  must keep the per-object az/el/dist triple from tearing in a way that is audible.
- Exact counter names and where the shared guarded helper lives.

### Deferred Ideas (OUT OF SCOPE)

- Real-DAW multi-instance check (OSD loaded several times in Reaper): happens with the OSD migration, outside this repo.
- Removing the legacy HRTF route (`getBinauralRenderer` / `swapActiveRenderer`): next major version.
- Running the sanitizer build in CI: Phase 6 (CI-01).
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| RTSF-01 | `BinauralRenderer` cannot allocate on the audio thread in a Release build (two sites, SpatialCore#19) | Sites confirmed on HEAD at `BinauralRenderer.cpp:178-183` and `:223-228`; the same defect class exists at three more sites in `RenderEngine.cpp` (1135-1136, 1148-1149, 1209) which a probe shows allocate in Release (Findings F2). Shared-helper design and a deterministic test seam are below. |
| RTSF-02 | `TrajectoryEngine` final positions safe to read from the audio thread (SpatialCore#18) | Race confirmed by reading; fix shape (three `std::atomic<float>` arrays, single store per value, compute-then-store) recommended. TSan demonstration plan in Validation Architecture. |
| RTSF-03 | `HRTFDatabase` libmysofa handle access is thread-safe | Contract analysis (Findings F5): `mysofa_getfilter_float` writes a scratch buffer inside the handle, so "const" lookups are not concurrency-safe on one database. D-09 detection design (in-use flag + render-thread identity) and the TSan proof on the `setHRTFProfile` route. |
| RTSF-05 | Whole audio path verified lock-free and realtime-safe by tools | Working, run-this-session tooling: `malloc_logger` allocation trap, dyld-interposed lock trap, ThreadSanitizer RelWithDebInfo tree. The trap found one real unscoped allocation (Ambisonics NFC, F1). |
| VERIFY-02 | `SharedFFTCache` safe across concurrent plugin instances (SpatialCore#12; OSD#96, OSD#131 regressions) | Provenance trace of #96/#131 (PR #142), a deterministic per-instance-state regression design, a note that the vDSP twiddle-table mechanism did not reproduce on this machine, and a UI sharing test design. |
</phase_requirements>

## Project Constraints (from CLAUDE.md)

- **NEVER allocate memory in any function called from processBlock** (DR-1). Locks and logging are in the same locked rule (PROJECT.md).
- **NEVER modify the `SpatializationAlgorithm` interface without a major bump** (DR-7). Nothing in this phase touches it; the RTSF-01/05 fixes live in `BinauralRenderer`, `RenderEngine`, `TrajectoryEngine`, `HRTFDatabase`.
- **ALWAYS maintain backward compatibility with existing plugins**; public API additions are additive (minor bump). Opt-in flags default false; flag-off output stays byte-for-byte unchanged.
- **ALWAYS run the full test suite before tagging a release.** The local gate is the only gate (CI runs zero tests until Phase 6).
- Lock-free audio path: no malloc, locks or logging in any function reachable from `renderBlock()`. `DBG()` in `BinauralRenderer`/`HRTFDatabase` is configuration-thread only and out of scope (ROADMAP scope correction).
- `SpatialCore`'s fast-math guard (`src/Core/FloatSemanticsGuard.h`) makes `-ffast-math` a build error; sanitizer flags must not add it.
- Tests: Catch2 v3.7.1, two executables (`SpatialCoreTests`, `SpatialCoreUITests`; its ctest names start with `ui:`). `tests/CMakeLists.txt` is **CRLF** in the git index (`git ls-files --eol` shows `i/crlf`), so an edit must preserve CRLF line endings or the diff rewrites the whole file. `CMakeLists.txt` (top level) is LF.
- Skills: `.claude/skills/juce-best-practices/SKILL.md` Realtime Safety checklist (no malloc/free, no locks, no logging, atomics for cross-thread state) is the project's own wording of DR-1; nothing in it contradicts the recommendations below.
- The user is not a programmer: Claude runs every check and brings back only plain-English judgment calls (CONTEXT specifics, auto-memory).

## Summary

The proof tooling the phase needs **exists and works on this machine today, without installing anything**. Apple clang 21 does not support `-fsanitize=realtime` (compiler error, verified), and Homebrew LLVM is not installed, so RealtimeSanitizer is unavailable here. Instead, three tools were built and exercised this session: (1) a ThreadSanitizer tree (`RelWithDebInfo`, `-fsanitize=thread`) that builds in a few minutes and links Apple's `libclang_rt.tsan_osx_dynamic.dylib`; (2) an in-process **allocation trap** built on libmalloc's `malloc_logger` hook, armed per thread around `renderBlock()`, which counts malloc, calloc, realloc, posix_memalign, `operator new` and frees including those made inside vDSP and libmysofa; (3) a tiny **lock trap** (a shared library using dyld `__interpose` for `pthread_mutex_lock`, `pthread_rwlock_*` and `pthread_cond_wait`) that catches `std::mutex` and `juce::CriticalSection`. Both traps were shown to fire on canaries before any "zero" was believed.

The traps found **one real realtime violation that CONTEXT does not list**: the Ambisonics output path (6 of the 23 formats) allocates on the audio thread whenever an object's smoothed distance moves, because `RenderEngine.cpp:1533` builds a temporary `juce::dsp::IIR::Coefficients<float>` (whose constructor allocates) for every object and every order. A 12-object, 120-block run measured 1,416 allocations at first order and 8,496 at sixth order. All 17 non-Ambisonics formats, all 12 algorithm indices on seven formats, profile switches (including to and from Simple), and format switches with and without the fade measured **zero** allocations from the very first block. The fix is a bit-identical in-place assignment (`std::array<float,4>`), so D-10 is satisfied by construction and a golden comparison proves it. The oversized-block guards in `RenderEngine.cpp` (three more `resize`/`assign` sites, plus the two named `BinauralRenderer` sites) also allocate in Release (5 allocations measured in 3 oversized blocks); fixing them changes what an oversized host block does, which needs a user decision (Open Question 1).

Two provenance findings affect the VERIFY-02 plan. OSD#131's issue text describes a CoreGraphics glyph crash caused by per-instance typeface duplication; the FFT cache came from OSD PR #142, which *hypothesised* a vDSP twiddle-table race and verified it only by running 12-16 instances in REAPER. A stress test of that vDSP mechanism (8 threads, 4,000 create/use/destroy cycles each, three runs) found **no mismatch on this Apple Silicon / macOS 26 machine**, so the "show the test fails against the old behaviour" step (D-07) cannot rely on that crash reproducing here; it needs deterministic invariants instead (cache identity and retention, per-instance HRTF isolation, shared typeface count).

**Primary recommendation:** Build the proof as ordinary Catch2 tests in the existing everyday gate (an in-process `malloc_logger` allocation trap plus a small interposed lock-trap dylib, macOS only, each self-checked by a canary) and add the third tree (`build-tsan`, RelWithDebInfo + `-fsanitize=thread`) that runs a tagged thread-safety subset; fix the Ambisonics NFC allocation first, because it is the only place the proof is red today.

## Architectural Responsibility Map

SpatialCore is a C++ library, not a web tier stack. The "tiers" below are the library's own thread/ownership tiers.

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Block rendering (all 5 paths, 23 formats) | Audio thread (`RenderEngine::renderBlock`) | -- | DR-1 applies here; everything the trap arms around. |
| Scratch/buffer sizing, FFT setup, profile loading | Message / loader thread (`prepare`, `setProfile`, `HRTFProfileLoader`) | -- | Allocation is allowed here; the audio-side guards must never fall back to it. |
| Trajectory animation producing positions | Message thread (60 Hz timer calls `tick`) | Audio thread reads `getFinal*` | The only producer/consumer float handoff that is not already atomic (RTSF-02). |
| libmysofa handle ownership | Loader thread writes a renderer the audio thread is not using | Audio thread reads the active one | Safety is by exclusive ownership + mailbox, not by locking (D-09). |
| Process-global FFT cache | Process (static singleton) | Message/loader (`prepare`-time creation) | Shared by every engine in the process; the multi-instance surface (VERIFY-02). |
| Misuse counters and Debug asserts | Any thread (lock-free atomics) | Test code reads them | SC-17 pattern (`getWriterViewOnRenderThreadCount`). |
| Race / allocation / lock proof | Test harness (Catch2 in-process) + a third build tree | -- | Test-only; nothing ships. |
| UI typeface sharing | Message thread (`SpatialCoreUI`) | -- | OSD#131's surface; painting is message-thread code. |

## Standard Stack

### Core (tooling; nothing new is installed)

| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| Apple clang | 21.0.0 (clang-2100.3.34.2), target arm64-apple-darwin25.6.0, Command Line Tools only [VERIFIED: `clang --version`, `xcode-select -p`] | Compile and link everything, including the sanitizer tree | The installed toolchain; the only one the local gate uses |
| ThreadSanitizer (`-fsanitize=thread`) | runtime `libclang_rt.tsan_osx_dynamic.dylib` in `/Library/Developer/CommandLineTools/usr/lib/clang/21/lib/darwin` [VERIFIED: `ls`] | Race detection (RTSF-02, RTSF-03, RTSF-05, VERIFY-02) | Phase 3 (03-07) already used it on this repo; found a real race on the current TrajectoryEngine in this session |
| libmalloc `malloc_logger` hook | system libmalloc on Darwin 25.6.0 [VERIFIED: ran, counted malloc/calloc/realloc/posix_memalign/operator new] | In-process allocation trap armed around `renderBlock` | The only allocation detector that works with the installed toolchain (RealtimeSanitizer is not available, see Alternatives) |
| dyld `__DATA,__interpose` shared library | system dyld [VERIFIED: tuples in a linked dylib fire, tuples in the main executable do not] | Lock trap: `pthread_mutex_lock`, `pthread_mutex_trylock`, `pthread_rwlock_rdlock/wrlock`, `pthread_cond_wait` | Catches `std::mutex` and `juce::CriticalSection` with no source change |
| Catch2 | v3.7.1 (existing FetchContent pin, `tests/CMakeLists.txt`) | Test framework for every new test | Already the project standard; `catch_discover_tests` runs every TEST_CASE in its own process under ctest, which is what makes cold-start tests meaningful |
| CMake | 4.4.4 installed (project minimum 3.22) [VERIFIED: `cmake --version`] | Build trees | Existing; `ninja` is not installed, the default Makefiles generator works [VERIFIED] |

### Supporting

| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| `FETCHCONTENT_SOURCE_DIR_JUCE` / `_MYSOFA` / `_CATCH2` | CMake feature | Point extra build trees at one already-fetched copy of each dependency | Configuring `build-tsan` after any other tree exists: first configure took 77 s (fetching), a configure that reuses the fetched sources took 15 s [VERIFIED: timings] |
| `SPATIALCORE_TEST_TIMEOUT_SCALE` | existing env var (`tests/Engine/ProfileSwitchTests.cpp:106-122`) | Stretch watchdogs and polls under sanitizer slowdown | Every TSan run (Phase 3 used 10) |
| `runWithWatchdog` | existing helper (`tests/Core/VBAPTripletSelectionTests.cpp:217`; `ProfileSwitchTests.cpp` has a similar one taking a name and seconds) | Turn a hang into a failure | Any test that runs a mutated (deliberately broken) library: removing the cache lock made the process spin for 9+ CPU-minutes [VERIFIED] |
| `lldb` | system | Debug a crashing trap | Only if a trap crashes |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `malloc_logger` trap | RealtimeSanitizer (`-fsanitize=realtime`) | **Not available here.** `clang++ -fsanitize=realtime` fails with `error: unsupported option '-fsanitize=realtime' for target 'arm64-apple-darwin25.6.0'`; no Homebrew LLVM is installed (`ls /opt/homebrew/opt \| grep -i llvm` empty); the CLT resource dir has no rtsan runtime [VERIFIED]. It would also flag locks and blocking calls in one tool. Recommended for the Phase 6 Linux CI leg (clang 20+ ships it) [ASSUMED: from training knowledge, not checked this session]. Installing `brew install llvm` just for this is not recommended: a multi-hundred-MB install for a tool the in-process trap already replaces on this platform. |
| `malloc_logger` trap | Global `operator new/delete` replacement | Misses every `malloc` made by JUCE `HeapBlock`, vDSP and libmysofa (C code); the probe needed those. Only useful as a cross-platform complement. |
| `malloc_logger` trap | Replace the default malloc zone's function pointers | Heavier (page protection) and riskier; `malloc_logger` gave the same counts in 30 lines. |
| Combined TSan + allocation trap in one tree | -- | **Impossible as built**: under `-fsanitize=thread` the `malloc_logger` canary counted 0 allocations (`canary: allocs=0 frees=0`), because TSan installs its own allocator [VERIFIED]. They run in separate trees (D-01 already says so). |
| Interposing `os_unfair_lock_lock` | -- | Tried: the process aborted (SIGABRT) [VERIFIED]. Not needed: nothing in SpatialCore uses it. |
| Seqlock or locked struct for TrajectoryEngine | three `std::atomic<float>` arrays | A 12-byte `std::atomic<struct>` is not lock-free and would hide a lock under the hood; a seqlock makes the reader retry. Atomic floats keep the audio read wait-free. See F-RTSF-02. |

**Installation:** none. No package is added; the lock-trap shared library is a new CMake target built from ~60 lines in this repo.

**Version verification:** `atomic<float>::is_always_lock_free` is `1` with `sizeof == 4` on this toolchain [VERIFIED: compiled `static_assert`]. The dependency pins already in the tree (JUCE 9.0.0, libmysofa v1.3.5, Catch2 v3.7.1) are unchanged.

## Package Legitimacy Audit

No external package is installed or added by this phase (the only new binary is a repo-local shared library). The existing FetchContent dependencies (JUCE 9.0.0 at `CMakeLists.txt:33`, libmysofa v1.3.5, Catch2 v3.7.1) are untouched, so the `package-legitimacy check` gate has nothing to evaluate.

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---------|----------|-----|-----------|-------------|---------|-------------|
| (none added) | -- | -- | -- | -- | -- | -- |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

## Architecture Patterns

### System Architecture Diagram

```
                       +--------------------------- local gate (D-01) ----------------------------+
                       |                                                                           |
 everyday gate         |   build (Debug)   build-release (Release)       build-tsan (RelWithDebInfo|
 (unchanged + new      |        |                 |                      + -fsanitize=thread)     |
  [rt] tests)          |        v                 v                                |               |
                       |  SpatialCoreTests / SpatialCoreUITests            SpatialCoreTests         |
                       |        |                                          "[tsan]" subset only     |
                       +--------|-------------------------------------------------|----------------+
                                |                                                 |
        +-----------------------+---------------------+                           v
        v                                             v                  TSan runtime watches every
 [rt-alloc] / [rt-lock] tests                  [multi] / [fftcache]      thread; exit != 0 or any
        |                                      stress tests              "WARNING: ThreadSanitizer"
        |  1. canary: trap must fire on a                                 line fails the gate
        |     deliberate malloc + CriticalSection
        |  2. build engine, prepare, set format/profile/flags
        |  3. ARM trap on THIS thread  --->  RenderEngine::renderBlock (audio-thread stand-in)
        |                                        |  paths: DirectBinauralHRTF | Simple (Woodworth) | Stereo
        |                                        |         Ambisonics | Discrete surround
        |                                        |  reads: layout slot (wait-free), trajectory finals
        |                                        |         (atomic), HRTF renderer (mailbox-claimed)
        |  4. DISARM; read counts            <---+  malloc_logger hook counts alloc/free for the armed
        |     alloc == 0, free == 0,                pthread_self() only; interposed pthread_mutex_lock
        |     lock == 0                             counts locks; loader/message threads are not armed
        v
 Catch2 REQUIRE (fails loudly, includes the first offending backtrace via backtrace())
```

Trace the primary use case by following the arrows: a test prepares an engine on the message-thread stand-in, arms the trap on the render thread, renders blocks, disarms, and asserts the counts; the same tests run in the TSan tree except the trap ones (the trap is blind under TSan), where the thread-safety subset runs instead.

### Recommended Project Structure

```
tests/
├── Support/
│   ├── RealtimeGuard.h            # RAII arm/disarm + canary; macOS implementation, other platforms: tests SKIP
│   └── rtguard/RTGuard.cpp        # source of the lock-trap shared library (SpatialCoreRTGuard, APPLE only)
├── Engine/
│   ├── RealtimeSafetyTests.cpp    # [rt-alloc][rt-lock] matrix: formats x flags x switches x block sizes
│   └── MultiInstanceTests.cpp     # [multi] many engines, isolation (OSD#96), concurrent first use
├── Binaural/
│   └── SharedFFTCacheTests.cpp    # [fftcache] identity, retention, cross-order isolation, first-use race
├── Trajectory/
│   └── TrajectoryTests.cpp        # + [trajectory][threads] concurrent tick/read (add to existing file)
└── UI/
    └── MultiMapTests.cpp          # [ui][multi] several maps alive and painted at once
```

Add each new file to the explicit list in `tests/CMakeLists.txt` (preserve CRLF). The shared library is a new `add_library(SpatialCoreRTGuard SHARED ...)` under `if(APPLE)`, linked PRIVATE into `SpatialCoreTests`.

### Pattern 1: Per-thread allocation trap (macOS)

**What:** A function installed into libmalloc's `malloc_logger` pointer, which counts only when `pthread_self()` equals the armed thread.
**When to use:** Around every `renderBlock()` call in an `[rt-alloc]` test, after a canary has proven the trap fires.
**Example:**
```cpp
// Source: verified this session (macOS 26 / Darwin 25.6.0, arm64, Apple clang 21). malloc_logger is a libmalloc
// SPI symbol, not declared in a public header; the type bits are alloc = 2, dealloc = 4 (realloc = 0xe, calloc 0x4a).
extern "C" {
typedef void (malloc_logger_t) (uint32_t type, uintptr_t a1, uintptr_t a2, uintptr_t a3,
                                uintptr_t result, uint32_t skip);
extern malloc_logger_t* malloc_logger;
}
namespace {
std::atomic<pthread_t> gArmed { nullptr };   // NOT thread_local: see Pitfall 1
std::atomic<int> gAllocs { 0 }, gFrees { 0 };
void hook (uint32_t type, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uint32_t)
{
    if (gArmed.load (std::memory_order_relaxed) != pthread_self()) return;   // other threads are never counted
    if (type & 2) gAllocs.fetch_add (1, std::memory_order_relaxed);
    if (type & 4) gFrees.fetch_add (1, std::memory_order_relaxed);
}
}
struct ScopedAllocTrap {
    ScopedAllocTrap() { gAllocs = 0; gFrees = 0; malloc_logger = hook; gArmed.store (pthread_self()); }
    ~ScopedAllocTrap() { gArmed.store (nullptr); }
};
```
A first-offender backtrace (`backtrace()` into a preallocated array, only when `gAllocs` goes 0 to 1 and with `gArmed` cleared while it runs) turned the Ambisonics finding into a one-line diagnosis (`Coefficients<float>::Coefficients(float,float,float,float)` called from `renderAmbisonicsOutput`).

### Pattern 2: Lock trap by interposition

**What:** A small shared library whose `__DATA,__interpose` table redirects the pthread locking calls to counters, then forwards to the real function.
**When to use:** Same `[rt-*]` tests; one extra `REQUIRE (locks == 0)`.
**Example:**
```cpp
// Source: verified this session. Tuples in the MAIN executable are ignored (measured 0 hits); tuples in a dylib that the
// executable links fire for every image, including libc++'s std::mutex and juce::CriticalSection (measured 2 hits for 2 locks).
extern "C" int rt_mutex_lock (pthread_mutex_t* m) { noteLockIfArmed(); return pthread_mutex_lock (m); }
// ... trylock, rwlock rd/wr, cond_wait the same way
struct Interpose { const void* repl; const void* orig; };
__attribute__((used)) static const Interpose kTuples[] __attribute__((section ("__DATA,__interpose"))) = {
    { (const void*) rt_mutex_lock, (const void*) pthread_mutex_lock },   // plus the others
};
```
It does **not** see `juce::SpinLock` or any atomic spin (no system call). Cover those with the source-scan rule below.

### Pattern 3: Source-scan rule for lock primitives (precedent: `[ui][fonts][rule]`)

**What:** A Catch2 test that reads `src/` and `include/` and fails if a lock primitive appears in a file reachable from `renderBlock()` outside an allow-list.
**When to use:** To cover `juce::SpinLock`, which the trap cannot see. Today the complete lock inventory is: `SharedFFTCache::getOrCreate` SpinLock (`PartitionedConvolver.cpp:16`), reached only from `PartitionedConvolver::prepare` and `HRTFDatabase::convertToMinPhase` (`HRTFDatabase.cpp:376`); `sharedFolderLock_` CriticalSection (`RenderEngine.cpp:356,362`, message and loader threads only) [VERIFIED: `grep` over `src` and `include`]. The existing UI font-rule tests already scan source text with a comment stripper (`tests/UI/FontProvenanceTests.cpp:294-343`); reuse the stripper idea, and add `SPATIALCORE_SOURCE_DIR` to `SpatialCoreTests` (today only `SpatialCoreUITests` defines it).

### Pattern 4: Guarded audio-side fallback (RTSF-01, D-03, D-04)

**What:** One helper that answers "does this scratch buffer fit?", counts a miss in a lock-free counter, asserts in Debug, and lets the caller skip the work. No `resize` on the audio thread.
**When to use:** Both `BinauralRenderer` sites, and the three `RenderEngine` oversized-block sites if Open Question 1 is accepted.
**Example:**
```cpp
// Shape only; names are discretionary (CONTEXT). Header: std::atomic<int> scratchTooSmallCount_ { 0 };
bool BinauralRenderer::scratchFits (size_t needed) noexcept
{
    if (convTmpL.size() >= needed && convTmpR.size() >= needed)
        return true;
    scratchTooSmallCount_.fetch_add (1, std::memory_order_relaxed);   // any thread may read it
    jassertfalse;                                                      // Debug only; JUCE logs and breaks only under a debugger
    return false;                                                      // caller skips: no resize, ever
}
// updateSourceHRIR:   if (! scratchFits (storedIRLength)) return;     // the source keeps its previous HRIR (D-03)
// renderSourceBuffers: if (! scratchFits (numSamples))   return;      // after the existing memset, so the output is zero, not stale
```
Note the order in `renderSourceBuffers`: the existing code zeroes `outL/outR` first (`BinauralRenderer.cpp:219-221`), so an early return leaves a defined (silent for this renderer's contribution) block rather than stale memory; D-03 says "affected work is skipped", and a convolver that did not run also did not advance its state, which is the "last good sound" the user asked for.

### Pattern 5: Allocation-free coefficient update (the Ambisonics fix)

**What:** Assign a `std::array<float,4>` to the existing `Coefficients` object instead of constructing a temporary `Coefficients`.
**When to use:** `RenderEngine.cpp:1533`.
**Example:**
```cpp
// Source: JUCE 9.0.0 modules/juce_dsp/processors/juce_IIRFilter.h:161 (operator= from std::array) and juce_IIRFilter_Impl.h:41-56 (assignImpl).
// assignImpl does clearQuick() + ensureStorageAllocated (max (8, Num)) + add(): no reallocation when the array already has capacity >= 8,
// which the placeholder built at RenderEngine.cpp:535-536 guarantees. a0Inv = 1 / 1.0f, so the stored values are exactly b0/a0, b1/a0, a1/a0,
// the same floats the temporary produced: bit-identical (D-10).
*nfcFilters[t][ord].coefficients = std::array<float, 4> { b0 / a0, b1 / a0, 1.0f, a1 / a0 };
```
The alternative with no JUCE-internals reasoning is writing the three floats through `coefficients->getRawCoefficients()` (`juce_IIRFilter.h:269`). Either is proven by the trap (0 allocations, moving distance, all six orders) and by a before/after bit comparison.

### Anti-Patterns to Avoid

- **Treating "zero allocations after a warm-up" as proof.** The Ambisonics allocation only happens while distance is moving and the lazy first-use paths only on the first block. Arm from the very first block after `prepare()` and move az, el and distance every block.
- **`thread_local` in the trap.** It crashes on threads that are being created (Pitfall 1).
- **A trap with no canary.** Under TSan it silently counts 0; a green result must be preceded by a REQUIRE that a deliberate `malloc` was seen.
- **Re-baselining a golden to make a fix pass.** D-10: stop and ask.
- **Adding a lock to make `HRTFDatabase` "thread-safe".** DR-1 forbids it on the audio path; the contract is ownership plus detection (D-09).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Race detection | A home-grown stress harness that "looks for glitches" | ThreadSanitizer | Reports the exact two stacks; found the TrajectoryEngine race in the first run |
| Allocation counting | `operator new` replacement or a wrapper allocator | libmalloc `malloc_logger` | Sees `HeapBlock`, vDSP and libmysofa allocations that `operator new` cannot |
| Lock detection | Wrapping every lock in the code with counters | dyld interposition of the pthread calls | Catches third-party and standard-library locks without touching them |
| Cross-thread float handoff | A custom lock or double buffer | `std::atomic<float>` (lock-free, 4 bytes) | Smallest change, wait-free, already the project's idiom (`active_`, layout slots) |
| Test-failure hangs | Manual timeouts in each test | `runWithWatchdog` | Already in the repo; the mutated cache hangs forever without it |
| Process-global FFT sharing | A second cache | Existing `SharedFFTCache` | Already correct under TSan with its lock (clean in a 16-thread first-use test; races reported when the lock is removed) |

**Key insight:** every tool needed is already on the machine or already in the repo; the work is wiring them into repeatable tests and fixing what they find.

## Findings by Requirement (measured or read this session)

All probe numbers below come from a temporary Catch2 file (`tests/Engine/RTProbe.cpp`, removed again; not committed) built in a RelWithDebInfo tree (`NDEBUG`, so `jassert` is a no-op like Release), 48 kHz, 256-sample blocks, 12 live objects moving az, el and distance every block, trap armed on the render thread only.

### F1. A real, unlisted allocation: Ambisonics NFC coefficients (RTSF-05, D-02)

`RenderEngine.cpp:1533` assigns a *temporary* `juce::dsp::IIR::Coefficients<float>` to each object's filter coefficients every time the smoothed distance moves by more than 0.01 m. The temporary's constructor allocates (`assignImpl` calls `ensureStorageAllocated (max (8, Num))`, `juce_IIRFilter_Impl.h:41-56`). The code comment above it at `:1499-1500` ("No allocation: coefficients are assigned first-order into state already sized for first order") is true of the *state* but not of the temporary.

| Output format | Order | Allocations in 120 moving blocks, cold start (alloc count = free count) |
|---------------|-------|---------|
| 17 AmbisonicsFOA | 1 | 1,416 |
| 18 AmbisonicsSOA | 2 | 2,832 |
| 19 AmbisonicsHOA | 3 | 4,248 |
| 20 Ambisonics4OA | 4 | 5,664 |
| 21 Ambisonics5OA | 5 | 7,080 |
| 22 Ambisonics6OA | 6 | 8,496 |

It is identical for Simple, SADIE and KEMAR variants and every flag combination (the Ambisonics path does not use the HRTF renderer). Captured stack (first offender): `Coefficients<float>::assignImpl<4>` <- `Coefficients<float>::Coefficients(float,float,float,float)` <- `RenderEngine::renderAmbisonicsOutput`. It was invisible to a 40-block warm-up (the distance filter settles, then the gate `abs (smoothed - prev) > 0.01` stops firing), which is why a cold start with moving distance matters. Fix: Pattern 5 above (bit-identical). SpatialCore#21 already removed a different allocation at this same spot (the second-order placeholder); the temporary was missed.

### F2. Oversized-block resizes (RTSF-01 + three sites CONTEXT does not list)

On HEAD the confirmed sites are:

| Site | Code (verbatim) | Fires when |
|------|-----------------|------------|
| `BinauralRenderer.cpp:178-183` (`updateSourceHRIR`) | `if ((int) tmpL.size() < storedIRLength) { jassertfalse; tmpL.resize (...); tmpR.resize (...); }` | scratch smaller than the loaded IR; **unreachable through the public API today** (`prepare()` and `setProfile()` both call `ensureScratchCapacity()`), so a test needs a TEST-ONLY seam (precedent: `setLoaderFailureForTesting`, accepted in STATE Deferred Items) |
| `BinauralRenderer.cpp:223-228` (`renderSourceBuffers`) | `if (convTmpL.size() < static_cast<size_t> (numSamples)) { jassertfalse; convTmpL.resize (...); convTmpR.resize (...); }` | block longer than the prepared maximum |
| `RenderEngine.cpp:1135-1136` | `jassertfalse; sourceAccumStorage_.assign (MAX_SOURCES * numSamples, 0.0f);` then re-derives `sourceAccumBufPtrs` | same |
| `RenderEngine.cpp:1148-1149` | `wetBufL_.resize (numSamples, 0.0f); wetBufR_.resize (...)` (no assert at all) | same |
| `RenderEngine.cpp:1209` | `if (xfadeWetL_.size() < ns) { xfadeWetL_.resize (ns, 0.0f); xfadeWetR_.resize (ns, 0.0f); }` | same, during a renderer crossfade |

Measured: rendering three blocks of 4x the prepared size on the HRTF path gave **5 allocations / 5 frees** in a Release-class build, first offender `std::vector<float>::assign` inside `renderDirectBinauralHRTF` (called from `renderBinauralWithProfileFade`). ROADMAP's old line numbers (`:196-209`, `:134,158-163`) are stale; CONTEXT's `:178-183` / `:223-228` match HEAD. An existing test (`tests/Engine/RenderEngineTests.cpp:121`, "survives an oversized block without heap corruption", CR-03) renders 4x blocks and only asserts finite output, so changing these sites changes what it exercises (Open Question 1).

Also relevant: `PartitionedConvolver::process` already splits an oversized block into prepared-size chunks (`PartitionedConvolver.cpp:181-196`, WR-02), so the repo has a precedent for "chunk it" in addition to "skip it".

### F3. Everything else measured zero (RTSF-05, D-02) — and what that does not cover

Zero allocations and zero `pthread` locks, armed from the **first** block after `prepare()` (separate process per combination), 120 blocks of moving az, el, distance:

- Formats 0-16 (Binaural, Stereo, Quad ... 9.1.6, SML 13.1; every non-Ambisonics format) x four variants: no profile (Simple Woodworth), SADIE loaded, SADIE + `engineSelectsHRTF`, KEMAR + `engineSelectsHRTF`. All with `engineComputesGains` and `engineDerivesDispatch` set.
- All 12 algorithm indices (`setAlgorithmIndex` 0..11) on formats 1, 3, 5, 11, 14, 15, 16.
- Under render, armed on the render thread only: `setHRTFProfile` SADIE -> Bernschuetz crossfade, switch to profile 0 (Simple), `setOutputFormat` toggling Binaural <-> 7.1.4 with and without `engineFadesFormatSwitch`.
- Lock trap over all 23 formats, first 20 blocks each with `engineSelectsHRTF`: 0 locks (canary: 2 locks seen for one `CriticalSection` plus one `std::mutex`).
- libmysofa's per-block lookup (`mysofa_getfilter_float` -> kd-tree `mysofa_lookup` -> `mysofa_interpolate`) did not allocate for SADIE or KEMAR while sources moved. D-10 did not trigger. Reading `easy.c:184-220`, `lookup.c:96-117` and `interpolate.c` agrees: only the constructors in `easy.c`, `lookup.c`, `neighbors.c` and `kdtree.c` allocate.

**Not covered by the probe, so the real matrix must add them:** block sizes other than 256 (32, 64, 512, 1024), other sample rates, profiles 2, 3, 4 on the moving HRTF path, `engineComputesGains = false` (consumer-supplied gains), `engineSelectsHRTF = false` with `ctx.useHRTF = true` (OpenSpatialDelay's own flow; variant 1 above rendered the Simple path), `engineFadesFormatSwitch` across a Binaural -> non-binaural switch during a renderer crossfade (the open milestone-audit item), the three flags on one binaural block together (the other open milestone-audit item), trajectories animating (they feed positions; the engine itself never calls `TrajectoryEngine`), and a stereo block with `engineComputesGains` false.

### F4. Lock inventory (RTSF-05)

Only two lock objects exist in `src/` and `include/` outside the UI: `SharedFFTCache::lock` (a `juce::SpinLock`, `SharedFFTCache.h:21`, taken in `getOrCreate`, `PartitionedConvolver.cpp:16`) and `RenderEngine::sharedFolderLock_` (a `juce::CriticalSection`, `RenderEngine.h:654`, message and worker threads only). `getOrCreate` is called from `PartitionedConvolver::prepare` and `HRTFDatabase::convertToMinPhase` (a public static utility nothing in the repo calls). Neither is reachable from `renderBlock()`. Function-local statics (`SpeakerLayout.cpp:82` `cached`, `BinauralRenderer.cpp:67` `refDirs`) are initialised on the message thread (`setOutputFormat`, `setProfile`).

### F5. RTSF-03: what is really shared inside libmysofa

`mysofa_getfilter_float` -> `mysofa_getfilter_float_advanced` calls `mysofa_interpolate (easy->hrtf, c, nearest, neighbors, easy->fir, delays)` (`easy.c:184-220`): **`easy->fir` is a scratch buffer inside the handle that every lookup writes.** `HRTFDatabase::getInterpolatedHRIR` is declared `const` (`HRTFDatabase.h:53-55`) yet is therefore not safe to call concurrently on one database. Consequences:

- The engine-owned route (`setHRTFProfile`) is safe by exclusive ownership: the loader writes a renderer the audio thread is not using, and `BinauralRenderer::setProfile` (which itself calls `getInterpolatedHRIR` six times for the normalisation, `BinauralRenderer.cpp:80-86`) only ever runs on that idle renderer. Proven clean by the Phase 3 `[hrtf-switch]` TSan pass and by this session's full-suite TSan run (F9) plus a new 8-engine run.
- The legacy route (`getBinauralRenderer (idx).hrtfDatabase.loadFromFile (...)` then `setProfile`, then `swapActiveRenderer()`) is safe only if the consumer loads the renderer that is neither active nor the source of a running crossfade. A load into the active renderer races on `easyHandle`, `loaded`, `irLength`, `easy->fir` **and** the convolver vectors that `setProfile` resizes through `PartitionedConvolver::prepare` (`BinauralRenderer.cpp:102-103`), so the damage is wider than the two fields RTSF-03 names. Nothing today detects it.

Recommended D-09 mechanism (counter plus Debug assert, SC-17 pattern), two complementary checks, names discretionary:

1. **Wrong-thread check on the two documented message-thread-only calls.** `swapActiveRenderer()` and `getPrepareRendererIndex()` (`RenderEngine.h:438-443`, both inline) compare the calling thread with the stored render-thread hash (`renderThreadHash_`, exactly how `getActiveLayout()` does it, `RenderEngine.cpp:1701-1708`) and bump a new `std::atomic<int>`. Do **not** assert in `getBinauralRenderer()`: it has no documented thread contract and a consumer may call it from `processBlock` to read `isSimpleMode()`; count there only if wanted.
2. **In-use check on mutation.** The audio thread marks the renderers it is using for this block (the active one, and the crossfade-source one while fading) with a relaxed atomic flag; `HRTFDatabase`'s mutators (`unload`, `loadFromBytes`, `loadFromFile`, `loadFromBinaryData`) and `BinauralRenderer::setProfile` count and assert when the flag is set. A load into the free renderer (the legitimate flow) never trips it. The flag must be cleared on the audio thread **before** the engine publishes the renderer as free to the loader (release store), so the engine's own loader never trips it; a test pins that with the existing `[churn]` scenario.

A simpler alternative that needs no engine bookkeeping: a reader-in-flight counter bumped around each `getInterpolatedHRIR` (two relaxed RMWs per HRIR lookup, which is once per moved source per block) and checked by the mutators. It only catches actual overlap in time, so tests are probabilistic; prefer check 2 for deterministic tests.

### F6. FFT sharing across instances (VERIFY-02)

`SharedFFTCache` hands every `PartitionedConvolver` in the process the **same** `juce::dsp::FFT` object per order, and all instances' audio threads call `performRealOnly*` on it concurrently. What that costs depends on the JUCE engine for the platform (`juce_FFT.cpp`):

- **macOS (`AppleFFT`, `juce_FFT.cpp:444-`)**: `perform*` are const and touch only the read-only `fftSetup`; no lock, no shared scratch [VERIFIED: read]. The claim that vDSP permits concurrent use of one setup on several threads is Apple's, not checked here [ASSUMED].
- **Windows/Linux without FFTW/MKL/IPP (`FFTFallback`, `juce_FFT.cpp:89-135`)**: `perform()` takes a `SpinLock processLock`, and the real-only transforms call `perform()`. With the shared cache that makes every instance's audio thread contend on **one process-wide spinlock** in `renderBlock`. That is a DR-1 lock on those platforms, invisible to the macOS proof. Windows is "implemented but not verified" in this project already; record it as a platform limit of the claim, not a macOS finding, and leave the decision (per-instance FFT objects vs shared) to the Phase 6 CI/Windows work [ASSUMED that the Windows/Linux build selects the fallback engine; not built here].
- `SharedFFTCache` is documented as "create once, never destroy" but `getSharedFFTCache()` is `static SharedFFTCache instance;` inside a function (`PartitionedConvolver.cpp:25-29`), so it **is** destroyed at process exit. D-08's wording should say "for the life of the process"; this is also the known JUCE leak-detector false positive in STATE Deferred Items (Phase 02 02-04). [VERIFIED: read]
- The repo has **no** unit test for `SharedFFTCache` (only two comments mention it: `BinauralRendererTests.cpp:19`, `PartitionedConvolverTests.cpp:17`) although OSD PR #142 lists one [VERIFIED: `grep`].
- `HRTFDatabase::correctLowFrequency` (public static, unused in the repo) builds a *local* `juce::dsp::FFT` per call (`HRTFDatabase.cpp:470`), the per-instance create/destroy pattern that the cache replaced; optional follow-up, not required by the requirement.

### F7. Provenance of OSD#96 and OSD#131 (VERIFY-02)

Traced through `gh`, because the user asks for the git trail before a correction:

- **OSD#96** (closed): AU + VST3 instance on one track, severe buzzing in the HRTF convolution path then a Reaper crash; "Likely cause: possible shared static state or singleton resource (HRTF data, convolver buffers)". The repo comment on `BinauralRenderer::hrtfDatabase` (`BinauralRenderer.h:19-21`) records the fix: a per-renderer `HRTFDatabase`, to remove a shared-state race between the timer thread loading and the audio thread reading.
- **OSD#131** (closed): the issue text is a CoreGraphics `CGGlyphBuilderLockBitmaps` crash with 8 instances; its analysis blames **per-instance typeface duplication** (56 typefaces for 8 instances) and proposes `SharedResourcePointer` for fonts and the look-and-feel. It does not mention FFTs.
- **OSD PR #142** ("Shared FFT cache for multi-instance stability (issue #131)", merged 2026-04-04, "Closes #131") is where `SharedFFTCache` came from. Its stated root cause is the vDSP twiddle-table hypothesis; its verification was manual (12 and 16 instances in REAPER, a unit test of same-order caching). It also pre-allocated `convTmpL/R` and `xfadeWetL_/R_`, i.e. it introduced the very resize guards that are RTSF-01 today.
- This session's attempt to reproduce the vDSP hypothesis directly (8 threads, 4,000 create-use-destroy cycles each of `juce::dsp::FFT` at orders 11-13, comparing against a reference transform, three runs): **0 mismatches, no crash** on this Apple Silicon / macOS 26 machine [VERIFIED: ran]. So D-07's "show the test fails against the old behaviour" cannot depend on that crash reproducing. Deterministic substitutes, with measured evidence:

| Regression | Test | Baseline | Mutation that must fail it | Measured |
|------------|------|----------|----------------------------|----------|
| OSD#96 (shared state between instances) | Engine A (profile 2, fixed signal) rendered alone vs. rendered while engine B on another thread switches profiles 3, 5, 0, 3 and renders; compare A's samples bit for bit | 0 of 51,200 samples differ | `BinauralRenderer::hrtfDatabase` replaced by a reference to one process-wide `HRTFDatabase` | **37,886 of 51,200 differ** |
| SharedFFTCache concurrent first use | 16 threads released together call `getOrCreate (10..13)`; every thread must get the same pointer per order | 0 mismatches, 0 TSan reports | remove the `SpinLock::ScopedLockType` line | **TSan data-race reports on the `std::map` insert and the process then hangs** (run it under `runWithWatchdog`) |
| SharedFFTCache identity and retention (the cache holds the setup after every user is gone) | `getOrCreate` twice returns the same pointer; after dropping all user references the cache's reference remains | not run (no such test today) | make `getOrCreate` return a fresh unshared `make_shared` | by construction fails identity; **run it in the plan** |
| OSD#131 as the issue actually describes it (typeface duplication) | N live `SpatialMapComponent`s share one `MonoFaces` | see Open Question 3 | make `monoFaces_` a plain member | not run |

### F8. RTSF-02: the race, measured, and the fix shape

`TrajectoryEngine::finalAz_`, `finalEl_`, `finalDist_` are plain `float[12]` (`TrajectoryEngine.h:113-115`), written by `tick()` (`TrajectoryEngine.cpp:76-90`) and `reset()` (`:143-145`), read by `getFinalAz/El/Dist` (`TrajectoryEngine.h:95-97`); only `active_` is atomic. A timer-writes / audio-reads test run under TSan on the **current** code reports `data race TrajectoryEngine.h:95/96/97` against `TrajectoryEngine.cpp:76/77/79` [VERIFIED: ran, this is the D-07 "old behaviour" evidence for RTSF-02]. The full existing suite (F9) is clean precisely because no test runs a reader concurrently with `tick`.

Two more facts shape the fix:

- The Random shape (`shape == 10`) writes `finalAz_` **twice** (raw at `:76`, wrapped at `:81`), so a concurrent reader can also observe the un-wrapped value (origin plus up to about 107 degrees, so up to about 287 degrees). Compute into locals and store each value once.
- The public members are also the consumer-facing "direct access" arrays (`// read by processBlock`). Replacing `float[]` with `std::atomic<float>[]` keeps reads (`x = engine.finalAz_[i]`) and writes (`engine.finalAz_[i] = x`) source-compatible through the implicit conversion and assignment; code that takes the address as `float*` would break. In this repo only the getters are used (`tests/`, `examples/demo/DemoComponent.cpp:323-324`); OSD's own `TrajectoryEngine` copy is separate until migration (`gh search code` shows `Source/TrajectoryEngine.h` in OSD), and `docs/integration-guide.md:157` already tells consumers the audio thread never reads `TrajectoryEngine` (it copies positions into per-object atomics), which contradicts the class header comment (`TrajectoryEngine.h:30`, "audio thread reads final positions"). Both usages are safe after the fix.

Recommended shape: three `std::atomic<float>` arrays, relaxed stores and loads, `active_` stored with release and read with acquire, `static_assert (std::atomic<float>::is_always_lock_free)`. A reader can still see az from one tick with el from the next (a mixed triple). The movement between two 60 Hz ticks bounds the error; add a test that measures the largest per-tick angular step over all 13 shapes at the maximum speed and pins it, so "not audible" is a number, not a belief [ASSUMED that this step is small enough; the test will establish it]. If a torn triple is ever judged audible, the escape is a per-object seqlock (reader retries), which is lock-free but not wait-free; not recommended now.

### F9. ThreadSanitizer on HEAD, whole suite

`build-tsan` (RelWithDebInfo, `-fsanitize=thread -fno-omit-frame-pointer` on C, C++ and the executable linker flags) built `SpatialCoreTests` and `SpatialCoreUITests`. Full `SpatialCoreTests` run on HEAD under TSan: **325 test cases, 325,112 assertions, all passed, 0 ThreadSanitizer reports, 29 min 44 s** (the same suite takes 1 min 23 s uninstrumented in `build-rt`). `SpatialCoreUITests` under TSan: 14 test cases, 133 assertions, 0 reports, 3.3 s. A deliberate-race canary reports (`WARNING: ThreadSanitizer: data race`) and the process exits with status 134 (Apple's TSan aborts after reporting), while a clean run exits 0 [VERIFIED]. So the gate is: exit status 0 **and** no `WARNING: ThreadSanitizer` line in the log. Eight `RenderEngine`s created, prepared, rendering, switching profiles and destroyed on eight threads (2 rounds, 400 blocks each, fresh process so the FFT cache is first used concurrently) ran clean under TSan in 38 s.

## Common Pitfalls

### Pitfall 1: `thread_local` inside the allocation hook crashes
**What goes wrong:** the first probe used a `thread_local bool armed`; the test died with `EXC_BAD_ACCESS` in `libdyld.dylib _tlv_get_addr` as soon as the HRTF loader thread started.
**Why it happens:** the first touch of a `thread_local` on a new thread allocates its storage through malloc, which re-enters the hook before the thread's TLS exists.
**How to avoid:** compare `pthread_self()` against an `std::atomic<pthread_t>` armed thread id, as in Pattern 1. Never allocate, log or touch TLS inside the hook.
**Warning signs:** a segfault that only appears in tests that start a worker thread.

### Pitfall 2: The trap reports a green zero when it cannot see anything
**What goes wrong:** under `-fsanitize=thread` the same trap counted 0 allocations for a deliberate `malloc` and `new std::vector` (canary printed `allocs=0 frees=0`) and "passed" every scenario [VERIFIED].
**How to avoid:** every `[rt-alloc]` test begins with a canary `REQUIRE (allocsSeen >= 1)` after a deliberate `malloc (64)` (and a `new`), exactly as the lock trap's canary needs `locks >= 1` for a `CriticalSection`. Tag the trap tests so the TSan tree excludes them (`~[rt-alloc]` or by running only `[tsan]`).

### Pitfall 3: A warm-up hides the allocation
**What goes wrong:** with 40 warm-up blocks before arming, all 23 formats measured 0; the Ambisonics allocation appears only on blocks where the smoothed distance is still moving, and lazy statics only on the first block.
**How to avoid:** arm from the first `renderBlock()` after `prepare()`, vary az, el **and distance** every block, and keep each cold-start combination in its own TEST_CASE (ctest runs each in its own process, so process-global lazy state is cold; running the executable directly shares one process, which is fine for the warm cases but not the cold ones). Say so in the test comment.

### Pitfall 4: Instrumented SOFA loads are slow, and the engine gives up after 15 s
**What goes wrong:** eight engines each loading SADIE or Bernschuetz at once under TSan took long enough that `~RenderEngine`'s `stopThread (15000)` expired and JUCE printed `!! killing thread by force !!` and aborted the process (the first multi-engine probe died this way). Phase 3 saw the same at `-O0` (03-07).
**How to avoid:** the multi-engine test uses the small profiles (2 CIPIC, 3 HUTUBS, 5 KEMAR; 1.2 to 1.8 MB) and calls `waitForHRTFProfileIdle` before an engine is destroyed; keep `RelWithDebInfo` for the sanitizer tree; set `SPATIALCORE_TEST_TIMEOUT_SCALE=10`. Do not change the engine's 15 s bound.

### Pitfall 5: The whole suite under TSan takes half an hour
**What goes wrong:** 29 min 44 s on HEAD versus 1 min 23 s uninstrumented. A gate that long will not be run.
**How to avoid:** the third-tree gate runs a tagged subset, `[tsan]`, chosen for thread interaction: the existing `[hrtf-switch]` cases, `[sc16]`/`[sc17]` triple-buffer cases, and every new `[multi]`, `[fftcache]`, `[trajectory][threads]` case. Run the full suite under TSan once at the phase end gate (`/gsd-verify-work`) and before a release tag, not at every commit. Record the tag list and its measured runtime in the README so it is reproducible.

### Pitfall 6: A mutation run can hang the gate
**What goes wrong:** with the cache lock removed, the concurrent first-use test left a process spinning for 9+ CPU-minutes (the unlocked `std::map` was corrupted) after TSan had already printed its races.
**How to avoid:** wrap that test in `runWithWatchdog` so a broken library fails fast; do mutation runs in a scratch step and restore the file with `git checkout` straight after (as the Phase 3 checks did).

### Pitfall 7: `tests/CMakeLists.txt` is CRLF
**What goes wrong:** an editor or script that rewrites line endings turns a 3-line change into a 180-line diff (it happened to the research probe's first edit). `git ls-files --eol` reports `i/crlf w/lf` for that file on this checkout.
**How to avoid:** edit with a tool that preserves CRLF (the Edit tool on exact lines does), or re-add `\r\n` explicitly; check `git diff --stat tests/CMakeLists.txt` is a few lines before committing.

### Pitfall 8: Sanitizer trees are not git-ignored
**What goes wrong:** `.gitignore` lists `build/` and `build-release/` only; `build-tsan/` shows up as untracked.
**How to avoid:** add `build-tsan/` (and any other new tree name) to `.gitignore` in the plan that documents the gate.

### Pitfall 9: A Debug `jassertfalse` in a guard does not stop a test
**What goes wrong:** nothing, but it surprises: JUCE logs and breaks only under a debugger, so a test that deliberately trips a guard in Debug just logs an assertion line (precedent: `RenderEngineTests.cpp:1630`). Keep the counter the thing the test asserts.

### Pitfall 10: Do not interpose `os_unfair_lock_lock`
**What goes wrong:** the test process aborted (SIGABRT) once an `os_unfair_lock_lock` interposer was linked [VERIFIED]. Only the pthread entry points are interposed in the working design.

## Code Examples

### Lock-trap shared library (complete, tested prototype)

```cpp
// tests/Support/rtguard/RTGuard.cpp  (SHARED library SpatialCoreRTGuard, APPLE only)
// Verified this session: a CriticalSection and a std::mutex taken on the armed thread gave locks=2; locks=0 over all 23 formats.
#include <pthread.h>
#include <atomic>
#include <cstdint>
extern "C" {
typedef void (malloc_logger_t)(uint32_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uint32_t);
extern malloc_logger_t* malloc_logger;
}
namespace {
std::atomic<pthread_t> gArmed { nullptr };
std::atomic<int> gAllocs { 0 }, gFrees { 0 }, gLocks { 0 };
void mallocHook (uint32_t type, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uint32_t)
{
    if (gArmed.load (std::memory_order_relaxed) != pthread_self()) return;
    if (type & 2) gAllocs.fetch_add (1, std::memory_order_relaxed);
    if (type & 4) gFrees.fetch_add (1, std::memory_order_relaxed);
}
inline void noteLock() { if (gArmed.load (std::memory_order_relaxed) == pthread_self()) gLocks.fetch_add (1, std::memory_order_relaxed); }
}
extern "C" {
void scrt_arm()    { gAllocs = 0; gFrees = 0; gLocks = 0; malloc_logger = mallocHook; gArmed.store (pthread_self()); }
void scrt_disarm() { gArmed.store (nullptr); }
int  scrt_allocs() { return gAllocs.load(); }
int  scrt_frees()  { return gFrees.load(); }
int  scrt_locks()  { return gLocks.load(); }
int rt_mutex_lock (pthread_mutex_t* m)    { noteLock(); return pthread_mutex_lock (m); }
int rt_mutex_trylock (pthread_mutex_t* m) { noteLock(); return pthread_mutex_trylock (m); }
int rt_rdlock (pthread_rwlock_t* l)       { noteLock(); return pthread_rwlock_rdlock (l); }
int rt_wrlock (pthread_rwlock_t* l)       { noteLock(); return pthread_rwlock_wrlock (l); }
int rt_cond_wait (pthread_cond_t* c, pthread_mutex_t* m) { noteLock(); return pthread_cond_wait (c, m); }
}
struct Interpose { const void* repl; const void* orig; };
#define SC_INTERPOSE(r, o) { (const void*) r, (const void*) o }
__attribute__((used)) static const Interpose kTuples[] __attribute__((section ("__DATA,__interpose"))) = {
    SC_INTERPOSE (rt_mutex_lock, pthread_mutex_lock),   SC_INTERPOSE (rt_mutex_trylock, pthread_mutex_trylock),
    SC_INTERPOSE (rt_rdlock, pthread_rwlock_rdlock),    SC_INTERPOSE (rt_wrlock, pthread_rwlock_wrlock),
    SC_INTERPOSE (rt_cond_wait, pthread_cond_wait),
};
```
```cmake
# tests/CMakeLists.txt (CRLF!) — APPLE only; the test target links it so dyld applies the tuples to every image
if(APPLE)
    add_library(SpatialCoreRTGuard SHARED Support/rtguard/RTGuard.cpp)
    target_link_libraries(SpatialCoreTests PRIVATE SpatialCoreRTGuard)
endif()
```
Building it by hand without `-mmacosx-version-min` only produced a "built for newer version 26.0" linker warning; a CMake target inherits `CMAKE_OSX_DEPLOYMENT_TARGET 12.0` from the top level.

### Armed render of one block, with the canary first

```cpp
TEST_CASE ("Realtime: the traps see a deliberate allocation and a deliberate lock", "[rt][rt-alloc]")
{
    scrt_arm();
    void* p = std::malloc (64); asm volatile ("" :: "r" (p) : "memory"); std::free (p);   // keep the optimiser from eliding it
    juce::CriticalSection cs; { const juce::ScopedLock l (cs); }
    scrt_disarm();
    REQUIRE (scrt_allocs() >= 1);     // a trap that cannot see is a false green (Pitfall 2)
    REQUIRE (scrt_locks()  >= 1);
}
// then, per combination:  engine.prepare (...);  /* set format, profile, flags */  scrt_arm();  engine.renderBlock (...);  scrt_disarm();
//                         CHECK (scrt_allocs() == 0);  CHECK (scrt_frees() == 0);  CHECK (scrt_locks() == 0);
```

### TrajectoryEngine (RTSF-02)

```cpp
// include/SpatialCore/Trajectory/TrajectoryEngine.h
static_assert (std::atomic<float>::is_always_lock_free, "the audio read must be wait-free");
float getFinalAz   (int i) const { return finalAz_[i].load   (std::memory_order_relaxed); }   // same for El, Dist
bool  isActive     (int i) const { return active_[i].load    (std::memory_order_acquire); }
std::atomic<float> finalAz_[kMaxObjects]   = {};   // same for finalEl_, finalDist_
// src/Trajectory/TrajectoryEngine.cpp  tick(), Random branch: compute locals (el/dist clamped, az wrapped once), then
finalAz_[t].store (azWrapped, std::memory_order_relaxed);  finalEl_[t].store (elClamped, ...);  finalDist_[t].store (distClamped, ...);
active_[t].store (true, std::memory_order_release);        // after the three values
```
Tests in `TrajectoryTests.cpp` read through the getters and through `engine.phase_[0]`/`engine.rng_` (public members); none takes the address of a `final*_` element, so they keep compiling. Existing expectations (`WithinAbs (36.0f, 0.5f)` etc.) are unaffected because the stored values are identical.

### ThreadSanitizer tree (D-01 third tree)

```bash
# Configure once. If another tree already fetched the dependencies, add
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=<tree>/_deps/juce-src -DFETCHCONTENT_SOURCE_DIR_MYSOFA=<tree>/_deps/mysofa-src -DFETCHCONTENT_SOURCE_DIR_CATCH2=<tree>/_deps/catch2-src
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build-tsan -j8                    # SpatialCoreTests alone linked in about 51 s once configured (10 jobs, M4 Pro)
SPATIALCORE_TEST_TIMEOUT_SCALE=10 TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1" \
  ./build-tsan/tests/SpatialCoreTests "[tsan]" 2>&1 | tee /tmp/sc-p5-tsan.log; echo "exit=$?"   # use ${PIPESTATUS[0]} in bash
grep -c "WARNING: ThreadSanitizer" /tmp/sc-p5-tsan.log     # must print 0, and the exit status must be 0
```
The flags are the same ones Phase 3 used (03-07 SUMMARY) and match this session's measured build. `SpatialCoreUITests` under TSan is quick (3.3 s) and can run whole.

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Hand-audit plus "run it in a DAW" for realtime safety | Sanitizer-class tooling: ThreadSanitizer for races, RealtimeSanitizer (`-fsanitize=realtime`) for allocation, locks and blocking calls | RTSan landed in LLVM 20 [ASSUMED: training knowledge, not checked here] | Not usable on this machine (Apple clang 21 rejects the flag); the in-process traps reproduce its allocation and lock checks on macOS. Revisit for Phase 6 CI on Linux. |
| `jassertfalse` plus an audio-thread `resize` "just in case" (OSD PR #142 style) | Count, skip, assert in Debug, never allocate (D-03, D-04) | This phase | Release builds stop allocating on a contract violation |

**Deprecated/outdated:**
- The ROADMAP line numbers for the RTSF-01 sites (`:196-209`, `:134,158-163`): moved by Phase 3; use HEAD's `:178-183` and `:223-228`.
- The comment "No allocation: coefficients are assigned first-order..." at `RenderEngine.cpp:1499-1500` is true of the filter state, not of the temporary `Coefficients`; update it with the fix.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | RealtimeSanitizer shipped in LLVM/clang 20 and is the right Linux CI tool for Phase 6 | Alternatives, State of the Art | Phase 6 picks a different tool; no effect on Phase 5 |
| A2 | vDSP allows concurrent use of one FFT setup on several threads | F6 | If false, the shared cache would race on macOS; mitigated by the 8-engine TSan run (clean) and the churn probe (0 mismatches) |
| A3 | Windows/Linux builds select JUCE's `FFTFallback` (with its SpinLock) because no FFTW/MKL/IPP is configured | F6 | The cross-instance lock would not exist there; not buildable here, so recorded as a platform limit |
| A4 | A mixed (az from one tick, el from the next) triple read is inaudible at 60 Hz | F8 | Audible zipper on very fast shapes; the proposed step-size test establishes the bound and a seqlock is the fallback |
| A5 | `malloc_logger` remains exported by libmalloc on macOS 12 (the project's deployment target) through current releases | Standard Stack | The trap fails to link or sees nothing; the canary REQUIRE makes this loud rather than silent, and the fallback is `operator new` replacement plus a documented gap |
| A6 | Consumers do not call `getBinauralRenderer()` from the audio thread, so a counter there is safe; an assert is not recommended | F5 | An assert would fire in a consumer's Debug build; that is why only the two documented message-thread-only calls get the assert |
| A7 | Painting and constructing `SpatialMapComponent`s is a message-thread activity (so the UI test creates and paints many on one thread rather than painting from several) | Open Question 3 | A multi-thread paint test would be possible and would be closer to #131's background `CA::CG::Queue`; no evidence either way here |
| A8 | The oversized-block behaviour users want is "render correctly in chunks" (Open Question 1, option C) | Open Questions | The user may prefer skip-and-silence; this is why it is a question, not a decision |

## Open Questions

1. **What should the engine do when a host hands it a block longer than the size it was prepared for?**
   - What we know: five sites resize or reassign buffers on the audio thread in that case (F2); D-03 settles only the two `BinauralRenderer` ones ("skip, keep last good sound"); the other three are in `RenderEngine.cpp` and the existing CR-03 test expects an oversized block to be handled without corruption; `PartitionedConvolver` already splits oversized blocks into prepared-size chunks (WR-02).
   - What's unclear: whether the user wants D-03's "skip" extended to the engine (the tail of the oversized block would be silent), or "chunk" (render correctly, no allocation, behaviour change only for the oversized case), or leave the engine sites as they are and only fix the two named sites (then RTSF-05's "zero allocations on the whole path" cannot be claimed for oversized blocks).
   - Recommendation: chunk, in `renderBlock`, by processing prepared-size slices with offset pointers (it matches the convolver precedent and keeps the audio correct); it is a plain-English question for the user: "if a DAW ever sends a longer block than it promised, should the plug-in play it correctly in pieces or drop the extra part?". Count the event either way (D-04). Update `RenderEngineTests.cpp:121` to assert the counter and a finite, non-silent result.

2. **Is the Ambisonics fix in scope?**
   - What we know: it is a DR-1 breach found by the D-02 matrix (F1), the fix is bit-identical, and RTSF-05 cannot report zero without it.
   - Recommendation: yes, treat as part of RTSF-05 (no user decision needed, D-10 is satisfied by a before/after comparison). Pin it with: an exact coefficient-bit comparison against the old construction over a sweep of distances and orders, plus an output fingerprint (peak, energy per channel) for orders 1 to 6 with moving distance captured before the fix in both Debug and Release.

3. **How far should the OSD#131 regression reach into the UI?**
   - What we know: `SpatialMapComponent` already shares three typefaces through `juce::SharedResourcePointer<MonoFaces>`; `SMLLookAndFeel` loads seven typefaces per instance (`SMLLookAndFeel.cpp:21-27`), which is exactly the per-instance duplication OSD#131 describes if a consumer makes one look-and-feel per editor.
   - What's unclear: whether D-06 stops at "N maps alive and painted together, sharing one `MonoFaces`" or also asks for `SMLLookAndFeel` to share its faces.
   - Recommendation: the test side only: a construction counter on `MonoFaces` (create 16 maps, expect 1; destroy all and create again, expect 2 in total), a paint of all maps compared pixel for pixel with the first, and a documented note that `SMLLookAndFeel` is per-instance. Sharing the look-and-feel's faces is an additive UI change with no visual effect (the `[ui][fonts][bytes]` tests pin the bytes), but it is a scope decision for the user.

4. **Where does the lock claim stop?**
   - What we know: the proof is macOS arm64 only, and on Windows/Linux JUCE's fallback FFT takes a spinlock inside the shared FFT (F6).
   - Recommendation: state the platform scope explicitly in README and the SUMMARY ("proved on macOS arm64; Windows/Linux in Phase 6 CI-01"), and file the shared-FFT-on-fallback-engine concern as a Phase 6 / Windows follow-up rather than changing the cache now (D-08 keeps it as is).

5. **How should the RTSF-01 `updateSourceHRIR` guard be tested when it is unreachable?**
   - What we know: `ensureScratchCapacity()` makes the branch unreachable through the public API.
   - Recommendation: a TEST-ONLY method on `BinauralRenderer` that shrinks the scratch (precedent: `setLoaderFailureForTesting`, accepted in STATE), declared unconditionally to avoid an ODR layout difference between SpatialCore's build and a consumer's.

## Runtime State Inventory

Not a rename, refactor or migration phase; omitted per the research template.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Apple clang | everything | yes | 21.0.0 (clang-2100.3.34.2) | -- |
| ThreadSanitizer runtime | RTSF-02/03/05, VERIFY-02 | yes | `libclang_rt.tsan_osx_dynamic.dylib` | -- |
| RealtimeSanitizer (`-fsanitize=realtime`) | allocation/lock detection | **no** (flag rejected; no Homebrew LLVM) | -- | `malloc_logger` trap + interposed lock trap (this research) |
| libmalloc `malloc_logger` | allocation trap | yes | Darwin 25.6.0 | `operator new` replacement (partial) |
| dyld interposition | lock trap | yes | -- | source-scan rule only |
| CMake | all trees | yes | 4.4.4 (>= 3.22) | -- |
| Ninja | build | no | -- | default Makefiles generator (used) |
| Git LFS HRTF files | embedded profiles and tests | yes (real HDF5 files in `HRTF/`) | git-lfs 3.8.0 | -- |
| `gh` CLI | issue provenance | yes | authenticated | -- |
| Network (first configure fetches JUCE, libmysofa, Catch2) | first configure of any new tree | yes | -- | reuse another tree's `_deps` with `FETCHCONTENT_SOURCE_DIR_*` |

**Missing dependencies with no fallback:** none.
**Missing dependencies with fallback:** RealtimeSanitizer (use the in-process traps; revisit for Linux CI).

## Validation Architecture

> `.planning/config.json` has no `workflow.nyquist_validation` key (only `_auto_chain_active`), so the section is included.

### Test Framework

| Property | Value |
|----------|-------|
| Framework | Catch2 v3.7.1 (FetchContent), two executables `SpatialCoreTests` and `SpatialCoreUITests` |
| Config file | `tests/CMakeLists.txt` (explicit source lists; CRLF); `catch_discover_tests` gives one ctest per TEST_CASE, each in its own process |
| Quick run command | `./build-release/tests/SpatialCoreTests "[rt]"` (new traps, a few seconds) and `"[fftcache],[multi]"` |
| Full suite command | the README gate: Debug build + both executables, then Release build + `ctest --test-dir build-release/tests --output-on-failure` (this session: 325 test cases in 1 min 23 s in a RelWithDebInfo tree) |
| Sanitizer gate (new, D-01) | `build-tsan` (above) running `"[tsan]"`; whole suite once per phase end gate and before a release tag (29 min 44 s) |

### Phase Requirements -> Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| RTSF-01 | `renderSourceBuffers` with a block above the scratch size: counter increments, no allocation, output defined, previous HRIR kept (`updateSourceHRIR` through the test seam) | unit + trap | `./build-release/tests/SpatialCoreTests "[rt][scratch]"` (Release is what matters: `jassert` is a no-op there) | no, Wave 0 (`BinauralRendererTests.cpp` gets it; trap tests go in `Engine/RealtimeSafetyTests.cpp`) |
| RTSF-01 (engine sites, if OQ1 accepted) | oversized block through `renderBlock` allocates nothing, counter increments, output correct | trap | `... "[rt-alloc][oversized]"` | partial (`RenderEngineTests.cpp:121` exists and changes) |
| RTSF-02 | concurrent `tick` and `getFinal*`: no TSan report; values always within range and never the unwrapped Random value; per-tick step bound pinned | thread + TSan | `./build-tsan/tests/SpatialCoreTests "[trajectory][threads]"` | no, Wave 0 |
| RTSF-03 | misuse counters: `swapActiveRenderer`/`getPrepareRendererIndex` on the render thread counted; a load into the active renderer counted; the engine's own loader never counts (existing `[churn]`) | unit | `... "[hrtf-switch][misuse]"` | no, Wave 0 |
| RTSF-03 | `setProfile`/`setHRTFProfile` while another thread renders: no TSan report | thread + TSan | `./build-tsan/... "[hrtf-switch]"` | yes (Phase 3, `[threads]`, `[churn]`) |
| RTSF-05 | alloc/lock matrix: 23 formats x flag combinations (all three, each pair, none) x {Simple, each of the 5 profiles} x block sizes {32, 64, 256, 512, 1024} x rates {44.1, 48, 96 kHz} (sample a covering set, not the full product) x 12 algorithm indices on a surround format; cold start; mid-play `setHRTFProfile` including 0 <-> SOFA; `setOutputFormat` with and without `engineFadesFormatSwitch`; trajectories feeding positions | trap | `./build-release/tests/SpatialCoreTests "[rt-alloc]"` and `"[rt-lock]"` | no, Wave 0 |
| RTSF-05 | lock primitives in reachable sources limited to the allow-list | source scan | `... "[rt][rule]"` | no, Wave 0 (needs `SPATIALCORE_SOURCE_DIR` on `SpatialCoreTests`) |
| RTSF-05 | whole thread-interaction subset race-free | TSan | the Sanitizer gate | partly (Phase 3 tests) |
| VERIFY-02 | `SharedFFTCache` identity, retention, cross-order isolation, 16-thread first use (watchdog) | unit + TSan | `... "[fftcache]"` | no, Wave 0 |
| VERIFY-02 | 8 engines created, prepared, rendering, switching small profiles and destroyed on their own threads; non-finite count 0 | stress + TSan | `... "[multi]"` | no, Wave 0 |
| VERIFY-02 (OSD#96) | engine A bit-identical alone vs. beside a switching engine B | unit | `... "[multi][isolation]"` | no, Wave 0 |
| VERIFY-02 (OSD#131) | N live `SpatialMapComponent`s share typefaces, paint identically, no crash | UI | `./build-release/tests/SpatialCoreUITests "[ui][multi]"` | no, Wave 0 |

### Sampling Rate

- **Per task commit:** `./build-release/tests/SpatialCoreTests "[rt],[fftcache],[multi],[trajectory][threads]"` plus the touched area's existing tags.
- **Per wave merge:** the full README gate (Debug + Release, both executables, `ctest` listing check for `ui:`).
- **Phase gate:** full gate green, then `build-tsan` with `[tsan]` clean (exit 0, zero `WARNING: ThreadSanitizer`), then the whole suite under TSan once, then the mutation evidence recorded in SUMMARY (D-07): trajectory race (already reproduced: TSan reports on the old code), isolation (shared database: 37,886 of 51,200 samples differ), cache lock removed (TSan races plus hang under a watchdog), `resize` re-introduced into a guard (trap counts it), NFC temporary re-introduced (trap counts 1,416+ allocations).

### Wave 0 Gaps

- [ ] `tests/Support/RealtimeGuard.h` and `tests/Support/rtguard/RTGuard.cpp` plus the `SpatialCoreRTGuard` CMake target (APPLE) and `SPATIALCORE_SOURCE_DIR` on `SpatialCoreTests`
- [ ] `tests/Engine/RealtimeSafetyTests.cpp`, `tests/Engine/MultiInstanceTests.cpp`, `tests/Binaural/SharedFFTCacheTests.cpp`, `tests/UI/MultiMapTests.cpp`; additions to `TrajectoryTests.cpp` and `BinauralRendererTests.cpp`
- [ ] `[tsan]` tag assignment on the existing `[hrtf-switch]`, `[sc16]`, `[sc17]` cases (or a documented tag union) so the third-tree command is one line
- [ ] `.gitignore` entry `build-tsan/`; README "Local test gate" extension (D-01) with the exact commands above, the platform scope, and the expected runtimes
- [ ] Framework install: none

### Documentation surfaces to update with the code (so docs do not drift)

`README.md` "Local test gate" (D-01); `CLAUDE.md` Design Principles ("the remaining defensive `jassertfalse` plus `resize` guards ... are RTSF-01, Phase 5") and the HRTF-switching paragraph (legacy-route thread contract, D-09); `docs/integration-guide.md:392-396` (the "tracked as RTSF-01" sentence) and the legacy-route paragraph near `:354`; `include/SpatialCore/Binaural/BinauralRenderer.h` and `HRTFDatabase.h` doc comments (the `const` lookup mutates `easy->fir`); `include/SpatialCore/Trajectory/TrajectoryEngine.h` (contract comment, `:30`); `include/SpatialCore/Binaural/SharedFFTCache.h` (D-08 wording: "for the life of the process"); the stale comment in `RenderEngine.cpp` above the NFC update; the `tests/Binaural/BinauralRendererTests.cpp:336-343` comment that says the guards "stay as they are" (it becomes false).

## Security Domain

> `security_enforcement` is not set in `.planning/config.json`, so the section is included (absent = enabled). This is a local C++ audio library: no network surface is added by this phase.

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|---------------|---------|-----------------|
| V2 Authentication | no | -- |
| V3 Session Management | no | -- |
| V4 Access Control | no | -- |
| V5 Input Validation | yes (narrowly) | Block length and sample indices from a host are validated against the prepared size before indexing (the RTSF-01 and oversized-block work); existing SOFA pre-validation (`HRTFDatabase::declaredShapeWithinBounds`) is unchanged |
| V6 Cryptography | no | -- |
| V14 Configuration / memory safety | yes | Sanitizer tree (TSan) in the gate; counters instead of silent fallbacks |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Heap overflow from a host block larger than prepared (CR-03 class) | Tampering / DoS | Bound every audio-side buffer use by its real size; chunk or skip, never grow on the audio thread |
| Use-after-free when a profile is unloaded while the audio thread reads it (legacy route misuse) | DoS / Tampering | Exclusive-ownership handoff (engine route); in-use flag + counter detection for the legacy route (D-09) |
| Data race on a cross-thread float (TrajectoryEngine) | Tampering | `std::atomic<float>`, TSan-proven |
| Interposer/trap shipped by accident | Tampering | Trap code lives in `tests/` only and is linked into the test executable only; never into `SpatialCore` or `SpatialCoreUI` |

## Sources

### Primary (HIGH confidence — run or read this session)
- Local runs: `clang --version`, `clang++ -fsanitize=realtime` (rejected), `ls` of the CLT resource directory, `cmake --version`; TSan tree configure and build; full `SpatialCoreTests` run under TSan (0 reports, 29 m 44 s); `SpatialCoreUITests` under TSan; malloc_logger, dyld-interposition, vDSP-churn, instance-isolation, FFT-cache-first-use, trajectory-race, 8-engine and cold-start/algorithm-index probes (all in a removed temporary file)
- Repo files read in full or in the cited ranges at HEAD `c4ed7b2`: `src/Binaural/BinauralRenderer.cpp`, `include/SpatialCore/Binaural/{BinauralRenderer,HRTFDatabase,SharedFFTCache,PartitionedConvolver}.h`, `src/Binaural/{HRTFDatabase,PartitionedConvolver}.cpp`, `src/Engine/RenderEngine.cpp` (render, layout, NFC, guards), `include/SpatialCore/Engine/RenderEngine.h`, `include/SpatialCore/Trajectory/TrajectoryEngine.h`, `src/Trajectory/TrajectoryEngine.cpp`, `tests/CMakeLists.txt`, `tests/Engine/ProfileSwitchTests.cpp`, `tests/Engine/RenderEngineTests.cpp`, `tests/Support/RouteRenderRig.h`, `tests/Binaural/BinauralMetrics.h`
- JUCE 9.0.0 source (fetched by the build): `modules/juce_dsp/frequency/juce_FFT.cpp`, `modules/juce_dsp/processors/juce_IIRFilter.h` and `juce_IIRFilter_Impl.h`
- libmysofa v1.3.5 source (fetched by the build): `src/hrtf/{easy,interpolate,lookup,neighbors,kdtree}.c`
- Planning artifacts: `05-CONTEXT.md`, `05-DISCUSSION-LOG.md` (not needed beyond CONTEXT), `REQUIREMENTS.md`, `STATE.md`, `03-07-SUMMARY.md`, `CLAUDE.md`, `README.md`, `docs/integration-guide.md`

### Secondary (MEDIUM confidence)
- GitHub via `gh`: Spatial-Media-Lab/OpenSpatialDelay issues #96, #131 and PR #142 (bodies); AndrewRahman/SpatialCore#18 (body); `gh search code` for `finalAz_` in OSD

### Tertiary (LOW confidence — marked for validation)
- RTSan availability in LLVM 20 and its suitability for Linux CI (A1); vDSP concurrent-setup guarantee (A2); Windows/Linux FFT engine selection (A3); `malloc_logger` stability across macOS versions (A5)

## Metadata

**Confidence breakdown:**
- Standard stack (tooling): HIGH — every tool was built or run here; the one gap is that `malloc_logger` is an SPI (A5), guarded by the canary
- Architecture (test and fix shapes): HIGH for RTSF-01/02 and the Ambisonics fix (each exercised or read to the line), MEDIUM for the D-09 legacy-route detection design (designed, not built) and the multi-map UI test (A7)
- Pitfalls: HIGH — each was hit during this research
- VERIFY-02 provenance: HIGH for the trace, MEDIUM for "the vDSP crash does not reproduce" (three runs on one machine)

**Research date:** 2026-10-06
**Valid until:** 2026-11-05 (tooling and code are stable; revisit if the toolchain, JUCE pin or macOS major version changes, because `malloc_logger`, the Apple TSan exit behaviour and `__interpose` are OS-level)

