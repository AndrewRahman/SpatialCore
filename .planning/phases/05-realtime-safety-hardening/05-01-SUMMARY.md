---
phase: 05-realtime-safety-hardening
plan: 01
subsystem: testing
tags: [realtime-safety, malloc_logger, pthread-interpose, catch2, ambisonics, nfc, juce-iir]

requires:
  - phase: 04-control-routes
    provides: "RenderEngine facade with engineComputesGains / engineDerivesDispatch, used to drive every render path from tests"
provides:
  - "SpatialCoreRTGuard: test-only macOS trap that counts malloc, free and pthread locks on one armed thread"
  - "RealtimeGuard.h helpers (ScopedRealtimeTrap, requireRealtimeTrapCanary, skip when unavailable)"
  - "RealtimeSafetyTests.cpp with MovingScene, OutputBank and armedRender, reused by Plans 05-02 and 05-03"
  - "Ambisonics NFC coefficient update that no longer allocates on the audio thread, with the sound byte-identical (D-10)"
affects: [05-02, 05-03, 05-04, RTSF-01, RTSF-05, VERIFY-02]

actuals:
  tokens: 7384
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - "Arm a per-thread trap around exactly one renderBlock call; fill inputs while disarmed; never warm up first"
    - "Canary before every trap case: a trap that cannot see reports a false zero"
    - "Before/after raw-output capture compared with cmp as the byte-identical proof for an audio-path change"

key-files:
  created:
    - tests/Support/rtguard/RTGuard.cpp
    - tests/Support/RealtimeGuard.h
    - tests/Engine/RealtimeSafetyTests.cpp
  modified:
    - tests/CMakeLists.txt
    - src/Engine/RenderEngine.cpp

key-decisions:
  - "The trap is a libmalloc malloc_logger hook plus a __DATA,__interpose table in a linked dylib, because Apple clang 21 rejects -fsanitize=realtime"
  - "The trap target exists only when APPLE and CMAKE_CXX_FLAGS has no -fsanitize; elsewhere every trap case SKIPs rather than passing green"
  - "NFC fix assigns std::array<float, 4> in place (not getRawCoefficients writes), proven bit-identical by [nfc][d10] and by cmp of the raw output"

patterns-established:
  - "Trap discipline: no per-thread storage, no logging, no allocation inside the hook; armed id is an std::atomic<pthread_t>"
  - "Hidden [.][...-capture] Catch2 case writes raw output to a file named by an environment variable for before/after comparison"

requirements-completed: [RTSF-05]

coverage:
  - id: D1
    description: "Test-only trap library counting allocations, frees and pthread locks on the one armed thread, never linked into SpatialCore or SpatialCoreUI"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "build-release/tests/SpatialCoreTests [rt][canary]"
        status: pass
      - kind: other
        ref: "git grep -l -e malloc_logger -e __interpose -- src include CMakeLists.txt (prints nothing)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Canary proves the trap sees a deliberate malloc, new, juce::CriticalSection and std::mutex before any zero is believed"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: the trap sees a deliberate allocation and a deliberate lock"
        status: pass
    human_judgment: false
  - id: D3
    description: "Cold-start 7.1.4 render of 12 moving objects allocates, frees and locks nothing over 120 blocks from the first block after prepare()"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: a cold-start 7.1.4 render of 12 moving objects allocates and locks nothing"
        status: pass
    human_judgment: false
  - id: D4
    description: "All six Ambisonics formats render 120 cold-start moving blocks with zero allocations, frees and locks"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Realtime: every Ambisonics format renders moving distance without allocating"
        status: pass
    human_judgment: false
  - id: D5
    description: "NFC fix is bit-identical: coefficient identity over 3 rates x 6 orders x 9 distances, and cmp of the six formats' raw output before and after in Debug and Release"
    requirement: RTSF-05
    verification:
      - kind: unit
        ref: "tests/Engine/RealtimeSafetyTests.cpp#Ambisonics NFC: the in-place array assignment stores the old coefficients bit for bit"
        status: pass
      - kind: other
        ref: "cmp /tmp/sc-p5-nfc-before-{debug,release}.bin /tmp/sc-p5-nfc-after-{debug,release}.bin (both exit 0)"
        status: pass
    human_judgment: false
  - id: D6
    description: "Full Debug suite, Debug UI suite and Release ctest pass; ctest lists ui: tests"
    verification:
      - kind: integration
        ref: "./build/tests/SpatialCoreTests; ./build/tests/SpatialCoreUITests; ctest --test-dir build-release/tests"
        status: pass
    human_judgment: false

duration: 13min
completed: 2026-10-06
status: complete
plan_head_before: 014a4103e764f1fbfaf71433a63ae8ab42b74350
plan_head_after: 5d8e7245b7e66c65915ebbdffb79c52496168f78
commits: 3
---

# Phase 5 Plan 01: Realtime trap and Ambisonics NFC allocation fix Summary

**A macOS-only allocation and pthread-lock trap (malloc_logger hook plus a dylib interpose table) proven by a canary, used to show the six Ambisonics formats allocated 1,427 to 8,562 times per 120 blocks on the audio thread, then fixed with an in-place `std::array<float, 4>` coefficient assignment whose output is byte-identical before and after.**

## Performance

- **Duration:** about 13 min (start time was not captured at the moment of starting; estimated from the first reads, 23:22Z, to 23:35Z)
- **Completed:** 2026-10-05T23:35Z
- **Tasks:** 2 (Task 1 tracer, Task 2 TDD: RED, GREEN)
- **Files modified:** 5 (3 created, 2 modified)

## Accomplishments

- `SpatialCoreRTGuard` counts malloc, calloc, realloc, posix_memalign, operator new, free and pthread mutex, rwlock and condvar waits on the one thread armed; other threads are never counted. It is built from `tests/` and linked into `SpatialCoreTests` only.
- The canary REQUIREs that the trap sees a deliberate `std::malloc`, `new`, `juce::CriticalSection` lock and `std::mutex` lock in the same build before any zero is believed. Both trees pass it with none skipped.
- Tracer: a cold-start 7.1.4 render of 12 moving objects measures 0 allocations, 0 frees and 0 locks over 120 blocks of 256, from the first block after `prepare()`, in Debug and Release.
- The only red result the research found is fixed: `renderAmbisonicsOutput` no longer builds a temporary `juce::dsp::IIR::Coefficients<float>`. All six Ambisonics formats now measure 0 allocations, 0 frees and 0 locks, and the sound is unchanged byte for byte.
- `MovingScene`, `OutputBank` and `armedRender` are in place for Plans 05-02 and 05-03.

## Evidence (as the plan's `<output>` requires)

- **BASE:** `014a4103e764f1fbfaf71433a63ae8ab42b74350` (HEAD when the plan started).
- **BASE Release test count:** 339 (`ctest --test-dir build-release/tests -N | tail -1`, taken after the first build and before any new test). After this plan: 343 = 339 + 4 non-hidden cases (canary, tracer, ambi, nfc). The hidden `[.][ambi-nfc-capture]` case is not registered by ctest (0 matches in `ctest -N`).
- **D-07 old-behaviour evidence, six BASE allocation counts** (`./build-release/tests/SpatialCoreTests "[rt][ambi]"`, 120 cold-start moving blocks of 256 at 48 kHz; frees equalled allocs in every case; the Debug tree gave the identical numbers). Failing run summary line: `test cases: 1 | 0 passed | 1 failed`, `assertions: 27 | 15 passed | 12 failed`.

  | Format | Order | Channels | Allocations at BASE | After fix |
  |--------|-------|----------|---------------------|-----------|
  | AmbisonicsFOA | 1 | 4 | 1,427 | 0 |
  | AmbisonicsSOA | 2 | 9 | 2,854 | 0 |
  | AmbisonicsHOA | 3 | 16 | 4,281 | 0 |
  | Ambisonics4OA | 4 | 25 | 5,708 | 0 |
  | Ambisonics5OA | 5 | 36 | 7,135 | 0 |
  | Ambisonics6OA | 6 | 49 | 8,562 | 0 |

  These are 11, 22, 33, 44, 55 and 66 higher than the research's 1,416 to 8,496 (the research probe used RelWithDebInfo with a slightly different scene); the scaling by order (one more filter per object per order) is the same.
- **First-offender stack at BASE** (identical in Debug and Release): `mallocHook` <- `_malloc_zone_malloc_instrumented_or_legacy` <- `juce::dsp::IIR::Coefficients<float>::assignImpl<4>` <- `Coefficients<float>::Coefficients(float, float, float, float)` <- `RenderEngine::renderAmbisonicsOutput` <- `RenderEngine::renderBlock` <- `armedRender`. No Debug-only first offender appeared: the tracer and the six-format case count the same in Debug and Release.
- **Four capture file sizes** (6 formats, 139 channels total, 120 blocks, 256 samples, 4 bytes = 139 x 120 x 1024), all 17,080,320 bytes:
  - `/tmp/sc-p5-nfc-before-debug.bin`, `/tmp/sc-p5-nfc-before-release.bin`
  - `/tmp/sc-p5-nfc-after-debug.bin`, `/tmp/sc-p5-nfc-after-release.bin`
- **cmp results:** `cmp before-debug after-debug` exit 0; `cmp before-release after-release` exit 0; after-release is non-empty. The captured data is real audio (4,270,080 floats, 4,269,479 nonzero, peak 2.847, no NaN), so the equality is not vacuous. (Debug and Release captures differ from each other, as expected between optimisation levels; that is not part of the criterion.)
- **Coefficient identity:** `[nfc][d10]` passes, 325 assertions, all 3 x 6 x 9 = 162 combinations equal in size and `memcmp` bits.

## Task Commits

1. **Task 1: Tracer, trap proven to see, cold-start moving 7.1.4 reports zero** - `b80981b` (feat)
2. **Task 2 RED: failing Ambisonics allocation case, NFC identity case, hidden capture** - `19de48f` (test)
3. **Task 2 GREEN: NFC coefficients assigned in place** - `5d8e724` (fix)

**Plan metadata:** recorded in the docs commit that follows this file (`docs(05-01): complete realtime trap and NFC fix plan`).

Tracer feedback gate: auto mode not active, `human_verify_mode` is the default end-of-phase, and the tracer `<verify>` carries only `<automated>`, so the gate was a re-run of the tracer verify end to end (Debug and Release). It passed, and expansion (Task 2) went ahead.

## Files Created/Modified

- `tests/Support/rtguard/RTGuard.cpp` - shared library: `malloc_logger` hook, `__DATA,__interpose` tuples for five pthread entry points, `extern "C"` `scrt_*` functions, first-offender backtrace
- `tests/Support/RealtimeGuard.h` - `RealtimeCounts`, `realtimeTrapAvailable`, `skipUnlessRealtimeTrap`, `ScopedRealtimeTrap`, `requireRealtimeTrapCanary`; no-ops when the trap is not linked
- `tests/Engine/RealtimeSafetyTests.cpp` - canary, tracer, six-format Ambisonics case, `[nfc][d10]` identity case, hidden capture; `MovingScene`, `OutputBank`, `armedRender`, `makeEngineContext`
- `tests/CMakeLists.txt` - `SpatialCoreRTGuard` target under `if (APPLE AND NOT CMAKE_CXX_FLAGS MATCHES "-fsanitize")`, `SPATIALCORE_RT_GUARD=1` and `SPATIALCORE_SOURCE_DIR` on `SpatialCoreTests`, new source in the list (14 added lines, CRLF kept: `git ls-files --eol` still `w/crlf`)
- `src/Engine/RenderEngine.cpp` - `renderAmbisonicsOutput` NFC update assigns `std::array<float, 4>` in place, comment made true, `#include <array>` added (12-line diff, nothing else touched)

## Decisions Made

- Followed the plan: libmalloc hook plus dylib interposer, canary-first, APPLE-and-no-sanitizer gating, in-place `std::array` assignment (rather than writing raw coefficients).
- Added the explicit `#include <array>` even though `RenderEngine.h` already includes it, so the .cpp does not rely on a transitive include.

## Deviations from Plan

None - plan executed exactly as written. Two small notes that are not deviations:

- The comment in `RTGuard.cpp` first said "no thread_local", which made the acceptance grep (`grep -c thread_local` must print 0) print 1; the comment was reworded before the first commit.
- The plan's Task 2 verify expects a Debug exit note of "Leaked objects detected: 1 FFT"; this run printed "3 instance(s) of class FFT" at exit of the Debug full and UI runs (exit code 0 both). This is the same process-lifetime `SharedFFTCache` noise (D-08); the new cases do not touch FFT code.

## Issues Encountered

None. (The stderr of the red runs contains the first-offender stack by design.)

## Known Stubs

None.

## Threat Flags

None. The new surface (a malloc hook and pthread interposer) is exactly threat T-05-01 in the plan, mitigated: `git grep -l -e malloc_logger -e __interpose -- src include CMakeLists.txt` prints nothing, and the target is linked only into `SpatialCoreTests`.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plans 05-02 and 05-03 can reuse `MovingScene`, `OutputBank`, `armedRender`, `makeEngineContext` and the trap. Those helpers are in the anonymous namespace of `RealtimeSafetyTests.cpp`; a later plan that needs them from another file will have to move them to a shared header.
- `build/` (Debug, with `compile_commands.json`) and `build-release/` are configured and built; both are gitignored.
- Known limit, by design: the trap does not see `juce::SpinLock` or other pure atomic spins; Plan 05-03's source-scan rule covers that.

## Self-Check: PASSED

- `tests/Support/rtguard/RTGuard.cpp`, `tests/Support/RealtimeGuard.h`, `tests/Engine/RealtimeSafetyTests.cpp`: FOUND
- Commits `b80981b`, `19de48f`, `5d8e724`: FOUND; `git log --all --grep="05-01"` returns 3 commits
- Plan-level verification re-run: canary and tracer pass in Debug and Release (2 cases, none skipped); `[rt][ambi]` red at BASE (counts above) and green after the fix; `[nfc][d10]` passes; both `cmp` exit 0; full Debug suite (329 cases), Debug UI suite (14 cases) and Release ctest (343 of 343) pass; `ctest -N` lists 14 `ui:` tests.
- Acceptance greps: `__interpose` 3, `thread_local` 0; `SpatialCoreRTGuard` 2, `SPATIALCORE_RT_GUARD=1` 1, `SPATIALCORE_SOURCE_DIR` 2; `tests/CMakeLists.txt` still `w/crlf`; shipped-source grep empty; the `std::array<float, 4> { b0 / a0, b1 / a0, 1.0f, a1 / a0 }` line count is 1; `git diff BASE -- src/Engine/RenderEngine.cpp` touches only `renderAmbisonicsOutput` plus the include.

---
*Phase: 05-realtime-safety-hardening*
*Completed: 2026-10-06*
