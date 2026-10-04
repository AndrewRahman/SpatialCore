---
phase: 03-binaural-defects-hrtf-packaging
reviewed: 2026-10-04T17:42:20Z
depth: standard
files_reviewed: 36
files_reviewed_list:
  - CMakeLists.txt
  - include/SpatialCore/Binaural/BinauralRenderer.h
  - include/SpatialCore/Binaural/HRTFDatabase.h
  - include/SpatialCore/Binaural/HRTFProfile.h
  - include/SpatialCore/Binaural/HRTFProfileResolver.h
  - include/SpatialCore/Binaural/PartitionedConvolver.h
  - include/SpatialCore/Core/SimpleBinauralCues.h
  - include/SpatialCore/Engine/RenderEngine.h
  - include/SpatialCore/SpatialCore.h
  - src/Binaural/BinauralRenderer.cpp
  - src/Binaural/EmbeddedHRTF.cpp
  - src/Binaural/HRTFDatabase.cpp
  - src/Binaural/HRTFProfileResolver.cpp
  - src/Binaural/PartitionedConvolver.cpp
  - src/Engine/RenderEngine.cpp
  - tests/CMakeLists.txt
  - tests/Algorithms/PanningLawTests.cpp
  - tests/Binaural/BinauralMetrics.h
  - tests/Binaural/BinauralTestUtilities.h
  - tests/Binaural/BinauralCueTests.cpp
  - tests/Binaural/BinauralRendererTests.cpp
  - tests/Binaural/PartitionedConvolverTests.cpp
  - tests/Binaural/ProfileLoudnessTests.cpp
  - tests/Binaural/HRTFEmbeddedTests.cpp
  - tests/Binaural/BernschuetzKU100Tests.cpp
  - tests/Binaural/CipicSubject003Tests.cpp
  - tests/Binaural/HutubsPP2Tests.cpp
  - tests/Binaural/MitKemarLargePinnaTests.cpp
  - tests/Binaural/SadieD2KU100Tests.cpp
  - tests/Engine/ProfileSwitchTests.cpp
  - CLAUDE.md
  - README.md
  - docs/development-roadmap.md
  - docs/integration-guide.md
  - .claude/skills/spatial-audio-dsp/SKILL.md
  - .claude/skills/spatialcore-architecture/spatialcore-architecture.md
findings:
  critical: 0
  warning: 7
  info: 7
  total: 14
status: issues_found
---

# Phase 3: Code Review Report

**Reviewed:** 2026-10-04T17:42:20Z
**Depth:** standard
**Files Reviewed:** 36
**Status:** issues_found

## Summary

Reviewed the phase diff (`15778d4^..HEAD`) for the HRTF profile-switch mailbox and worker
(`HRTFProfileLoader`, `claimReadyRenderer`, `endRendererFade`), the resolver and embedded-data
path, the Simple-path cue bank, the sample-timed convolver and renderer crossfades, the CMake
changes, the docs and the new tests. I also ran `[renderer],[hrtf-resolve],[convolver]` against
`build-release/tests/SpatialCoreTests`: all 19 cases pass, which includes the libmysofa v1.3.5
build in `build/_deps`.

The lock-free protocol holds up. I traced the renderer ownership flags (`rendererFree_`,
`readyRenderer_`, `rendererMeta_`, `activeRendererIndex`) through every path I could construct:
claim on the HRTF path, claim on a non-HRTF path, a path change mid-fade, a request superseded
during a load, `prepare()` during a load, and a stale unclaimed result. I found no state in which
the worker and the audio thread write the same renderer. The audio side of the mailbox (atomics
only) allocates nothing, takes no lock and touches no file. Flag-off consumers go through the
legacy dispatch. They see only the documented changes: sample-based fades and the Simple-path cue
bank.

No BLOCKER was found. The main weaknesses are in the untrusted-input handling around the shared
folder. The 256 MB cap bounds file bytes only, and the worker thread has no failure path for an
oversized or throwing load. There is also a thread-contract gap on `prepare()`, a deferred
64-sample ITD line that is pinned by tests, and a silent-output foot-gun in the new opt-in flag.

## Warnings

### WR-01: Decoded IR size is not bounded, so a file under the 256 MB cap can force a multi-GB allocation

**File:** `src/Binaural/HRTFDatabase.cpp:56-89`, `src/Binaural/BinauralRenderer.cpp:52,77,100-103`, `src/Binaural/HRTFProfileResolver.cpp:58-65`
**Issue:** T-03-09 is mitigated only by `kMaxSharedHRTFFileBytes`, which limits the file's bytes.
After `mysofa_open_data` succeeds, `loadFromBytes` accepts whatever `N` (filter length) and `M`
came out. libmysofa v1.3.5 validates the SOFA structure (`R == 2`, `M > 0`) but puts no ceiling
on `N`, and `mysofa_resample` scales `N` by `target/fileRate` with no cap. `BinauralRenderer::setProfile`
then does the following:
- It allocates `tmpIRL/tmpIRR` of `N` floats.
- It runs `PartitionedConvolver::prepare (blockSize, N)` on **24 convolvers**.
- Each convolver allocates two slots of about `5 * fftSize` floats, where `fftSize` is the next power of two above `blockSize + N - 1`.

I did not craft a hostile file, so the figure below is arithmetic rather than a measurement. At
`N = 2^22` samples (a roughly 130 MB single-position file), `fftSize = 2^23` and the convolvers
need about `48 * 5 * 2^23 * 4` bytes, which is about 8 GB. `convTmpL/R` also grow to `N` in
`ensureScratchCapacity`. A SOFA file that declares a very low `DataSamplingRate` multiplies this
further. The result is OOM or a dead worker (see WR-02) from a file the cap was meant to reject.
The shared folder is the project's stated untrusted source (T-03-09). On Windows, `%ProgramData%`
subfolders are often user-writable unless the installer sets ACLs, and no installer exists yet
(SUITE-01).
**Fix:** Validate right after load, before any renderer sizing. Reject (as "unreadable") any
database that is out of range, so the resolver falls back to the embedded copy:
```cpp
// HRTFDatabase::loadFromBytes, after mysofa_open_data succeeds
constexpr int kMaxIRLength = 16384;      // largest shipped IR is 558 at 48 kHz; 16384 is ~340 ms
constexpr int kMaxPositions = 100000;    // placeholder; set from the largest shipped M with headroom
if (filterLength < 1 || filterLength > kMaxIRLength
    || easyHandle->hrtf->M < 1 || easyHandle->hrtf->M > kMaxPositions)
{
    unload();
    return false;
}
```
Apply the same bound to the embedded path only if you want symmetry. The embedded data is trusted.

### WR-02: The loader thread has no failure path, so one throwing load leaves the engine "Loading" forever

**File:** `src/Engine/RenderEngine.cpp:78-97, 216-229`
**Issue:** `HRTFProfileLoader::run()` and `serveRequest()` have no `try/catch`. `juce::MemoryBlock`,
`std::vector` and `HeapBlock` throw `std::bad_alloc` on exhaustion (WR-01, a large shared file),
and the worker runs all of them. JUCE's `Thread::threadEntryPoint` swallows the exception
(`catch (...) { jassertfalse; }`) and the thread simply ends. Afterwards:
- `loaderBusy_` stays `true` and `handledSerial_` never catches up.
- `getHRTFProfileStatus()` reports `Loading` indefinitely.
- `waitForHRTFProfileIdle` never returns `true`.
- The held renderer is never returned (`releaseHeld()` is skipped), so `rendererFree_` stays `false`.
- `hrtfLoader_` is non-null, so `setHRTFProfile` never restarts a worker.

The doc promise "a failed load never mutes anything and the status says what went wrong" is not
met for this class of failure.
**Fix:** Catch inside the loop and settle the request as failed:
```cpp
owner.loaderBusy_.store (true, std::memory_order_release);
bool settled = false;
try { settled = serveRequest (serial); }
catch (...)
{
    releaseHeld();
    publishStatus (owner.requestedProfile_.load(), HRTFLoadState::Failed,
                   HRTFProfileSource::None, HRTFProfileProblem::SharedFileUnreadableNoBuiltIn);
    settle (serial);
    settled = true;
}
owner.loaderBusy_.store (false, std::memory_order_release);
```
Consider adding a dedicated `HRTFProfileProblem` value, such as `OutOfMemory`, rather than reusing
an existing one. Also add a test using an injected oversized-IR file once WR-01 lands.

### WR-03: The size cap is a stat-then-read check, so special files and symlinks bypass it

**File:** `src/Binaural/HRTFProfileResolver.cpp:58-65`, `src/Binaural/HRTFDatabase.cpp:46`
**Issue:** The resolver checks `sharedFile.getSize() > maxSharedFileBytes` and then calls
`loadFromFile`, which uses `File::loadFileAsData` and reads until EOF with no limit.
`existsAsFile()` is true for a FIFO, a device node or a symlink to one. For those, `getSize()` is
0, so the cap passes. The read then either blocks the worker forever (FIFO) or grows memory
without bound (for example a symlink to `/dev/zero`). `prepare()` and `~RenderEngine` then stall
for the 15 s `stopThread` timeout and JUCE force-kills the thread. A file that grows between the
check and the read is the same hole. This needs write access to the shared folder, as in WR-01.
**Fix:** Enforce the cap while reading, from one open handle:
```cpp
juce::FileInputStream in (sofaFile);
if (! in.openedOk() || in.getTotalLength() > maxBytes) return false;
juce::MemoryBlock bytes;
in.readIntoMemoryBlock (bytes, (ssize_t) maxBytes + 1);
if ((juce::int64) bytes.getSize() > maxBytes) return false;   // grew or lied about its size
```
Plumb `maxBytes` through `HRTFDatabase::loadFromFile (file, rate, maxBytes = default)`, or do the
read in the resolver and call `loadFromMemory`. Reject non-regular files, because `existsAsFile`
alone is not enough.

### WR-04: `prepare()` changed contract (stops and restarts a thread, can block 15 s) but is undocumented and unsynchronised with `setHRTFProfile`

**File:** `src/Engine/RenderEngine.cpp:414-425, 487-522`, `include/SpatialCore/Engine/RenderEngine.h:228-236`
**Issue:** `prepare()` now does the following:
- It joins the loader with `stopThread (15000)`, destroys `hrtfLoader_`, and creates and starts a new one.
- It writes `hrtfEverRequested_`, `requestSerial_` and `forceReload_`, and re-prepares the active renderer.

`setHRTFProfile` touches the same `hrtfLoader_` unique_ptr and `hrtfEverRequested_` (plain
members) and is documented as message-thread-only. `prepare()` has no stated thread. JUCE
`prepareToPlay` is called from non-message threads by some hosts. If it overlaps a
`setHRTFProfile` from the UI or a parameter change, there is a data race on `hrtfLoader_`, which
can mean a double-start or a use-after-reset. The header comment on `prepare()` still describes
only the old behaviour (LFE filter and buffers). Nothing tells a consumer that `prepare()` blocks
for a load in progress, or that it must not overlap `setHRTFProfile` or `renderBlock`.
**Fix:** Either state the contract in the `prepare()` header comment ("same thread as
`setHRTFProfile`, never concurrent with `renderBlock`; may block until a load in flight
finishes"), or guard `hrtfLoader_`, `hrtfEverRequested_` and the serial bookkeeping with a
message-side `juce::CriticalSection` (never taken on the audio thread) shared by `prepare()` and
`setHRTFProfile()`.

### WR-05: The 64-sample ITD line wraps real SOFA delays, and the characterisation tests pin the defect

**File:** `include/SpatialCore/Binaural/BinauralRenderer.h:105`, `src/Binaural/BinauralRenderer.cpp:282-292`, `tests/Binaural/BinauralRendererTests.cpp:229-331`
**Issue:** `kITDBufferSize = 64` and the read pointer masks with `& 63`, so any `delL/delR >= 63`
wraps modulo 64. The pinned table shows the delays the engine actually produces. SADIE at 48 kHz
has delays of 66 to 102 samples, and at 44.1 kHz 71 to 93. These are the SOFA delay plus the
detected onset from `getAlignedHRIR`, so they are mostly bulk offset. Whether the inter-ear
difference survives the wrap is accidental. It holds only when both ears are on the same side of
a multiple of 64. A pair such as (62, 66) comes out wrong, even sign-flipped. At 88.2, 96 and
192 kHz, which the engine accepts, the delays double or more and wrap several times.
`CHECK (maxSadie48Delay >= 64.0f)` asserts that the defect exists, so a fix breaks the test. The
review brief and the commit message say this is deliberately deferred and tracked as
AndrewRahman/SpatialCore#25, so I am not calling it unnoticed. It stays a WARNING because it is
shipped, audible-class incorrect behaviour on the HRTF path and nothing at runtime guards it.
**Fix:** Size the line from the sample rate and remove the baked-in common offset. Use a power of
two, at least `ceil (1.5e-3 * sampleRate) + 2` (256 covers 192 kHz), and subtract
`min (delayL, delayR)` where the onset-detect fallback sets the delays. Then update
`kItdTable` and the `>= 64` check in the same commit. Until then, keep #25 linked from
`kITDBufferSize`.

### WR-06: `engineSelectsHRTF` without `engineComputesGains` renders silence on the Simple path, with no guard

**File:** `src/Engine/RenderEngine.cpp:656-674`, `include/SpatialCore/Engine/RenderEngine.h:198-211`
**Issue:** `RenderBlockContext::objGains` is zero-initialised. The Woodworth path reads
`blockCtx.objGains`. With `engineSelectsHRTF = true` and `engineComputesGains = false`, profile 0
and every Simple-to-HRTF blend run on zero gains, so there is silence or half-level audio and no
diagnostic. The requirement exists only in a comment and in the docs ("set it together with
`engineComputesGains`"). This is the same class of consumer-side contract that SC-16 chose to
avoid for dispatch derivation. A consumer that sets only the new flag gets a mute plugin on
profile 0.
**Fix:** Make the dependency structural so that no combination of flags is silent:
```cpp
if (blockCtx.engineComputesGains || blockCtx.engineSelectsHRTF) ... computeObjectGains (src, layout, gainScratch_);
```
That is, `engineSelectsHRTF` implies gain computation for binaural blocks. The default-off
behaviour is unchanged. Update the header and docs accordingly. Alternatively add a test that
pins the current behaviour and document it as a known footgun.

### WR-07: Legacy (flag-off) consumers see output changes, which sits uneasily with the "ALWAYS maintain backward compatibility" rule

**File:** `src/Engine/RenderEngine.cpp:761-777, 1030-1117`, `src/Binaural/PartitionedConvolver.cpp:229-247`, `docs/integration-guide.md` ("The Simple profile sounds different")
**Issue:** The new opt-in flags are default-off as promised, but three behaviours change for every
existing consumer with no opt-in:
- The Simple path now runs the rear/up/down cue bank and is no longer flat (BUG-01, D-02, "no switch back").
- The convolver warm-up now needs at least `irLen` samples, so a 558-sample KEMAR IR at 512-sample calls warms for two calls instead of one.
- The renderer crossfade is `max (8 * block, 4096)` samples, so block sizes of 256 and below now fade longer than before.

The first is documented and was a deliberate decision. The other two are noted in code comments
but not in the integration guide's migration section, and the header comment says "byte-for-byte
unchanged ... apart from the sample-based renderer crossfade" while omitting the convolver
warm-up change. CLAUDE.md's critical rule says existing plugins must keep working. An output
change for sources outside the front half is, by SpatialCore's own versioning table, at least a
minor bump with release notes.
**Fix:** Add the convolver warm-up and renderer fade changes to the integration guide's
behavioural-change note and the `RenderEngine.h:206-210` comment. Make the version bump and
release note part of the phase gate, because OSD regression recordings of the Simple path will
change.

## Info

### IN-01: Docs state "SOFA load up to 36 MB" but the code accepts shared files up to 256 MB

**File:** `include/SpatialCore/Engine/RenderEngine.h:359`, `CLAUDE.md:98`
**Issue:** Both claim the loader handles "up to 36 MB". That is the largest built-in. A
shared-folder override is accepted up to `kMaxSharedHRTFFileBytes` (256 MB), so load time and
peak memory are about 7x the documented figure. This also understates the worst case for the
`stopThread (15000)` budget in `prepare()` and the destructor.
**Fix:** Say "up to 36 MB built in, up to 256 MB from the shared folder".

### IN-02: "No lock, allocation or file access is ever on the audio side" is stronger than the code

**File:** `CLAUDE.md:98`, `docs/integration-guide.md` ("Threading")
**Issue:** The mailbox claim is atomics-only, as stated. But `BinauralRenderer::updateSourceHRIR`
and `renderSourceBuffers`, and `RenderEngine::renderDirectBinauralHRTF`, still contain
`jassertfalse` plus `resize` fallbacks on the audio thread. They are deferred as RTSF-01 (Phase 5)
and are reachable only on a prepare-contract violation. The sentence reads as a whole-path
guarantee.
**Fix:** Scope the claim to the profile-switch handoff, or add "(the remaining defensive resize
guards are RTSF-01)".

### IN-03: Worker polls every 2 ms indefinitely when the audio thread is not rendering

**File:** `src/Engine/RenderEngine.cpp:192-209`
**Issue:** A second request must wait for the faded-out renderer to be freed, and only the audio
thread frees it (`endRendererFade`). If the host stops calling `processBlock` (a suspended or
idle track), the worker spins in `sleep (2)` until audio resumes. That is about 500 wakeups per
second on a low-priority thread, and the status stays `Loading` without any indication why.
Correctness is fine and shutdown is prompt.
**Fix:** Poll with exponential backoff (2 ms up to about 50 ms), or `wait()` on the thread's
event and have `endRendererFade` skip the signal (it must not signal from the audio thread).

### IN-04: `loadFromFile` read failure leaves a stale handle behind `loaded = false`

**File:** `src/Binaural/HRTFDatabase.cpp:46-51`
**Issue:** On a read failure the function sets `loaded = false` but does not call `unload()`. A
previously loaded `easyHandle`, `irLength` and `numPositions` stay allocated and inconsistent with
`isLoaded()`. It is harmless in the resolver (the next load unloads first), but the contract
"returns false ... " leaves the database half-loaded.
**Fix:** Call `unload()` first, as `loadFromBytes` and `loadFromBinaryData` do.

### IN-05: Test provenance comment names libmysofa v1.3.2; the tree pins v1.3.5

**File:** `tests/Binaural/BinauralRendererTests.cpp:229`
**Issue:** The comment says the ITD table was "captured ... libmysofa v1.3.2". The pin moved to
v1.3.5 in `c1c2e42` (plan 03-08), after the capture (plan 03-01). The tests pass on v1.3.5 (I
ran them), so the data is still valid, but the provenance line is now misleading.
**Fix:** Re-capture or annotate "values unchanged under v1.3.5".

### IN-06: `SKILL.md` and `SimpleBinauralCues.h` disagree on the measured overhead figure

**File:** `.claude/skills/spatial-audio-dsp/SKILL.md:178`, `include/SpatialCore/Core/SimpleBinauralCues.h:38`
**Issue:** `SKILL.md` says "overhead 2.6 dB RMS". The header records "Up 2.70 / 8.4" from the same
research. One of them is a rounding or transcription slip.
**Fix:** Use one number, preferably the header's 2.7.

### IN-07: Test reliability, wall-clock assertions and CMake edge case

**File:** `tests/Engine/ProfileSwitchTests.cpp:818,921`, `CMakeLists.txt` (`get_target_property (_sc_mysofa_bindir mysofa-static BINARY_DIR)`)
**Issue:**
- `CHECK (result.slowestCallMs < 50.0)` is a wall-clock bound on `setHRTFProfile`. The first call also starts a thread. Local runs pass, but the check can flake on a loaded machine. CI runs no tests today, so it only bites local runs.
- For an `IMPORTED` consumer-supplied `mysofa-static`, `BINARY_DIR` is not defined. `target_include_directories (SpatialCore PRIVATE "<var>-NOTFOUND")` then adds a bogus path instead of failing clearly.
- Several `HRTFEmbeddedTests` assertions are wrapped in `if (profileIsEmbedded (...))`, so in a `SPATIALCORE_EMBED_ALL_HRTF=OFF` build the "broken shared file" case asserts nothing.

**Fix:** Scale the 50 ms bound by `timeoutScale()`, or assert only that the call does not wait on the load. Guard the CMake line with `if (_sc_mysofa_bindir)`. Add an `else` branch in the OFF-build test case that checks the `SharedFileUnreadableNoBuiltIn` result.

---

_Reviewed: 2026-10-04T17:42:20Z_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
