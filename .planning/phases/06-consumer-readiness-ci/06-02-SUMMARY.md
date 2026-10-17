---
phase: 06-consumer-readiness-ci
plan: 02
subsystem: engine
tags: [render-engine, diagnostics, intg-02, catch2, realtime-safety, integration-guide]

requires:
  - phase: 06-consumer-readiness-ci
    provides: "plan 06-01: configured build/ and build-release/ trees, root-level ctest"
provides:
  - "RenderEngine::getUnsuppliedGainsBlockCount(): any-thread, lock-free count of blocks that render a gain-reading path on zero consumer gains"
  - "Path-aware detector (blockHasUnsuppliedGains) in renderBlock, Debug assertion once per engine, Release counts only"
  - "tests/Engine/UnsuppliedGainsTests.cpp: [intg02] cases for every path, flag and edge, plus the [rt][intg02] trap case"
  - "docs/integration-guide.md 'Set these flags' block (D-07)"
affects: [06-03, 06-04-harness, 06-09]

plan_head_before: f38c9b24ce5369de43170e1696a8030ba2e694e0
plan_head_after: 0308d8a4b69c310fb89cea5491da2fb2ea36b0ca

actuals:
  tokens: 8100
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns:
    - "Count then assert-once diagnostic: relaxed fetch_add, jassertfalse only when the previous value was 0"
    - "Misuse detector reads the finished per-block context (after derive, compute and the engineSelectsHRTF fill), never the consumer's raw fields"

key-files:
  created:
    - tests/Engine/UnsuppliedGainsTests.cpp
  modified:
    - include/SpatialCore/Engine/RenderEngine.h
    - src/Engine/RenderEngine.cpp
    - tests/CMakeLists.txt
    - docs/integration-guide.md

key-decisions:
  - "Detection rule: count when engineComputesGains is false, the rendered path reads consumer gains, at least one object is active, and every active object's gains on that path are exactly 0"
  - "The detector only counts and (Debug) asserts; it never computes or substitutes gains (D-06)"
  - "On discrete surround the LFE channel is generated from the mono sum, so a misuse block is silent on the speaker channels but not on the LFE channel"

requirements-completed: [INTG-02]

coverage:
  - id: D1
    description: "A stereo block with live objects and all-zero gains is counted by a lock-free counter and renders exact silence; the same block with the flag on or with gains supplied is not counted"
    requirement: INTG-02
    verification:
      - kind: unit
        ref: "tests/Engine/UnsuppliedGainsTests.cpp#INTG-02: a stereo block with live objects and no gains is counted, and nothing fills the gains"
        status: pass
    human_judgment: false
  - id: D2
    description: "Detector is path-aware: discrete surround, Simple binaural and stereo variants count zero gains; HRTF path, Ambisonics, the engineSelectsHRTF fill, engineComputesGains and supplied gains count 0; the derived dispatch is judged, not stale consumer fields"
    requirement: INTG-02
    verification:
      - kind: unit
        ref: "./build-release/tests/SpatialCoreTests \"[intg02]\" (7 cases, 45 assertions)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Edges of 'active': tap fade 0 at both ends is inactive, above 0 at either end is active, one supplied object beside zero ones is not counted, no active object and a zero-length block are never counted"
    requirement: INTG-02
    verification:
      - kind: unit
        ref: "tests/Engine/UnsuppliedGainsTests.cpp#INTG-02: edges of 'active' and of the block"
        status: pass
    human_judgment: false
  - id: D4
    description: "A misuse block allocates, frees and locks nothing on the render thread in a Release build on macOS (DR-1)"
    requirement: INTG-02
    verification:
      - kind: unit
        ref: "./build-release/tests/SpatialCoreTests \"[rt][intg02]\" (1 case, 7 assertions, 0 allocs, 0 frees, 0 locks over 120 blocks)"
        status: pass
    human_judgment: false
  - id: D5
    description: "No existing test trips the new assertion (false-positive audit), and no golden or tolerance changed"
    requirement: INTG-02
    verification:
      - kind: integration
        ref: "ctest --test-dir build/tests -j8 -V (Debug, 413 tests) searched for RenderEngine.cpp:847; git diff f38c9b2 --stat -- tests"
        status: pass
    human_judgment: false
  - id: D6
    description: "docs/integration-guide.md has one 'Set these flags' block naming the recommended flags, the one gains-mode choice, and getUnsuppliedGainsBlockCount() as the consumer's own check"
    requirement: INTG-02
    verification: []
    human_judgment: true
    rationale: "Whether the guide reads clearly to a migrating consumer is a judgment; the grep acceptance checks (heading count, names present, LF endings) pass but cannot assert clarity"

duration: 20min
completed: 2026-10-17
status: complete
---

# Phase 6 Plan 02: Unsupplied-gains detector (INTG-02) Summary

**`RenderEngine::getUnsuppliedGainsBlockCount()` counts every block that renders a gain-reading path (Simple binaural, stereo variant, discrete surround) while gains are neither engine-computed nor consumer-supplied, asserts once per engine in Debug, allocates and locks nothing in Release, and the integration guide gains a "Set these flags" block.**

## Performance

- **Duration:** about 20 min wall clock
- **Started:** 2026-10-17T15:31Z (approx.)
- **Completed:** 2026-10-17T15:52Z
- **Tasks:** 3
- **Files modified:** 5 (1 created, 4 modified)
- **BASE:** `f38c9b24ce5369de43170e1696a8030ba2e694e0`

## Accomplishments

- Closed the residual SpatialCore#14 trap without changing anyone's sound. Defaults are unchanged (`engineComputesGains` still `false`); the engine never fills gains for the consumer.
- The detector sits right after `const RenderBlockContext& ctx = *dispatchCtx;`, so it judges the finished context: with `engineDerivesDispatch` it judges the path the engine renders, and the `engineSelectsHRTF` objGains fill (WR-06) is already applied.
- Seven `[intg02]` cases (45 assertions) cover every path, flag and edge from the plan. A mutation check (removing the HRTF and Ambisonics guard from the detector) made 2 of the 7 fail, so the negative cases bite.
- The integration guide has one "Set these flags" block (recommended set, the single gains-mode choice, the counter as the consumer's own test check).

## TDD record

**RED (commit `add8122`).** Accessor and member stubbed so the test compiles, no detector. `./build-release/tests/SpatialCoreTests "[intg02]"` exit 42:

```
UnsuppliedGainsTests.cpp:151: FAILED:
  CHECK( rig.engine.getUnsuppliedGainsBlockCount() == 4 )
with expansion:
  0 == 4
test cases: 1 | 1 failed      assertions: 7 | 6 passed | 1 failed
```

Semantic assessment: the target test executed (7 assertions ran), failed on exactly the planned assertion (the count of misuse blocks), for the intended reason (no detector exists yet). The other six assertions (fresh engine 0, flag-on 0, supplied-gains 0, signal present) passed, as they should before the detector. Not an INVALID_RED.

**GREEN (commit `3f88507`).** Detector added; `[intg02]` 7 assertions pass in Release and Debug; `"[engine],[sc16],[sc17],[hrtf-switch]"` in Release: 3040 assertions in 72 cases, all passed.

**Task 2 (commit `a332606`).** The detector already existed, so the added cases pass on first run (no separate RED, they pin behaviour of the shipped detector). Their ability to fail was checked by the mutation above.

**REFACTOR.** None needed.

### TDD Gate Compliance

RED `add8122` (`test(06-02)`) precedes GREEN `3f88507` (`feat(06-02)`). No REFACTOR commit. The plan is `type: execute` with tdd tasks, so gate compliance is advisory.

## `[rt][intg02]` counts

`./build-release/tests/SpatialCoreTests "[rt][intg02]"` (Release, macOS, canary passed first): 1 test case, 7 assertions, none skipped, exit 0. 120 misuse blocks, trap armed around exactly one `renderBlock` each: allocs 0, frees 0, locks 0, and `getUnsuppliedGainsBlockCount() == 120`. In the Debug tree the case SKIPs by design (the assertion logs).

`"[rt]"` in Release: 2521 assertions in 47 cases, all passed.

## False-positive audit

Debug `ctest --test-dir build/tests -j8 -V` (413 tests, 100% passed, 1 skipped by design) was searched for `JUCE Assertion failure in RenderEngine.cpp:847` (the new `jassertfalse`). 9 hits, all from this plan's own new tests:

| ctest # | Test | Hits | Label |
|---|---|---|---|
| 390 | INTG-02: a stereo block with live objects and no gains is counted... | 1 | deliberate |
| 391 | INTG-02: discrete surround counts zero objChannelGains... | 2 | deliberate |
| 392 | INTG-02: the Simple binaural path counts zero objGains... | 1 | deliberate |
| 394 | INTG-02: the detector judges the path the engine renders... | 1 | deliberate |
| 395 | INTG-02: edges of 'active' and of the block | 4 | deliberate |

No pre-existing test trips the assertion (including `ProfileSwitchTests.cpp:1546`, the WR-06 zero-objGains case, which counts 0). Remaining false positives: 0. The multiple hits per test come from Catch2 SECTIONs re-running the test body with a fresh engine each time (the assertion fires once per engine).

## Full gate

- Debug `./build/tests/SpatialCoreTests`: exit 0, 396 test cases (395 passed, 1 skipped: the `[rt][intg02]` case by design), 329072 assertions passed. The process-lifetime `FFT` leak report at exit is the same expected noise as in plan 06-01.
- Debug `./build/tests/SpatialCoreUITests`: exit 0, 17 cases, 152 assertions.
- Debug ctest: 413/413 passed (1 skipped by design).
- Release `ctest --test-dir build-release --no-tests=error --output-on-failure`: 413/413 passed; `ui:` listing count 17.
- No golden or tolerance changed: `git diff f38c9b2 --stat -- tests` shows only `tests/Engine/UnsuppliedGainsTests.cpp` (+530) and `tests/CMakeLists.txt` (+1, still `w/crlf`).

## Acceptance criteria (re-run)

- `grep -c "getUnsuppliedGainsBlockCount" include/SpatialCore/Engine/RenderEngine.h`: 1
- `grep -c "unsuppliedGainsBlockCount_.fetch_add (1, std::memory_order_relaxed)" src/Engine/RenderEngine.cpp`: 1
- `grep -c "bool engineComputesGains = false;" include/SpatialCore/Engine/RenderEngine.h`: 1
- `git diff BASE -- src/Engine/RenderEngine.cpp` exit 0; added lines matching `DBG|Logger|std::vector|resize|ScopedLock|std::mutex|push_back`: 0
- `git ls-files --eol tests/CMakeLists.txt`: `w/crlf`; `docs/integration-guide.md`: `w/lf`
- `grep -c TEST_CASE tests/Engine/UnsuppliedGainsTests.cpp`: 7 (at least 6 required)
- Guide: `grep -c "^### Set these flags"` = 1; names all four flags, `setHRTFProfile`, `setAlgorithmIndex`, `getBlockLayout`, `getUnsuppliedGainsBlockCount`; states both modes and that defaults are `false`; `git diff BASE -- docs/integration-guide.md` is 25 insertions and 1 deletion (the "Let the engine compute gains" bullet).

## Task Commits

1. **Task 1 (tracer), RED:** `add8122` (test) - failing test for the counter
2. **Task 1 (tracer), GREEN:** `3f88507` (feat) - detector and accessor
3. **Task 2:** `a332606` (test) - every path and edge, plus the `[rt][intg02]` trap case
4. **Task 3:** `0308d8a` (docs) - "Set these flags" block

**Plan metadata:** committed with this SUMMARY (docs).

## Files Created/Modified

- `include/SpatialCore/Engine/RenderEngine.h` - public `getUnsuppliedGainsBlockCount()` with doc comment, `unsuppliedGainsBlockCount_` member, private `blockHasUnsuppliedGains()` declaration
- `src/Engine/RenderEngine.cpp` - the detector, the check after the per-block context is final, the accessor
- `tests/Engine/UnsuppliedGainsTests.cpp` - `GainsRig` and the `[intg02]` / `[rt][intg02]` cases
- `tests/CMakeLists.txt` - one source-list line
- `docs/integration-guide.md` - the D-07 block and one reworded bullet

## Decisions Made

- Followed the plan's four-part detection rule exactly. Gains for slots beyond the layout's speakers and gains on slots that are not live do not clear the count (both pinned by tests), because the render path never reads them.
- The detector bounds `numSpeakers` with `std::min (..., MAX_SPEAKERS)` before indexing, so a malformed layout cannot read past the gain array.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Test asserted exact silence on every discrete-surround output channel**
- **Found during:** Task 2
- **Issue:** the first version of the surround misuse case CHECKed all output channels exactly 0. The LFE channel is generated from the mono sum (low-pass, -10 dB), not from the per-speaker gains, so it carries signal on a misuse block. The test was wrong, not the engine.
- **Fix:** added `speakerChannelsExactlySilent()` to the rig, which checks only the layout's speaker channels, and used it for the surround and Quad rows. The stereo and Simple-binaural rows keep the whole-bank check.
- **Files modified:** `tests/Engine/UnsuppliedGainsTests.cpp`
- **Verification:** `[intg02]` passes (45 assertions), Release and Debug.
- **Committed in:** `a332606`

**2. [Rule 1 - Bug] Acceptance criterion `grep -c "getUnsuppliedGainsBlockCount"` printed 2**
- **Found during:** Task 1
- **Issue:** the comment on the private `blockHasUnsuppliedGains` helper named the public method, so the expected count of 1 read 2.
- **Fix:** reworded that comment to "the public unsupplied-gains diagnostic" and amended the (unpushed) GREEN commit, so `3f88507` already carries the fix.
- **Files modified:** `include/SpatialCore/Engine/RenderEngine.h`
- **Verification:** count prints 1.
- **Committed in:** `3f88507`

---

**Total deviations:** 2 auto-fixed (2 bugs in my own test and comment). **Impact on plan:** none; no library behaviour changed.

## Issues Encountered

- The plan's Task 2 `<automated>` checks list the Debug `SpatialCoreTests` run; it was run directly and also through `ctest -V` (both green).
- The per-commit branch rule for worktrees expects an `agent-*` branch. This run is the sequential executor on the user's branch `AndrewRahman/gsd-discuss-phase-6`, as the orchestrator directed, so only the protected-branch check applied.

## Known Stubs

None.

## Threat Flags

None. The detector adds no network, auth or file-access surface. T-06-03 (mitigated: bounded loops, one relaxed atomic add, `[rt][intg02]` 0/0/0), T-06-04 (mitigated: counter, Debug assertion, guide instruction) and T-06-05 (mitigated: the tracer CHECKs the misuse output is exactly 0.0f, no gain fill) are covered.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 06-04's harness can assert `getUnsuppliedGainsBlockCount() == 0` after rendering each of its configurations, as the guide now tells consumers to.
- `CLAUDE.md` does not yet mention `getUnsuppliedGainsBlockCount()` among the `RenderEngine` diagnostics; it was outside this plan's file list, so it is left for a doc-sync pass.
- `build/` and `build-release/` are built and green at `0308d8a`.

## Self-Check: PASSED

- Created file exists: `tests/Engine/UnsuppliedGainsTests.cpp`.
- Commits `add8122`, `3f88507`, `a332606`, `0308d8a` are ancestors of HEAD (verified with `git merge-base --is-ancestor`).
- All acceptance criteria re-run (counts above).

---
*Phase: 06-consumer-readiness-ci*
*Completed: 2026-10-17*
