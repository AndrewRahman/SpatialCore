# Codebase Concerns

**Analysis Date:** 2026-08-09

## Tech Debt

### vDSP FFT Twiddle Table Use-After-Free (Issue #131)

**Files:** `include/SpatialCore/Binaural/SharedFFTCache.h`, `src/Binaural/PartitionedConvolver.cpp` (lines 7-28)

**Issue:** Apple's vDSP library shares internal twiddle factor memory across FFT setups of the same order. Destroying the last FFT setup of a given order frees the shared twiddle table even while another thread's `vDSP_fft_zrip` is reading from it. This causes crashes in multi-plugin scenarios where plugins create/destroy FFT setups concurrently on different threads.

**Impact:** Unpredictable crashes when multiple plugin instances load/unload simultaneously on macOS. The workaround (persistent FFT cache) prevents the problem but leaks memory for the lifetime of the process. Real fix requires:
1. JUCE upstream patch to FFT wrapper
2. Or switch to a different FFT library (FFTW, Kiss FFT)

**Current Mitigation:** `SharedFFTCache` maintains a static map of FFT pointers, never destroying them during plugin lifetime.

**Fix approach:** 
- File issue with JUCE to fix their FFT destructor
- OR implement a reference-counting FFT manager with proper lifecycle handling
- OR switch to a cross-platform FFT library that doesn't have this issue

---

### Overlap-Save Boundary Discontinuities (Issue #50)

**Files:** `include/SpatialCore/Binaural/PartitionedConvolver.h` (lines 24-27, 49-71), `src/Binaural/PartitionedConvolver.cpp` (lines 50-125, 195)

**Issue:** During HRTF transitions, the overlap-save algorithm's tail buffer carries state from the old impulse response. When a new IR is loaded mid-convolution, the transition causes an audible pop or click at the block boundary due to spectral mismatch between the old and new IR tails.

**Current state:** v1.0.5 dual-convolver architecture loads the new IR into an inactive slot, runs both in parallel during a 4-block warmup + 4-block crossfade (~42ms at 48kHz). Gain interpolation per-sample prevents transients.

**Remaining risks:**
- Crossfade duration is hardcoded (`kCrossfadeBlocks = 4`) — not configurable
- If `setIR()` is called during a transition, the new IR is deferred and queued in `pendingIR` — potential race if multiple `setIR()` calls stack
- Warmup block count (`kWarmupBlocks = 1`) is tight — if audio thread outpaces warmup, inactive slot hasn't built overlap buffer

**Fix approach:**
- Make crossfade duration configurable based on sample rate
- Add bounds checking to `pendingIR` queue to prevent overflow if too many `setIR()` calls queue
- Increase or validate warmup block count under high-latency conditions

---

### HRTF Database Race Condition (Issue #96)

**Files:** `include/SpatialCore/Binaural/BinauralRenderer.h` (lines 21-23), `src/Binaural/HRTFDatabase.cpp` (entire)

**Issue:** In v1.0.10 and earlier, a single global `HRTFDatabase` was shared across all `BinauralRenderer` instances. When a timer thread loaded a new SOFA profile via `setProfile()`, the audio thread was simultaneously doing KD-tree HRIR lookups on the database. No synchronization — pure data race.

**Current state:** v1.0.11+ each `BinauralRenderer` has its own `HRTFDatabase` instance, eliminating the shared state. However, `HRTFDatabase::loadFromMemory()` and `getInterpolatedHRIR()` are still not internally thread-safe. If a plugin calls `setProfile()` from a UI timer while audio is running, concurrent reads/writes to `easyHandle` (libmysofa struct) will race.

**Remaining risks:**
- No spinlock or mutex protects `HRTFDatabase::easyHandle` or `loaded` flag
- `libmysofa` is not thread-safe — concurrent access to `MYSOFA_EASY` struct causes undefined behavior
- Per-renderer instance solves the multi-plugin case but NOT the single-plugin case where UI and audio threads race

**Fix approach:**
- Add `juce::SpinLock` to `HRTFDatabase` around all reads/writes to `easyHandle`
- Document that `setProfile()` must not be called while audio is processing
- OR implement double-buffering: keep two HRTF databases and swap atomically

---

### Minimum-Phase HRIR Conversion Artifacts (Issue #47)

**Files:** `include/SpatialCore/Binaural/HRTFDatabase.h` (lines 47-51), `src/Binaural/HRTFDatabase.cpp` (lines 148-210)

**Issue:** When blending between adjacent HRIR positions via time-domain EMA interpolation (crossfade), spectral mismatches from phase differences cause comb filtering artifacts. Converting HRIRs to minimum-phase removes excess phase, allowing smooth interpolation.

**Current state:** v1.0.4 implements `convertToMinPhase()` via cepstral decomposition (log-magnitude FFT, windowing, inverse FFT, exponentiate). Works but is CPU-intensive.

**Remaining risks:**
- `convertToMinPhase()` is never called in current production code — it's defined but unused
- The log-magnitude computation clamps to 1e-20f for numerical stability, but under aggressive optimization flags (`-ffast-math`), exponentiate can still overflow/underflow
- Cepstral decomposition assumes linear-phase minimum-phase conversion — doesn't verify the HRTF is actually minimum-phase

**Fix approach:**
- Integrate `convertToMinPhase()` into HRTF loading pipeline (post-SOFA load)
- Add overflow/underflow guards to exponentiate step: clamp result to reasonable range
- OR pre-compute minimum-phase HRIRs offline and bake into SOFA files

---

### SOFA Zero-Delay ITD Detection (Issue #89)

**Files:** `src/Binaural/HRTFDatabase.cpp` (lines 97-114)

**Issue:** Some SOFA datasets (MIT KEMAR, CIPIC, HUTUBS, Bernschuetz) report zero delay for both channels but have ITD (inter-aural time difference) baked into the waveform. When `getAlignedHRIR()` tries to remove ITD by shifting, it gets no shift values and returns unaligned HRIRs, causing comb filtering during crossfade.

**Current state:** v1.0.11 detects onset using threshold (10% of peak amplitude) and computes ITD from waveform. Works for most datasets. SADIE II KU100 reports explicit delays and bypasses this fallback.

**Remaining risks:**
- `detectOnset()` threshold is hardcoded to 0.1f (10% of peak) — may fail on HRIRs with gradual rise
- If `onsetL` or `onsetR` > `irLength/2`, the function returns 0 without warning — silently disables ITD correction
- No logging for when fallback is used — audio engineer has no feedback if ITD detection fired

**Fix approach:**
- Make onset threshold configurable (0.05-0.2 range for different HRTF types)
- Log when fallback detects onset vs explicit delay
- Add test coverage for datasets with zero-reported delays (KEMAR, CIPIC)

---

## Data Races & Thread Safety Issues

### TrajectoryEngine Position Race Condition

**Files:** `include/SpatialCore/Trajectory/TrajectoryEngine.h` (lines 96-98, 99-108), `src/Trajectory/TrajectoryEngine.cpp` (lines 94-100, etc.)

**Issue:** `TrajectoryEngine` is designed to be called from two threads:
- Timer thread (UI): calls `tick()` to advance animation at ~60Hz, writes to `finalAz_`, `finalEl_`, `finalDist_`, and other state
- Audio thread: calls `getFinalAz(i)`, `getFinalEl(i)`, `getFinalDist(i)` from `processBlock` to read current position

**Problem:** `finalAz_`, `finalEl_`, `finalDist_` are plain floats with NO synchronization. The timer thread writes them while the audio thread reads them — a classic data race. Only `active_[i]` is atomic.

**Impact:**
- Audio thread may read torn/partial writes (intermediate values during assignment)
- On ARM or other weakly-ordered architectures, reads can be reordered
- Manifests as jittery/jumping trajectory positions or occasional pops

**Fix approach:**
- Wrap all final position reads/writes in a spinlock
- OR use atomic<float> for `finalAz_`, `finalEl_`, `finalDist_` (C++20 or manual CAS loop)
- OR double-buffer: keep a struct with all 3 positions, swap atomically via shared_ptr<> or spinlock

**Safety level:** HIGH — affects audio quality during trajectory playback

---

### BinauralRenderer Audio Thread Allocation Guard

**Files:** `src/Binaural/BinauralRenderer.cpp` (lines 205-210)

**Issue:** In `renderSourceBuffers()`, if the pre-allocated convolution temp buffers weren't sized in `prepare()`, the code hits `jassertfalse` and then attempts to resize. In release builds, `jassertfalse` is a no-op, so the resize happens on the audio thread — violating the "no allocation in processBlock" rule.

**Impact:** Occasional stuttering or dropouts if buffer resize is triggered (e.g., if client never calls `prepare()` or calls it with wrong size).

**Fix approach:**
- Change to hard error (throw or return false) instead of assert
- Pre-allocate maximum possible size in constructor
- Document that `prepare()` MUST be called before `processBlock()`

---

## Algorithm Triplet Data Gaps

### Missing VBAP Triplet Computation for 3D Layouts

**Files:** 
- `src/Algorithms/VBAPAlgorithm.cpp` (line 18)
- `src/Algorithms/VBIPAlgorithm.cpp` (line 19)
- `src/Algorithms/MDAPAlgorithm.cpp` (lines 47, 100)

**Issue:** When a speaker layout has height channels (3D), the algorithms compute 3D VBAP gains using a triplet (3-speaker) matrix. The triplet list must be pre-computed and passed in `LayoutContext`. If triplets are empty, the code hits `jassertfalse` and falls back to a 3D nearest-speaker heuristic.

**Problem:** The fallback is a guard for "this shouldn't happen" — but it CAN happen if:
1. A consumer plugin creates a 3D layout without computing triplets
2. The triplet computation in `SpeakerLayout` is incomplete for certain geometries
3. Triplets become stale after layout reconfiguration

**Impact:** When triplets are missing, 3D VBAP gains are incorrect. Audio pans to nearest speaker instead of smooth vector panning. Production audio fails.

**Fix approach:**
- Add defensive triplet computation directly in algorithm (e.g., use KNN to find 3 nearest speakers)
- OR enforce triplets at API level (constructor requires non-empty triplet list for 3D layouts)
- OR add logging to report when fallback is used
- Add test coverage for triplet edge cases

---

## Test Coverage Gaps

### Minimal Test Suite

**Files:** 
- `tests/Algorithms/SpatializationAlgorithmTests.cpp` (65 lines)
- `tests/Trajectory/TrajectoryEngineTests.cpp` (37 lines)
- `tests/IO/SpeakerLayoutTests.cpp` (37 lines)
- `tests/DSP/UtilitiesTests.cpp` (70 lines)

**Issue:** 
- Total test code: 209 lines
- Total source code: 4,084 lines
- Coverage ratio: ~5%

**Untested components:**
- **Binaural rendering** (338 lines): No tests for HRTF loading, crossfading, ITD delay application
- **HRTF Database** (315 lines): No tests for SOFA parsing, onset detection, minimum-phase conversion
- **Partitioned Convolver** (309 lines): No tests for overlap-save, dual-slot transitions, pending IR queue
- **OSC implementations** (179 lines): No tests for message parsing, position conversion, connection state
- **All spatialization algorithms except basic checks** (600+ lines): Missing validation of panning laws, energy conservation
- **UI components** (1,150+ lines): No tests for mouse interaction, rendering, state consistency
- **Trajectory Engine** (478 lines): Only 37 lines of tests — Random trajectory, onset detection paths untested
- **Ambisonics encoding** (244 lines): No tests for SH evaluation, channel ordering, normalization
- **Output format registry** (115 lines): No tests for format negotiation, channel mapping

**Risk:** 
- Edge cases in critical audio paths (convolution, HRTF transition) are unvalidated
- Regression bugs in algorithm gains, channel routing go undetected
- UI drag/drop, OSC message parsing have no safety net

**Fix approach:**
- Set minimum 50% coverage target (2,000+ lines of tests)
- Prioritize: Binaural rendering (crossfade transitions), HRTF onset detection, algorithm energy conservation
- Add integration tests for multi-algorithm rendering pipeline
- Add regression tests for known issues (#50, #89, #96, #131)

---

## Performance & Scaling Concerns

### ITD Delay Buffer Overflow Risk

**Files:** `include/SpatialCore/Binaural/BinauralRenderer.h` (line 97)

**Issue:** ITD (inter-aural time difference) is applied via fractional-sample delay using a circular buffer `itdBufferL/R[MAX_SOURCES][kITDBufferSize]` where `kITDBufferSize = 64`. Maximum ITD value is ~0.7ms (~34 samples @ 48kHz). However, if a malformed HRTF or edge-case elevation angle reports ITD > 64 samples, the circular buffer index math wraps incorrectly and reads stale data or overwrites active samples.

**Impact:** Audio clicks, pops, or distortion when ITD exceeds buffer size.

**Fix approach:**
- Clamp ITD values: `ITD = juce::jlimit(-60.0f, 60.0f, delayL/delayR)`
- Add assert in `renderSourceBuffers()` to catch oversized ITD
- Increase `kITDBufferSize` to 128 for safety margin

---

### Large Stack Allocations in Audio Path

**Files:** `src/Binaural/PartitionedConvolver.cpp` (various)

**Issue:** Convolver slot FFT buffers and work buffers are heap-allocated in `prepare()`, but some temporary arrays for overlap-save may use stack. On embedded systems with small stacks, this risks overflow.

**Current state:** All buffers are `.resize()`'d to heap. No obvious stack issues, but code is not formally validated.

**Fix approach:**
- Audit all function stack frames in audio path with `clang -fstack-size-section` or similar
- Document maximum stack usage per function
- Use JUCE's `jassert(` for stack assertions if available

---

## Error Handling & Robustness

### Minimal Error Propagation

**Files:** Across all source files

**Issue:** Most functions return `bool` for success/failure, but callers often ignore the return value. Example:
- `HRTFDatabase::loadFromMemory()` returns `bool`, but caller in `BinauralRenderer::setProfile()` doesn't check
- `ADMOSCReceiver::connect()` returns `bool`, but usage is unchecked

**Impact:** Silent failures — if SOFA loading fails, the renderer falls back to default behavior without logging why.

**Fix approach:**
- Add logging (via JUCE's `DBG` or custom logger) for all error paths
- Document fallback behavior when primary operation fails
- Consider throwing exceptions for critical failures (SOFA load) instead of silent bool returns

---

### DBG Macro Left in Production Code

**Files:** 
- `src/Binaural/HRTFDatabase.cpp` (lines 36, 50)
- `src/Binaural/BinauralRenderer.cpp` (line 127)

**Issue:** Debug output is compiled into release builds. Causes logging overhead and potential performance regression under certain logging configurations.

**Fix approach:**
- Replace `DBG()` with conditional debug logging (e.g., `jassert()` guard)
- OR remove debug output entirely and rely on assertions for validation

---

## Security & Stability

### No Bounds Checking on OSC Message Parsing

**Files:** `src/OSC/ADMOSCReceiver.cpp` (entire)

**Issue:** OSC message receiver parses `/adm/obj/N/...` messages and extracts object index `N`. No validation that `N < MAX_SOURCES` (12). A malicious or malformed OSC sender could inject `N = 999`, causing out-of-bounds array access.

**Impact:** Crash or undefined behavior.

**Fix approach:**
- Add bounds check: `if (objectIndex < 0 || objectIndex >= MAX_SOURCES) return;`
- Log and ignore out-of-bounds messages

---

### HRTF Binary Data (Git LFS) Dependency

**Files:** `CMakeLists.txt`, HRTF SOFA files in BinaryData

**Issue:** HRTF SOFA files are tracked via Git LFS. If a developer:
1. Clones the repo without LFS client installed
2. Or LFS server is unavailable
3. SOFA files are replaced with pointer files

The build still succeeds (BinaryData includes the pointer text instead of binary), but HRTF loading fails at runtime.

**Impact:** Silent HRTF load failure — users get Simple (Woodworth) binaural instead of SOFA-based rendering. No warning.

**Fix approach:**
- Add CMake check: verify SOFA file sizes are > 100KB (pointer files are ~100 bytes)
- Add `MESSAGE(FATAL_ERROR)` if SOFA files are LFS pointers
- Document Git LFS setup requirement in README and CI
- OR embed SOFA data as compressed C++ arrays instead of external files

---

## Known Limitations

### Limited Speaker Layout Support

**Files:** `src/IO/SpeakerLayout.cpp`, `include/SpatialCore/IO/SpeakerLayout.h`

**Issue:** Only 13 ITU-R BS.775/BS.2051 standard layouts supported. Custom or vendor-specific layouts (e.g., bespoke Atmos configurations) are not supported. Triplet computation for VBAP assumes standard geometry.

**Impact:** Plugins in custom spatial installations must use nearest standard layout, losing precision.

**Fix approach:**
- Add user-defined layout API (speaker positions as floats, auto-compute triplets)
- OR document supported layouts clearly to prevent misuse

---

### JUCE FFT Dependency Risk

**Files:** `CMakeLists.txt`, `include/SpatialCore/Binaural/SharedFFTCache.h`

**Issue:** Convolution uses JUCE's `juce::dsp::FFT`. If JUCE changes FFT implementation or removes it, SpatialCore breaks. JUCE is a GUI framework not a DSP library — this is a brittle dependency.

**Risk level:** MEDIUM — JUCE is stable, but upstream changes could be disruptive.

**Fix approach:**
- Consider switching to standalone DSP library (FFTW, Kiss FFT, Pocketfft)
- OR fork JUCE's FFT and maintain locally

---

### Ambisonics Max-rE Weighting Validation

**Files:** `src/Binaural/AmbisonicsAlgorithm.cpp`, `include/SpatialCore/IO/AmbisonicsCodec.h`

**Issue:** Ambisonics rendering uses max-rE weighting for 3rd order. No validation that decode matrices are numerically stable or that truncation doesn't introduce artifacts.

**Fix approach:**
- Add pre-computed decode matrix tests (check energy preservation, null patterns)
- OR use reference implementation (e.g., ambix) and compare numerically

---

## Recommendations by Priority

### Critical (Address Before Production Release)

1. **Fix TrajectoryEngine race condition** — Add spinlock for `finalAz_/El_/Dist_` reads/writes
2. **Add bounds checking to OSC receiver** — Prevent out-of-bounds array access
3. **Validate HRTF database thread safety** — Add spinlock or document thread constraints
4. **Ensure audio thread allocations never happen** — Replace `jassertfalse` with hard error in `BinauralRenderer`

### High (Next Release)

5. **Expand test coverage** — Target 50% minimum, focus on binaural rendering and algorithm correctness
6. **Add SOFA file presence validation** — Detect Git LFS pointer files before build completes
7. **Implement ITD buffer bounds clamping** — Prevent overflow from large ITD values
8. **Add triplet validation for 3D algorithms** — Fail loudly if triplets missing

### Medium (Next 2 Releases)

9. **Implement minimum-phase HRIR conversion** — Integrate into loading pipeline (currently unused)
10. **Add configurable crossfade duration** — Allow adaptation for different buffer sizes
11. **Replace DBG output** — Use conditional logging or assertions only
12. **Document thread safety model** — Clearly specify which threads call which functions

### Low (Future Enhancement)

13. **Switch to standalone FFT library** — Reduce JUCE dependency
14. **Support custom speaker layouts** — Generalize beyond 13 ITU layouts
15. **Optimize SharedFFTCache** — Consider thread-local or reference counting instead of static holding

---

*Concerns audit: 2026-08-09*
