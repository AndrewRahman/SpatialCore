---
phase: 03-binaural-defects-hrtf-packaging
plan: 06
subsystem: engine
tags: [hrtf, render-engine, loader-thread, atomic-mailbox, profile-switching, juce-thread, catch2]

requires:
  - phase: 03-binaural-defects-hrtf-packaging
    provides: "03-05 resolveHRTFProfile, getSharedHRTFFolder, HRTFProfileStatus, describeHRTFProfileStatus, embedded profiles; 03-03 BinauralRenderer::setProfile scratch sizing; 03-04 Simple-path cue bank"
provides:
  - "RenderEngine::setHRTFProfile (int): one call, returns at once, latest request wins"
  - "RenderEngine::getHRTFProfileStatus (lock-free), waitForHRTFProfileIdle, setSharedHRTFFolderForTesting"
  - "HRTFProfileLoader worker thread (lazy, low priority) and a one-slot atomic mailbox the audio thread claims at the top of a block"
  - "prepare() quiesce: stops the worker, discards an unclaimed result, reloads at new sample rate or block size"
  - "Umbrella includes for HRTFProfile.h, HRTFProfileResolver.h, SimpleBinauralCues.h"
affects: [03-07 thread proof and shutdown, 03-09 Simple <-> HRTF crossfade and sample-based fade, OpenSpatialDelay and OpenSpatialPanner glue removal]

plan_head_before: de6d7d7ae839593e6b20371d42622cb37a838184
plan_head_after: 11fba8aa6d79c510bee72292c063cf42fcf40acb

actuals:
  tokens: 11500
  tasks: 2
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Single-writer worker thread owned by the engine, started lazily on the first request; an engine that never gets a request starts no thread"
    - "Renderer ownership by atomics: free flag per renderer (audio thread release-stores true, worker exchanges to false to claim) plus a one-slot mailbox (worker release-store, audio acquire-exchange)"
    - "Status packed into one 32-bit atomic word, text built on the reader's thread from the static table"

key-files:
  created: []
  modified:
    - include/SpatialCore/Engine/RenderEngine.h
    - src/Engine/RenderEngine.cpp
    - tests/Engine/ProfileSwitchTests.cpp
    - include/SpatialCore/SpatialCore.h

key-decisions:
  - "The worker decides skip-or-load from its own belief of what is playing (derived from a per-renderer meta record and the mailbox outcome), never from an audio-thread-published value, so a claim racing the decision cannot mislabel the active profile"
  - "A request that finds an older unclaimed result in the mailbox always takes it back first, so a stale result is never claimed after a newer request failed or turned out to be the active profile"
  - "The path-change crossfade abort in claimReadyRenderer only runs once engine-owned switching has been used (requestSerial_ != 0), so a legacy escape-hatch consumer keeps its crossfade state exactly as before"
  - "prepare() resets the crossfade bookkeeping only when a request was ever made, for the same reason"
  - "Loading status reports source None and problem None; Ready, Failed and Idle report the worker's last word"
  - "Tests that need a specific embedded profile use a probe profile (3 with all five embedded, 5 in the KEMAR-only build), so every case runs in both embedding modes"

patterns-established:
  - "Engine-owned switching tests drive a LiveRender helper block by block and interleave requests with rendering, like a host"
  - "Profile 0 on the HRTF path is a silent Simple-mode renderer; tests do not expect sound there (Plan 03-09 owns the Simple <-> HRTF crossfade)"

requirements-completed: [DATA-01]

coverage:
  - id: D1
    description: "setHRTFProfile switches the profile in one call: the SOFA load runs on a worker thread, the call returns in under 50 ms, the audio thread claims the result from an atomic mailbox and renders it"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: one call loads KEMAR from embedded data and the audio thread plays it"
        status: pass
    human_judgment: false
  - id: D2
    description: "Most recent request wins when requests arrive faster than loads finish, with no audio rendering in between; a settled request is a no-op and a failed one retries"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: the most recent request wins, and a settled request is a no-op"
        status: pass
    human_judgment: false
  - id: D3
    description: "A failed load (invalid index, nothing found) keeps the current profile playing and reports Failed with a readable text; no mute"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: a failed request leaves the current profile playing and reports why"
        status: pass
    human_judgment: false
  - id: D4
    description: "A broken same-name shared file plays the built-in copy and reports it (D-09); a file dropped into the shared folder is used on the next switch with no restart (D-11)"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: a broken same-name shared file plays the built-in copy and says so"
        status: pass
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: a file dropped into the shared folder is used on the next switch, no restart"
        status: pass
    human_judgment: false
  - id: D5
    description: "Criterion 3 through the engine: with no shared folder and no path, profiles 1-5 each become active from embedded data and render finite non-silent output; profile 0 makes the Simple renderer active"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: every embedded profile becomes active through the engine with no shared folder"
        status: pass
    human_judgment: false
  - id: D6
    description: "prepare() during a load is safe and reloads at the new sample rate; a second prepare at the same settings reloads nothing"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: prepare() during a load is safe and reloads at the new sample rate"
        status: pass
    human_judgment: false
  - id: D7
    description: "A non-binaural format or a path change mid-crossfade never strands the loader: the claim switches at once or ends the fade and frees the renderer, and the next request completes"
    requirement: DATA-01
    verification:
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: switching while a non-binaural format renders completes and strands nothing"
        status: pass
      - kind: unit
        ref: "tests/Engine/ProfileSwitchTests.cpp#Profile switch: a crossfade abandoned by a path change frees its renderer"
        status: pass
    human_judgment: false
  - id: D8
    description: "Audio-thread safety of the mailbox and free-flag protocol under real concurrency"
    requirement: DATA-01
    verification: []
    human_judgment: true
    rationale: "The proof under a live render thread plus a switching thread, shutdown mid-load and a ThreadSanitizer run is Plan 03-07; this plan proves the rules single-threaded through the engine and by design review of the atomics only"

duration: 36 min
completed: 2026-10-04
status: complete
---

# Phase 3 Plan 06: Engine-owned profile switching Summary

**RenderEngine::setHRTFProfile switches the HRTF profile in one call: a lazily started worker thread resolves and loads it into the idle renderer, and the audio thread claims it from a one-slot atomic mailbox at a block boundary and crossfades with the existing machinery.**

## What you will hear

Nothing changes in the default sound, and an existing plugin that never calls the new function behaves exactly as before. What is new is that a plugin can now say "use profile 3" in one line and keep playing. The old profile keeps sounding while the new file loads in the background, then the two crossfade. If the request cannot be met (a profile number that does not exist, or a file that is nowhere to be found), the sound simply stays on what it was and a readable status such as "profile 9 failed: no such profile" is available for the plugin to show. A broken file in the shared folder falls back to the built-in copy and the status says "unreadable, used built-in". A file dropped into the shared folder is used on the next switch with no restart. The two plugins' own load-and-swap code can now be deleted in a later step.

## Performance

- **Duration:** 36 min
- **Started:** 2026-10-04T13:53:00Z
- **Completed:** 2026-10-04T14:29:00Z
- **Tasks:** 2 (Task 1 tracer, Task 2 rules)
- **Files modified:** 4

## Accomplishments

- `setHRTFProfile (int)`, `getHRTFProfileStatus() const noexcept`, `waitForHRTFProfileIdle (int)`, `setSharedHRTFFolderForTesting (const juce::File&)` on `RenderEngine`, with the thread contract written in the header. No existing public member changed or was removed (the diff against the plan base has no removed line in the header).
- `HRTFProfileLoader`: a nested `juce::Thread`, started on the first request after `prepare()` at low priority. It claims a free renderer (or takes back its own unclaimed result), `resolveHRTFProfile`, `invalidateSources`, `setProfile`, then a release store into the mailbox. Invalid indexes are answered before any renderer is touched.
- `claimReadyRenderer` on the audio thread: atomic loads, stores and exchanges only. On the HRTF path the existing swap detection runs the crossfade and frees the old renderer where it ends; on any other path the switch is instant and a stranded crossfade is ended and freed.
- `prepare()` stops the worker, discards an unclaimed result, recomputes the free flags, and when the sample rate or maximum block size changed it forces a reload of the requested profile and resizes the active renderer's convolvers for the new block size.
- Status word packing (`packHRTFStatus` / `unpackHRTFStatus`), lock-free reads from any thread.
- 9 new test cases; `[hrtf-switch]` lists 9 (tracer, failure, latest-wins, prepare, status, d11, non-binaural, abandon, legacy click) and `[hrtf-resolve][embedded]` lists 2.

## Task Commits

1. **Task 1: Tracer, setHRTFProfile (5) from the message thread, worker resolves embedded KEMAR, audio thread claims it** - `5fc763c` (feat; the red test was written first and did not compile until the API existed)
2. **Task 2: Every rule at engine level** - `11fba8a` (test)

Task 2 was written test-first, but the Task 1 implementation already covered every rule, so all eight cases passed on their first run. No RED commit exists for Task 2 for that reason; the only change after the first run was a test fix for the KEMAR-only build (below).

**Plan metadata:** the docs commit following this SUMMARY.

Tracer gate: interactive run, `end-of-phase` mode, tracer `<verify>` automated-only; the verify (build, `[hrtf-switch][tracer]`, `[engine]`, `[hrtf-switch][click][legacy]`) was re-run end to end and passed, so execution continued to Task 2.

## Verification results

| Check | Result |
|---|---|
| Task 1 verify (Debug): `[hrtf-switch][tracer]`, `[engine]`, `[hrtf-switch][click][legacy]` | pass |
| `[hrtf-switch],[hrtf-resolve][embedded]` Debug, 6 consecutive runs | 11 cases, 195 assertions, all 6 runs pass |
| Full Debug suite | 250 of 250 test cases pass (309,042 assertions) |
| Release ctest (`build-release`) | 100% passed, 250 of 250 |
| KEMAR-only build (`/tmp/sc-kemar-only`, OFF) `[hrtf-switch],[hrtf-resolve][embedded]` | 11 cases, 163 assertions pass |
| Consumer build (`/tmp/osd-dr3-check`, OSD `OpenSpatialDelay` and `OpenSpatialDelayTests`) | both build |
| Acceptance greps | API names 10 (need 4); `HRTFProfileResolver.h` 1; `SimpleBinauralCues.h` 1; no removed public declaration |

The known exit-time "Leaked objects detected ... class FFT" message appears as before and is not from this plan.

## Files Created/Modified

- `include/SpatialCore/Engine/RenderEngine.h` - the four public calls with the thread contract, the loader forward declaration, the mailbox / free-flag / status atomics
- `src/Engine/RenderEngine.cpp` - the loader thread, the message-thread calls, `claimReadyRenderer`, the `prepare()` quiesce and reload, the status packing, the crossfade-end free and active-profile publish
- `tests/Engine/ProfileSwitchTests.cpp` - `LiveRender` driver, temp-folder fixture and the nine engine-level cases
- `include/SpatialCore/SpatialCore.h` - umbrella includes for the three headers

## Decisions Made

See `key-decisions` in the frontmatter. The main one: the worker never trusts an audio-thread-published value to decide skip-or-load. It keeps its own belief about what is playing, which it updates from the outcome of its mailbox exchange (an empty mailbox after a publish means the audio thread claimed it) and from a per-renderer record of what each renderer was loaded from. That closes a window where a claim lands between the worker reading "active" and acting on it.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Test helper expected sound from profile 0 on the HRTF path**
- **Found during:** Task 2 (KEMAR-only build run, `[hrtf-switch][d11]`)
- **Issue:** In a build that embeds only KEMAR the "other profile" the test switches to is 0, and a Simple-mode renderer on the HRTF path is silent by design, so the shared `switchAndSettle` helper's non-silent check failed.
- **Fix:** For profile 0 the helper renders 50 blocks and checks only finiteness and that the crossfade ended.
- **Files modified:** `tests/Engine/ProfileSwitchTests.cpp`
- **Verification:** KEMAR-only build 11/11, default build 11/11
- **Committed in:** `11fba8a`

**2. [Rule 2 - Missing critical] Legacy behaviour kept byte-for-byte when switching is unused**
- **Found during:** Task 1 (design of `claimReadyRenderer` and `prepare()`)
- **Issue:** The plan's unconditional "end a stranded crossfade on a non-HRTF path" and "reset the crossfade state in prepare()" would have changed the behaviour of an existing consumer that never calls `setHRTFProfile`, against the plan's own "byte-for-byte as before" truth.
- **Fix:** Both run only after the first `setHRTFProfile` call (`requestSerial_ != 0` on the audio side, `hrtfEverRequested_` in `prepare()`).
- **Files modified:** `src/Engine/RenderEngine.cpp`
- **Verification:** `[engine]` and `[hrtf-switch][click][legacy]` unchanged and passing, OSD consumer build
- **Committed in:** `5fc763c`

**3. [Rule 2 - Missing critical] Active renderer's convolvers resized when `prepare()` changes the block size**
- **Found during:** Task 1 (design of the prepare reload)
- **Issue:** Between a `prepare()` at a new rate or block size and the claim of the reloaded copy (seconds, for a large file) the old active renderer would keep rendering with convolvers sized for the old block size.
- **Fix:** In that case `prepare()` calls `setProfile` on the active renderer so its scratch and convolvers match the new block size until the new copy takes over.
- **Files modified:** `src/Engine/RenderEngine.cpp`
- **Verification:** `[hrtf-switch][prepare]`
- **Committed in:** `5fc763c`

---

**Total deviations:** 3 auto-fixed (1 bug in a test helper, 2 missing critical)
**Impact on plan:** No scope creep; the two engine changes narrow the plan's behaviour so existing consumers are untouched and a mid-reload renderer stays in spec.

## Issues Encountered

- The first Task 2 build failed on a `constexpr` that used `std::vector::size()`; replaced with a `constexpr bool` set by the embedding macro. No effect on behaviour.
- The retry of a failed request cannot be observed beyond "Loading or Failed straight after the call, Failed again once idle" through the public API, since the serial is private. The plan's wording allows exactly that; no test-only accessor was added.

## Known Stubs

None.

## Threat Flags

None. The new surface (a worker thread, an atomic mailbox) is the one the plan's threat model covers (T-03-14 to T-03-17). No new file access beyond reading the shared folder through the existing resolver.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 03-07 can now prove the protocol under a real render thread plus a switching thread, shutdown mid-load, and a ThreadSanitizer run (`[hrtf-switch][threads]`, `[hrtf-switch][shutdown]`).
- Plan 03-09 owns the Simple <-> HRTF crossfade and the sample-based fade length; profile 0 requested on the HRTF path is currently a silent Simple-mode renderer after the existing 8-block fade.
- The data-race freedom of the mailbox and free-flag protocol rests on design review and single-threaded tests until 03-07 runs.

## Self-Check: PASSED

---
*Phase: 03-binaural-defects-hrtf-packaging*
*Completed: 2026-10-04*
