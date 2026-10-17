---
phase: 05-realtime-safety-hardening
plan: 06
subsystem: testing
tags: [multi-instance, shared-fft-cache, threadsanitizer, osd-96, osd-131, watchdog, catch2]

requires:
  - phase: 05-realtime-safety-hardening
    provides: "Plans 05-01 to 05-05: allocation and lock traps, build-tsan tree and the [tsan] subset, SPATIALCORE_TEST_TIMEOUT_SCALE use, HRTF handover thread contract"
  - phase: 03-binaural-defects-hrtf-packaging
    provides: "RenderEngine::setHRTFProfile loader and mailbox, per-renderer HRTFDatabase"
provides:
  - "tests/Support/Watchdog.h: spatialcore::test::timeoutScaleFromEnv() and runWithWatchdog (Fn, std::chrono::milliseconds) for new thread tests"
  - "[multi][isolation][tsan]: OSD#96 regression, an engine renders bit-identically (0 of 51,200 samples differ) alone and beside an engine that switches profiles 3, 5, 0, 3"
  - "[multi][stress][tsan]: eight engines create, prepare, render, switch profiles and destroy on eight threads at once, twice, no non-finite sample"
  - "[fftcache][tsan]: four SharedFFTCache cases (identity, retention, orders 9 to 13, sixteen threads on new orders 14 and 15 under a watchdog)"
  - "SharedFFTCache.h states the D-08 contract, provenance and the Windows/Linux platform limit; no behaviour change"
affects: [05-07, 05-08, VERIFY-02, Phase 6 CI-01 (Windows/Linux FFT lock), OpenSpatialDelay migration]

plan_head_before: 8e380441fb0f275abe45b32c882295f6423e82b7
plan_head_after: 0154988fef04b8d6d0be71f360370e873fd0f15d

actuals:
  tokens: 5900
  tasks: 2
  commits: 4

tech-stack:
  added: []
  patterns:
    - "Thread tests keep their state in a std::shared_ptr and assert on the test thread only, inside runWithWatchdog, so a hung library fails fast instead of hanging the binary (Catch2 3.7.1 assertions are not thread-safe)"
    - "D-07: every regression test is shown red against the old behaviour by a scratch mutation, recorded, and restored with git checkout"

key-files:
  created:
    - tests/Support/Watchdog.h
    - tests/Engine/MultiInstanceTests.cpp
    - tests/Binaural/SharedFFTCacheTests.cpp
  modified:
    - tests/CMakeLists.txt
    - include/SpatialCore/Binaural/SharedFFTCache.h
    - src/Binaural/PartitionedConvolver.cpp

key-decisions:
  - "The shared FFT cache is documented as kept for the life of the process, not 'never destroyed': the function-local static is destroyed at exit after JUCE's leak detector, which is the Debug 'Leaked objects detected: N FFT' line (one per order used), a deliberate permanent allocation and not a leak"
  - "Test helpers stay in the new header; ProfileSwitchTests.cpp and VBAPTripletSelectionTests.cpp keep their own copies (plan: do not edit existing files)"

patterns-established:
  - "Concurrent-first-use tests use orders no other test uses (14, 15), so the first use is concurrent even when the whole executable runs in one process"

requirements-completed: [VERIFY-02]

coverage:
  - id: D1
    description: "OSD#96 cannot come back unnoticed: an engine's output is bit-identical (0 of 51,200 samples differ) whether or not another engine switches profiles beside it, and the test goes red when the per-renderer HRTF database is shared"
    requirement: VERIFY-02
    verification:
      - kind: unit
        ref: "tests/Engine/MultiInstanceTests.cpp#Multi-instance: an engine renders bit-identically whether or not a second engine switches profiles beside it (OSD#96)"
        status: pass
    human_judgment: false
  - id: D2
    description: "SharedFFTCache has unit tests: one object per order, retained after every user drops it, distinct objects of the right size for orders 9 to 13, one object per order under sixteen threads released together"
    requirement: VERIFY-02
    verification:
      - kind: unit
        ref: "tests/Binaural/SharedFFTCacheTests.cpp ([fftcache], 4 cases)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Eight engines created, rendered, switched and destroyed on eight threads at once render no non-finite sample and are clean under ThreadSanitizer"
    requirement: VERIFY-02
    verification:
      - kind: unit
        ref: "tests/Engine/MultiInstanceTests.cpp#Multi-instance: eight engines create, prepare, render, switch and destroy on their own threads at once"
        status: pass
      - kind: other
        ref: "SPATIALCORE_TEST_TIMEOUT_SCALE=10 ./build-tsan/tests/SpatialCoreTests '[fftcache],[multi]' (exit 0, 0 ThreadSanitizer warnings)"
        status: pass
    human_judgment: false
  - id: D4
    description: "The SharedFFTCache create-once contract, its provenance (OpenSpatialDelay PR #142 for #131, beside #96) and the Windows/Linux platform limit are written in the header"
    requirement: VERIFY-02
    verification: []
    human_judgment: true
    rationale: "Documentation accuracy is a judgment call; the grep acceptance checks only prove the phrases exist"

duration: ~16min
completed: 2026-10-06
status: complete
---

# Phase 5 Plan 06: Multi-instance proof Summary

**Multi-instance regressions for OSD#96 (bit-identical output beside a profile-switching engine) and the shared FFT cache (identity, retention, 16-thread first use), an eight-engine stress clean under ThreadSanitizer, and the cache's create-once contract written down.**

## Performance

- **Duration:** about 16 min (start time not captured at launch; taken from STATE's `last_updated` 02:04Z)
- **Completed:** 2026-10-06T02:20Z
- **Tasks:** 2
- **Files modified:** 6 (3 created, 3 modified)
- **BASE:** `8e380441fb0f275abe45b32c882295f6423e82b7`

## Accomplishments

- OSD#96 has a deterministic regression: engine A (profile 2, 100 blocks of 256, four sines) renders 51,200 samples, 0 differing bit for bit, alone and while engine B on another thread cycles profiles 3, 5, 0, 3.
- SharedFFTCache, which had no unit test, now has four, including sixteen threads released together on orders 14 and 15 under a watchdog.
- Eight engines on eight threads (Binaural or 7.1.4, 44.1 or 48 kHz, 400 moving blocks, two profile switches each, two rounds) finish with 0 non-finite samples; the whole `[fftcache],[multi]` subset is clean under ThreadSanitizer.
- `SharedFFTCache.h` carries the D-08 contract, provenance and platform limit (Windows/Linux fallback FFT SpinLock, checked in Phase 6).

## Task Commits

1. **Task 1 (tracer): OSD#96 bit-identity** - `4524c10` (test)
2. **Task 2: cache tests and eight-engine stress** - `056915c` (test)
3. **Task 2: D-08 contract documentation** - `464787e` (docs), `0154988` (docs, keeps the "life of the process" phrase on one line for the acceptance grep)

**Plan metadata:** committed separately (docs: complete plan).

## D-07 Mutation Results (each restored with `git checkout`, tree rebuilt)

| # | Mutation | Test | Result |
|---|----------|------|--------|
| 1 | `BinauralRenderer::hrtfDatabase` replaced by a reference to one function-local static `HRTFDatabase` (Release) | `[multi][isolation]` | **Red by crash in 3 of 3 runs**: exit 133 (SIGTRAP, no output) once, exit 134 (SIGABRT) twice, Catch2 reports the test failed "due to a fatal error condition". No differing-sample count was obtained: the shared database is mutated by two loaders at once and the process dies before the comparison. RESEARCH F7's 37,886 of 51,200 came from a probe that survived; here the failure is the stronger form of the same fault (it is the OSD#96 crash itself). |
| 2 | Removed `juce::SpinLock::ScopedLockType sl (lock);` from `getOrCreate` (build-tsan, scale 10) | `[fftcache]` | **Red**: 32 `WARNING: ThreadSanitizer` reports (30 data race, 2 data race on vptr), exit 137 (killed by `alarm 420` guard after the test failed; the run took about 1 s to fail), concurrent case failed `differing14 == 0` / `differing15 == 0` style assertions (SharedFFTCacheTests.cpp:125, :126), 3 of 4 cases passed. |
| 3 | `getOrCreate` returns a fresh `std::make_shared` every call, map ignored (Release) | `[fftcache]` | **Red**: 3 of 4 cases fail, 6 assertions: identity (`first.get() == second.get()`), retention (`watcher.expired()`, `again.get() == address`, `again == watcher.lock()`) and both concurrent-case checks. |

After each restore, `git diff 8e380441 -- include/SpatialCore/Binaural/BinauralRenderer.h` is empty and `git diff 8e380441 -- src/Binaural/PartitionedConvolver.cpp` has no line outside a comment.

## Verification Results

| Gate | Result |
|------|--------|
| build-tsan `[fftcache],[multi]`, scale 10, `halt_on_error=0 second_deadlock_stack=1` | exit 0, 0 `WARNING: ThreadSanitizer`, 6 test cases, 44 assertions, **39 s** (35 s on the first run) |
| Debug `SpatialCoreTests` (full) | 386 test cases, 328,307 assertions, all passed (exit 0) |
| Debug `SpatialCoreUITests` | 14 test cases, 133 assertions, all passed (exit 0) |
| Release `ctest --test-dir build-release/tests` | 400 of 400 passed (113.55 s); `ctest -N | grep -c 'ui:'` prints 14 |
| Release `[fftcache]` / `[multi]` / `[multi][isolation]` | 4 cases (34 assertions) / 2 cases (10) / 1 case (6), all passed |
| `tests/CMakeLists.txt` CRLF | `git ls-files --eol` reports `w/crlf`; diff vs BASE is 2 added lines |
| Acceptance greps | `life of the process` 1; `OpenSpatialDelay#(96\|131)` 2; bare `#96`/`#131` 0; `MultiInstanceTests.cpp` in CMake 1 |

## Decisions Made

- The cache comment says "kept for the life of the process" and explains the Debug leak-detector line, replacing "creates once, never destroys" (the function-local static is destroyed at exit).
- The vDSP hypothesis from OpenSpatialDelay PR #142 is recorded as not reproduced on macOS 26 / Apple Silicon, so the tests pin invariants, not that crash.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug in own deliverable] "life of the process" phrase wrapped across two comment lines**
- **Found during:** final acceptance-criteria run (`grep -c "life of the process"` printed 0)
- **Issue:** The D-08 phrase was split by a line wrap, so the plan's own acceptance grep did not match.
- **Fix:** Re-wrapped the comment so the phrase sits on one line; comment-only.
- **Files modified:** `include/SpatialCore/Binaural/SharedFFTCache.h`
- **Verification:** grep prints 1; Release `[fftcache],[multi]` still passes.
- **Committed in:** `0154988`

**2. [Process note] TDD "RED" for Task 2 is the D-07 mutation, not a failing commit**
- The tests describe behaviour the cache already has, so there is no failing-first commit; the red evidence is mutations 2 and 3, recorded above. Task 1's mutation (1) was a crash, which the plan allows to count as red.

**3. [Process note] `timeout(1)` is not on macOS**
- The first mutation run used `timeout 400` and exited 127 without running anything (noticed, not recorded as a result); it was rerun without it, and later guarded runs use `perl -e 'alarm N; exec @ARGV'`.

---

**Total deviations:** 1 auto-fixed (Rule 1) plus 2 process notes
**Impact on plan:** None on scope or behaviour. No production line outside a comment changed.

## Issues Encountered

- A Debug run prints "Leaked objects detected: N instance(s) of class FFT" and a JUCE assertion line at exit (N grows with the number of orders used: 2 for the isolation case alone, 7 for the new subset). Exit status stays 0. This is the known STATE Deferred Item (SharedFFTCache static outlives JUCE's leak detector); the new header comment now documents it as deliberate.

## Known Stubs

None.

## Threat Flags

None. Tests and comments only; no new endpoint, auth path, file access or schema change. Threat register: T-05-14 and T-05-15 mitigated and shown red by mutation; T-05-16 transferred to Phase 6 (documented platform limit).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- VERIFY-02 engine side is done. The UI half of OSD#131 (typeface duplication) is Plan 05-07.
- Not claimed: real-DAW multi-instance behaviour, Windows or Linux (fallback FFT SpinLock contention, A3 assumption), and Apple's vDSP concurrent-perform guarantee (A2, mitigated by the isolation test and the eight-engine TSan run, not proven from documentation).
- Ready for 05-07.

## Self-Check: PASSED

- Created files exist: `tests/Support/Watchdog.h`, `tests/Engine/MultiInstanceTests.cpp`, `tests/Binaural/SharedFFTCacheTests.cpp`.
- Commits `4524c10`, `056915c`, `464787e`, `0154988` all found; `git rev-list --count 8e38044..HEAD` was 4 before this SUMMARY.
- All task acceptance criteria re-run and passing (see Verification Results); plan-level verification 1 to 3 satisfied.

---
*Phase: 05-realtime-safety-hardening*
*Completed: 2026-10-06*
