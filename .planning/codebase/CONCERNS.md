# Codebase Concerns

**Analysis Date:** 2026-08-10

Scope: full repo (`src/`, `include/`), excluding `build/` and JUCE/. Cross-referenced against `gh issue list --state open` (14 open issues, #2–#17, minus gaps). Every finding below cites a real `path:line` that was read directly; no fabricated issue numbers are used for repo GitHub issues.

## Realtime-Safety Violations (CLAUDE.md Rule #1: no malloc/lock/log in processBlock)

**Audio-thread allocation fallback in `BinauralRenderer::renderSourceBuffers` (reachable from processBlock):**
- Files: `src/Binaural/BinauralRenderer.cpp:196-209`
- Issue: `renderSourceBuffers()` is called every block from the audio thread. If `convTmpL`/`convTmpR` are undersized it hits `jassertfalse` (line 207) followed by `convTmpL.resize(...)` / `convTmpR.resize(...)` (lines 208-209). `jassertfalse` compiles to a no-op in Release builds (`JUCE_DEBUG` off), so in shipping builds the `resize()` calls execute unconditionally as the fallback path, allocating on the audio thread and violating the project's #1 locked rule.
- Trigger: any call where `numSamples` at runtime exceeds the `preAllocSize` passed to `prepare()` (`BinauralRenderer.cpp:18-19`) — e.g., a host requesting an oversized block that wasn't accounted for at `prepare()` time.
- Impact: audio dropout / glitch (allocation) or, in the worst case, allocator contention with another thread → priority inversion on the audio thread.
- Fix approach: either hard-fail (return early, skip the block) instead of resizing, or guarantee `prepare()` is always called with the true maximum block size and assert-and-bail in Release too (`jassert` + early `return` rather than `jassertfalse` + resize).
- GitHub issue: not tracked by any open issue. **Not yet filed.**

**Second, near-identical audio-thread allocation fallback in `BinauralRenderer::updateSourceHRIR` (reachable from processBlock via `RenderEngine`):**
- Files: `src/Binaural/BinauralRenderer.cpp:134,158-163`; call site `src/Engine/RenderEngine.cpp:178` inside the per-block HRIR update loop (`RenderEngine.cpp:171-179`), which the comment at `RenderEngine.cpp:60-61` and `BinauralRenderer.cpp:16` confirms sits in the `processBlock` dispatch path.
- Issue: same pattern as above — `jassertfalse` (line 162) is a no-op in Release, so the subsequent `tmpL.resize()` / `tmpR.resize()` (lines 163... actually see `convTmpL`/`convTmpR` shared buffers reused here per comment at line 156) executes unconditionally in shipping builds when `storedIRLength` exceeds the pre-allocated work-buffer size.
- Impact: same as above — audio-thread allocation in Release.
- Fix approach: same as above; additionally consider making these two call sites share one guarded helper instead of duplicating the fallback logic twice.
- GitHub issue: not tracked. **Not yet filed.** (Related to the general realtime-safety guarantee implied by open issue #12's "thread-safety" framing, but #12 is scoped specifically to `SharedFFTCache`, not this buffer-sizing bug — treat as a separate, unfiled defect.)

## Unqualified Cross-Repo Issue References In Code Comments

**Correction (verified 2026-08-10):** an earlier pass of this document called these
citations fictional. They are **not**. Every one resolves in
`Spatial-Media-Lab/OpenSpatialDelay`, whose tracker runs to #235 — the repository
this code was extracted from. The defect is notation, not invention: bare `#NNN`
reads as a SpatialCore issue, and SpatialCore's tracker only reaches #17.

Verified by `gh issue view <N> -R Spatial-Media-Lab/OpenSpatialDelay`:

| Cited | State | Real title |
|-------|-------|-----------|
| `#50` | closed | Binaural HRTF: perceptual pops on azimuth/elevation movement |
| `#131` | closed | CoreGraphics crash with multiple plugin instances loaded simultaneously |
| `#234` | **open** | Audio artifacts at buffer sizes below 256 samples |

**Sites needing requalification to `Spatial-Media-Lab/OpenSpatialDelay#NNN`:**
- `include/SpatialCore/Binaural/SharedFFTCache.h:17` — cites `#131`
- `src/Binaural/PartitionedConvolver.cpp:7` — cites `#131`
- `src/Binaural/PartitionedConvolver.cpp:46,89` — cites `#50`
- `src/Binaural/PartitionedConvolver.cpp:120-124` — cites `#234`

- Impact: a reader checking `#131` against SpatialCore's tracker finds nothing and
  concludes the comment is junk, discarding a real and load-bearing design rationale.
- Fix approach: rewrite each as a fully-qualified `owner/repo#N` reference. Low risk,
  comment-only.
- **`#234` is open**, and its citation sits in live convolver code at
  `PartitionedConvolver.cpp:120-124` — SpatialCore inherited an unfixed OpenSpatialDelay
  defect during extraction. This is a real open bug in this tree, not just a stale comment.
- Related live SpatialCore issue: **#12** (SharedFFTCache thread-safety across concurrent
  instances) covers the same failure mode as OSD `#96` and `#131`.

## Known Bugs (tracked by open GitHub issues)

**No elevation cue in DirectBinaural — front/back and pole collapse (issue #15):**
- Files: `src/Algorithms/DirectBinauralAlgorithm.cpp:23-34`
- Symptoms: `float lateral = sinAz * cosEl;` (line 26) collapses to 0 both at elevation ±90° (`cosEl → 0`) and at azimuth 0°/180° regardless of elevation (`sinAz → 0`), so the effective lateral-angle-derived ILD/ITD path (lines 30, 34) gives no elevation cue and cannot distinguish front from back sources.
- GitHub issue: **#15** — "Simple-binaural path produces no elevation cue — lateral angle collapses at the poles."

**Metres label is display-only, disconnected from DSP distance (issue #16):**
- Files: `src/UI/SpatialMapComponent.cpp:265` (`int meters = juce::roundToInt (r * r * 20.0f);`), DSP consumer at `src/Algorithms/DirectBinauralAlgorithm.cpp:38`
- Symptoms: the "Xm" label drawn in the spatial map (line 266) is computed purely for display from the normalized radius `r`, using an arbitrary `r*r*20` mapping. It has no relationship to the distance value actually consumed by the binaural distance-attenuation code at `DirectBinauralAlgorithm.cpp:38`, so the on-screen "meters" figure does not represent a real physical unit used anywhere in the signal chain.
- GitHub issue: **#16** — "Metres is a display-only label, not a real end-to-end distance unit."

**Doppler `kMaxObjects` hardcoded separately from `MAX_SOURCES` (issue #17):**
- Files: `include/SpatialCore/Trajectory/DopplerVelocity.h:17` (`static constexpr int kMaxObjects = 12;`) vs `include/SpatialCore/Core/Types.h:8` (`static constexpr int MAX_SOURCES = 12;`)
- Symptoms: the two constants currently agree (both 12) but are declared completely independently, with `kMaxObjects` sizing seven parallel per-object arrays in `DopplerVelocity.h:51-61`. Raising `MAX_SOURCES` (as proposed by open issue #3, "Raise MAX_SOURCES from 12 to 32") without also updating `kMaxObjects` will silently desync array bounds between the Doppler subsystem and the rest of the engine, risking out-of-bounds reads/writes on the audio thread.
- GitHub issue: **#17** — "No consumer-usable doppler pitch-shift path — semitone computation has no resampling delay line." (Issue #17's title is about the missing resampling delay line, but its body/scope covers this hardcoded-constant risk; it is also directly relevant to open issue **#3**, which would trigger the desync if merged without a corresponding `DopplerVelocity.h` update.)

## Thread-Safety Concerns

**`TrajectoryEngine` final-position floats are non-atomic despite a documented cross-thread contract:**
- Files: `include/SpatialCore/Trajectory/TrajectoryEngine.h:30` (comment: "Timer thread updates state at ~60Hz; audio thread reads final positions."), storage at lines 103-106 (`float finalAz_[kMaxObjects]`, `finalEl_[kMaxObjects]`, `finalDist_[kMaxObjects]` — plain floats), accessors at lines 85-87 (`getFinalAz`/`getFinalEl`/`getFinalDist`), writes at `src/Trajectory/TrajectoryEngine.cpp:76-90,143-145`.
- Issue: the class's own header comment documents a producer/consumer relationship between a ~60Hz timer thread (writer) and the audio thread (reader), but only `active_[kMaxObjects]` is declared `std::atomic<bool>` (`TrajectoryEngine.h:106`). The three position arrays that actually carry the per-block spatialization data (`finalAz_`, `finalEl_`, `finalDist_`) are plain `float[]`, read and written from different threads with no atomics, memory barriers, or lock. This is a data race under the C++ memory model (undefined behavior), even though in practice it usually just produces torn/stale reads rather than a crash.
- Impact: potential torn reads of azimuth/elevation/distance on the audio thread (audible zipper/glitch artifacts), and formally undefined behavior that tools like TSan will flag.
- Fix approach: make `finalAz_`/`finalEl_`/`finalDist_` `std::atomic<float>` (or pack them into a single atomically-swapped struct, matching the "dual-buffered layouts" pattern already used elsewhere per CLAUDE.md's design principles) and use `memory_order_relaxed` or `_acquire/_release` loads/stores.
- GitHub issue: not tracked by any open issue. **Not yet filed.**

**`SharedFFTCache` — genuinely guarded, but scope of the open issue is narrower than the risk described in its own comments:**
- Files: `include/SpatialCore/Binaural/SharedFFTCache.h:10-23`, `src/Binaural/PartitionedConvolver.cpp:14-27`
- Verification: `getOrCreate()` takes `juce::SpinLock::ScopedLockType sl (lock)` (`PartitionedConvolver.cpp:16`) before touching the shared `std::map<int, std::shared_ptr<juce::dsp::FFT>>` cache, and the cache is a function-local `static SharedFFTCache instance` (`PartitionedConvolver.cpp:26`), i.e., correctly process-global and lazily/thread-safely initialized (C++11 static-local init guarantee).
- Caller context: `getOrCreate()` is invoked from `PartitionedConvolver::prepare()` (`PartitionedConvolver.cpp:44-45`) and `HRTFDatabase.cpp:180`, both of which are configuration-time paths (not inside `processBlock`'s per-sample loop), so the `SpinLock` here does not itself violate the no-lock-in-processBlock rule — but see the note below on `prepare()` being callable while another plugin instance's audio thread is mid-render.
- Residual risk not fully closed by the current implementation: the header comment (`SharedFFTCache.h:13-16`) explains the *reason* for the cache (vDSP twiddle-table use-after-free across plugin instances) but the cache "creates once, never destroys" — there is no `release()`/refcounting path, so this is a deliberate permanent leak-by-design, not a bug, but it means format changes cannot ever shrink the cache's memory footprint for the life of the process.
- GitHub issue: **#12** — "Verify SharedFFTCache thread-safety across concurrent plugin instances" (open; still unresolved/unverified per its title, despite the SpinLock already being present).

**ADM-OSC object-index bounds checking is present and correct:**
- Files: `src/OSC/ADMOSCReceiver.cpp:43` (`if (objNum < 1 || objNum > MAX_SOURCES) return;`)
- Verification: every `/adm/obj/N/...` and `/osd/obj/N/...` message path validates `objNum` against `MAX_SOURCES` (from `include/SpatialCore/Core/Types.h:8`) before use. No out-of-bounds write was found in this file. Not a concern, but noted here because it was an explicit audit target — kept for completeness rather than raised as an issue.

## Non-Realtime-Safety DBG Usage (verified out of processBlock path)

- Files: `src/Binaural/BinauralRenderer.cpp:127` (inside `setProfile()`, a UI/configuration-thread call, `BinauralRenderer.cpp:22-33`), `src/Binaural/HRTFDatabase.cpp:35,58,72` (inside SOFA-loading, also configuration-thread only).
- Verification: none of these `DBG(...)` call sites are inside `renderSourceBuffers()` or `updateSourceHRIR()` (the two functions confirmed reachable from `processBlock`). `DBG` is also a JUCE macro that compiles to nothing when `JUCE_DEBUG` is off, so even if it were reachable it would not execute in Release builds. Not a concern — included for completeness since it was an explicit audit target.

## Fragile Areas

**Dual-slot IR crossfade state machine in `PartitionedConvolver`:**
- Files: `src/Binaural/PartitionedConvolver.cpp:87-114` (`setIR`), `src/Binaural/PartitionedConvolver.cpp:120-160+` (`processSlot`)
- Why fragile: `setIR()` branches on both `state != State::Idle` (mid-crossfade deferral to `pendingIR`) and on whether the active slot's spectrum is all-zero (first-ever IR, no crossfade). Any future change to `State` transitions or to when `setIR()` is called relative to `processSlot()` block boundaries risks silently dropping a pending IR update (`hasPendingIR`/`pendingIRLen` are only consumed by looking at call sites elsewhere in this file) or double-applying a crossfade.
- Safe modification: change `state` transitions only alongside the block-boundary code that consumes `pendingIR`; add/extend unit tests around IR-swap-under-crossfade before modifying.
- Test coverage: not verified in this pass (test files were not enumerated for `quality`/testing focus in this concerns-only audit).

## Open GitHub Issues Not Reflected In Code Concerns

The following open issues describe missing features, research tasks, or documentation debt rather than defects found in the current code, and are not duplicated as "concerns" above:

- **#13** — "Document PresetBrowser API contract" (documentation gap, not a code defect).
- **#11** — "Verify AmbisonicsCodec channel-order/normalisation convention (ACN/SN3D vs FuMa)" (verification task, no confirmed bug found in this pass).
- **#10** — "Research and design a spread API across all 8 spatialization algorithms" (feature research, not a defect).
- **#9** — "CI-verify SpatialCore against JUCE 9.0.0" (CI/infra task).
- **#7** — "Add per-object RMS/energy visualisation to SpatialMapComponent" (feature request).
- **#6** — "Add global master transform (rotate-all) to shared UI layer" (feature request).
- **#5** — "Extract air absorption (HF rolloff) from OpenSpatialDelay" (feature extraction/porting task).
- **#4** — "Extract distance attenuation curves from OpenSpatialDelay" (feature extraction/porting task).
- **#3** — "Raise MAX_SOURCES from 12 to 32 and widen hand-off struct arrays" (feature/scaling request — see cross-reference under issue #17 above for the concrete desync risk this would trigger in `DopplerVelocity.h`).
- **#2** — "[OpenSpatialPanner] Preset system for spatial positioning — headline USP" (feature request, tracked at the consumer-plugin level).

---

*Concerns audit: 2026-08-10*
