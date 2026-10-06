---
phase: 05-realtime-safety-hardening
plan: 08
subsystem: docs-verification
tags: [realtime-safety, threadsanitizer, readme, claude-md, integration-guide, requirements, verification-evidence]

requires:
  - phase: 05-realtime-safety-hardening
    provides: "Plans 05-01 to 05-07: the [rt] trap, oversized-block and scratch guards, the whole-path matrix, build-tsan and the atomic trajectory finals, the legacy-route watch, the multi-instance and multi-map proofs"
provides:
  - "README 'Realtime-safety check (every phase end and before a release tag)': the [rt] pass, the build-tsan configure and build, the [tsan] subset, the UI suite and the whole SpatialCoreTests suite under ThreadSanitizer, with measured run times and the macOS-only scope (D-01, D-13)"
  - "CLAUDE.md, docs/integration-guide.md and the spatialcore-architecture skill describing what Phase 5 shipped, with no sentence deferring RTSF-01 to Phase 5"
  - "PROJECT.md 'OSD Phase 5 follow-ups' row for the OSD migration"
  - "RTSF-01, RTSF-02, RTSF-03, RTSF-05 and VERIFY-02 ticked and Complete against the VERIFICATION evidence section below"
affects: [phase-5-verification, phase-6-ci, osd-migration, release-tagging]

actuals:
  tokens: TOKENS_PENDING
  tasks: 3
  commits: COMMITS_PENDING

tech-stack:
  added: []
  patterns:
    - "The realtime-safety check is a separate phase-end and pre-release check; the everyday Debug and Release gate is unchanged"
    - "A check passes only when its exit status is 0 and its 'WARNING: ThreadSanitizer' count is 0; the whole-suite log ends with an 'exit=<status>' line written by the same command line"

key-files:
  created: []
  modified:
    - README.md
    - CLAUDE.md
    - docs/integration-guide.md
    - .claude/skills/spatialcore-architecture/spatialcore-architecture.md
    - .planning/PROJECT.md
    - .planning/REQUIREMENTS.md
    - .planning/STATE.md
    - examples/demo/DemoComponent.h

key-decisions:
  - "The README check is five numbered steps in the order the plan fixed; the tsan configure command is written out with all four -fsanitize=thread settings and the optional FETCHCONTENT note"
  - "Run times are recorded as measured in this run, each step alone: [rt] about 20 s, [tsan] about 17 min, UI under TSan about 45 s, whole suite about 32 min"
  - "The proof's scope sentence is the exact D-13 wording in README and the same facts in CLAUDE.md, the guide and the skill; no document claims Windows, Linux, CI or a real DAW"

patterns-established:
  - "Evidence before ticks: the VERIFICATION evidence section was written first and REQUIREMENTS.md edited after it"

requirements-completed: [RTSF-01, RTSF-02, RTSF-03, RTSF-05, VERIFY-02]

coverage:
  - id: D1
    description: "README documents the realtime-safety check beside the unchanged everyday gate, with both pass conditions, the run times and the macOS-only scope"
    requirement: RTSF-05
    verification:
      - kind: other
        ref: "grep -c build-tsan README.md (5); grep -c 'Proved on macOS (Apple Silicon); Windows and Linux are checked in Phase 6' README.md (1); git ls-files --eol README.md (w/crlf)"
        status: pass
    human_judgment: false
  - id: D2
    description: "The documented check runs green as written: everyday gate, [rt], [tsan], UI suite under TSan and the whole suite under TSan, all exit 0 with zero ThreadSanitizer warnings"
    requirement: RTSF-05
    verification:
      - kind: integration
        ref: "./build/tests/SpatialCoreTests; ./build/tests/SpatialCoreUITests; ctest --test-dir build-release/tests; ./build-release/tests/SpatialCoreTests '[rt]'; build-tsan '[tsan]', SpatialCoreUITests and SpatialCoreTests (whole)"
        status: pass
    human_judgment: false
  - id: D3
    description: "CLAUDE.md, the integration guide and the architecture skill describe the shipped realtime behaviour and the proof's scope"
    verification:
      - kind: other
        ref: "grep -c 'tracked as RTSF' docs/integration-guide.md (0); grep -c 'RTSF-01, Phase 5' CLAUDE.md (0); counters named in CLAUDE.md and the guide"
        status: pass
    human_judgment: true
    rationale: "Whether the prose reads clearly and accurately to a plugin author is a judgment no test asserts; the greps prove the named facts are present and the stale deferral is gone"
  - id: D4
    description: "PROJECT.md 'OSD Phase 5 follow-ups' row lists the five things OpenSpatialDelay must know at migration"
    verification:
      - kind: other
        ref: "grep -c 'OSD Phase 5 follow-ups' .planning/PROJECT.md (1)"
        status: pass
    human_judgment: true
    rationale: "Completeness of a hand-off list for another repository is a judgment call"
  - id: D5
    description: "The five Phase 5 requirements are ticked and Complete, and the two STATE deferrals Phase 5 owned are resolved, written after the evidence section"
    requirement: VERIFY-02
    verification:
      - kind: other
        ref: "grep -cE '^- \\[x\\] \\*\\*(RTSF-01|RTSF-02|RTSF-03|RTSF-05|VERIFY-02)\\*\\*' .planning/REQUIREMENTS.md (5)"
        status: pass
    human_judgment: false

duration: 70min
completed: 2026-10-06
status: complete
plan_head_before: 8927af6f11edd674ecdeaf7abba3c6705ea09af7
plan_head_after: HEAD_AFTER_PENDING
commits: COMMITS_PENDING
---

# Phase 5 Plan 08: Realtime-safety check documented, run green, and Phase 5 requirements ticked Summary

**The realtime-safety check (the `[rt]` allocation and lock trap, then `[tsan]`, the UI suite and the whole `SpatialCoreTests` suite under ThreadSanitizer) is written into the README beside the unchanged everyday gate, was run exactly as written with every exit status 0 and zero ThreadSanitizer warnings (whole suite: 386 cases, 1,938 s), and RTSF-01, RTSF-02, RTSF-03, RTSF-05 and VERIFY-02 are now ticked against the evidence below.**

## Performance

- **Duration:** about 70 min (start 2026-10-06T02:43Z from the plan-head ledger file; the whole-suite ThreadSanitizer run alone took 32 min 18 s)
- **Tasks:** 3 (Task 1 tracer; Tasks 2 and 3 auto)
- **Files modified:** 8 (0 created, apart from this SUMMARY)
- **BASE:** `8927af6f11edd674ecdeaf7abba3c6705ea09af7` (HEAD when this plan started)

## Accomplishments

- **Tracer:** README gained "Realtime-safety check (every phase end and before a release tag)" after the unchanged Debug/Release block. The diff against BASE adds 34 lines and removes none; README is still CRLF. The check was then run from the repository root word for word.
- Every doc a future session or a consumer reads now describes the shipped behaviour: oversized blocks rendered in pieces and counted, scratch guards that skip and count, atomic trajectory finals, the legacy HRTF route's thread contract and its two counters, the deliberate process-lifetime `SharedFFTCache`, one shared `SMLLookAndFeel` per process, and the proof with its macOS-only scope.
- PROJECT.md carries the "OSD Phase 5 follow-ups" row; REQUIREMENTS.md and STATE.md are updated after the evidence section.

## Task Commits

1. **Task 1 (tracer): README realtime-safety check, run green as written** - `947fa58` (docs)
2. **Task 2: CLAUDE.md, guide, skill, PROJECT.md row, demo comment** - `817276a` (docs)
3. **Task 3: requirement ticks and resolved STATE rows (after this evidence section)** - see "Plan metadata" below

**Plan metadata:** the `docs(05-08)` commit that carries this file, STATE.md and ROADMAP.md follows.

Tracer feedback gate: auto mode off, `human_verify_mode` default `end-of-phase`, and the tracer `<verify>` is automated-only, so the gate is the end-to-end run of the check itself. It passed on the first run; expansion (Tasks 2 and 3) went ahead.

## VERIFICATION evidence

### The check as run (this plan, Task 1, from the repository root, each step alone, macOS Apple Silicon)

| # | Command (as written in the README) | Result | Exit | TSan warnings | Time |
|---|---|---|---|---|---|
| 1 | `cmake --build build -j8` (Debug, every target) then `./build/tests/SpatialCoreTests` | 386 cases, 328,307 assertions, all passed (expected exit noise: "Leaked objects detected: 7 instance(s) of class FFT", D-08) | 0 | n/a | 422 s |
| 2 | `./build/tests/SpatialCoreUITests` (Debug) | 16 cases, 150 assertions, all passed | 0 | n/a | 14 s |
| 3 | `cmake --build build-release -j8` (every target) then `ctest --test-dir build-release/tests --output-on-failure` | 402 of 402 passed | 0 | n/a | 117 s |
| 4 | `ctest --test-dir build-release/tests -N \| grep -c 'ui:'` | 16 (must be above 0) | 0 | n/a | n/a |
| 5 | `./build-release/tests/SpatialCoreTests "[rt]"` | 46 cases, 2,514 assertions, none skipped | 0 | n/a | 19 s |
| 6 | the `build-tsan` configure command and `cmake --build build-tsan -j8` | configured and built (the tree already existed with the same flags; reconfigure and build were no-ops) | 0 | n/a | 1 s each |
| 7 | `[tsan]` subset, `SPATIALCORE_TEST_TIMEOUT_SCALE=10`, `TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1"`, log `/tmp/sc-p5-tsan.log` | 40 cases, 488 assertions, all passed | 0 | 0 | 992 s |
| 8 | `SpatialCoreUITests` under TSan, log `/tmp/sc-p5-ui-tsan.log` | 16 cases, 150 assertions, all passed | 0 | 0 | 45 s |
| 9 | whole `SpatialCoreTests` under TSan, log `/tmp/sc-p5-tsan-full.log`, final line `exit=0` | 386 cases: 343 passed, 43 skipped (the allocation-trap cases, which skip in this tree by design); 325,980 assertions, all passed, 0 skipped | 0 | 0 | 1,938 s |

The third automated verify of Task 1 (log exists and is not empty, final line exactly `exit=0`, zero warning lines) passed on the whole-suite log. The `[tsan]` subset grew from 29 cases (05-04) to 40: 5 more `[hrtf-switch]` cases (05-05) and 6 `[multi]`/`[fftcache]` cases (05-06). Its run time grew from 860 s to 992 s with it.

### ROADMAP Phase 5 success criteria

| # | Criterion | Proving commands and tags | Result |
|---|---|---|---|
| 1 | `renderSourceBuffers` and `updateSourceHRIR` cannot allocate in Release (SpatialCore#19) | Release `[rt][scratch]` (3 cases, the guards skip, keep the last good sound and count), `[oversized]` (five render paths, 30 blocks of 1024 on a 256-sample engine, 0 allocations; three bit-identity sections: one 256-sample block equals four 64-sample blocks bit for bit steady, during the renderer crossfade, and Simple-to-HRTF blend), the rewritten CR-03 case; Release ctest step 3 | pass (steps 3 and 5) |
| 2 | `TrajectoryEngine` finals are atomic and clean under ThreadSanitizer (SpatialCore#18) | build-tsan `[trajectory][threads][tsan]` (inside `[tsan]`), `[trajectory][step]`; `static_assert (std::atomic<float>::is_always_lock_free)` | pass (steps 7 and 9) |
| 3 | `setProfile()` from a UI timer during audio processing does not race on the libmysofa handle | `[hrtf-switch][ui-timer]` (30 Hz timer calling `setHRTFProfile` while another thread renders with all three engine flags set: 910 ticks, 51,921 blocks, 4 distinct profiles, 0 non-finite samples), `[hrtf-switch][misuse]`, `[hrtf-switch][in-use]`, `[legacy][threads]`; all inside `[tsan]` | pass (steps 7 and 9) |
| 4 | Several plugin instances render concurrently without crashing or buzzing (SpatialCore#12; OSD#96 and OSD#131) | `[multi][isolation]` (engine A bit-identical alone and beside a profile-switching engine B on another thread: 0 of 51,200 samples differ), `[fftcache]` (4 cases including 16 threads on orders 14 and 15), `[multi][stress]` (eight engines on eight threads), `[ui][multi]` (sixteen live maps share one face set; churn beside four live maps) | pass (steps 1, 3, 7, 8, 9) |
| 5 | A thread-sanitizer plus allocation-detector pass over the `processBlock` path reports zero allocations, zero locks, zero races | Release `[rt]` (46 cases, 2,514 assertions: canary, the 23-format matrix, 6 profiles, sizes, algorithms, trajectories, switches, ordering, legacy route, `[rt][rule]` lock inventory), and the whole suite under ThreadSanitizer: exit 0, 0 warnings | pass (steps 5 and 9) |

### Requirements

| Requirement | Proving commands and tags | Result |
|---|---|---|
| RTSF-01 | `[rt][scratch]`, `[oversized]`, CR-03 case, `[rt]`; Plan 05-02 | pass |
| RTSF-02 | `[trajectory][threads][tsan]`, `[trajectory][step]`, whole suite under TSan; Plan 05-04 | pass |
| RTSF-03 | `[hrtf-switch][ui-timer]`, `[hrtf-switch][misuse]`, `[hrtf-switch][in-use]`, `[legacy][threads]`, `[rt][legacy]` under TSan and the trap; Plan 05-05 | pass |
| RTSF-05 | `[rt]` (46 cases), `[rt][rule]`, `[tsan]` subset and whole suite under TSan; Plans 05-01, 05-03, 05-04 | pass |
| VERIFY-02 | `[multi][isolation]`, `[multi][stress]`, `[fftcache]`, `[ui][multi]`; Plans 05-06 and 05-07 | pass |

### D-07 old-behaviour and mutation results, Plans 05-01 to 05-07

| Plan | What was reintroduced or measured at BASE | What failed | How it was restored |
|---|---|---|---|
| 05-01 | Old Ambisonics NFC code (a temporary `juce::dsp::IIR::Coefficients<float>` built per object per block), six formats, 120 blocks | 1,427 / 2,854 / 4,281 / 5,708 / 7,135 / 8,562 allocations for FOA, SOA, HOA, 4OA, 5OA, 6OA; `test cases: 1 \| 0 passed \| 1 failed`, 12 of 27 assertions failed | fixed in `5d8e724` (in-place `std::array<float, 4>` assignment); 0 allocations after, `cmp` of raw output before and after exit 0 in Debug and Release |
| 05-02 | Old `renderDirectBinauralHRTF` at BASE, 30 blocks of 1024 on a 256-sample engine | HRTF configuration: 7 allocations and 7 frees (the `sourceAccumStorage_` grow); GREEN-commit mutation of `scratchFits` (miss path resizes and returns true): `[rt][scratch]` 2 of 3 cases failed, 8 assertions failed | fixed in `2d8df43`; mutation restored with `git checkout`, 41 assertions in 3 cases pass again |
| 05-03 | Old `static const float maxrE[4]` built with `std::cos` in `AmbisonicsAlgorithm::computeGains` | `[rt][algorithms]` failed: 2 locks (`__cxa_guard_acquire` on first use on the render thread), deterministic over 3 runs; two scratch mutations of `[rt][rule]` (a new file with a `std::mutex` and a `juce::SpinLock`; a replaced `Thread::sleep (2)` line) each failed the rule, naming the file or the stale allow-list entry | fixed in `9fe1829` (`static constexpr`, bit-identical by `[ambi][maxre][d10]` and `cmp`); both mutations restored, rule green |
| 05-04 | Unmodified `TrajectoryEngine` (plain float finals), `[trajectory][threads]` in build-tsan | exit 134, 1 `WARNING: ThreadSanitizer: data race` (`tick()` write against `getFinalAz()` read), 892 out-of-range readings in 2,918,251 passes | fixed in `52b089a` (atomic finals); same command now exit 0, 0 warnings |
| 05-05 | Legacy-route detector stubs; `fetch_add` in `noteLegacyRouteCall` removed; `markRendererUse()` made a no-op | `[hrtf-switch][misuse]` 2 of 4 assertions failed (`0 == 2`, `0 == 1`); in-use RED and mutation both 6 of 43 assertions failed. Not caught, stated plainly: removing the two early `setInUseByAudioThread (false)` calls still passed 3 of 3 runs because the end-of-block clear does the same job microseconds later; the early clear is reasoned, not mutation-proven | each mutation restored with `git checkout`, rebuilt, all pass |
| 05-06 | (1) `BinauralRenderer::hrtfDatabase` replaced by one function-local static; (2) the spinlock removed from `SharedFFTCache::getOrCreate` (build-tsan); (3) `getOrCreate` returning a fresh `make_shared` each call | (1) crash in 3 of 3 runs (exit 133 once, 134 twice); (2) 32 ThreadSanitizer reports, 3 of 4 `[fftcache]` cases failed; (3) 3 of 4 cases failed, 6 assertions | each restored with `git checkout`, tree rebuilt, `git diff` against BASE empty outside comments |
| 05-07 | `juce::SharedResourcePointer<MonoFaces>` replaced by a per-instance `std::unique_ptr<MonoFaces>` (the OSD#131 failure mode) | `[ui][multi]` 3 of 10 assertions failed: construction delta 16 (expected 1), 15 of 16 maps with a different face identity | restored with `git checkout`, tree rebuilt, 2 cases and 17 assertions pass |

### Platform scope (D-13) and what is deliberately left for later

- **Scope:** the proof was run on macOS (Apple Silicon). Windows and Linux are checked in Phase 6. CI runs none of this until Phase 6, and nothing here claims a real DAW.
- **Real-DAW multi-instance check (D-05):** with the OpenSpatialDelay migration, outside this repository.
- **Windows and Linux:** JUCE's fallback FFT spinlock with the shared cache (D-13, RESEARCH A3), and the `malloc_logger`-style trap, which skips on other platforms rather than passing green: Phase 6.
- **Legacy HRTF route:** `swapActiveRenderer` and friends stay, watched and counted, until the next major version (D-09).
- **A mixed trajectory reading** (azimuth from one tick, elevation from the next) is measured and pinned by `[trajectory][step]` and is no worse than the jump four shapes already make themselves; the documented consumer flow never reads these arrays on the audio thread.

## Files Created/Modified

- `README.md` - new "Realtime-safety check" subsection (34 lines added, none removed, CRLF kept)
- `CLAUDE.md` - Binaural, Trajectory and UI rows, the HRTF-switching paragraph (legacy route contract and counters), the Profile loader bullet (no stale deferral), a new "Realtime-safety proof (Phase 5)" bullet, the Tests bullet
- `docs/integration-guide.md` - control bullet (atomic getters), look-and-feel sharing note, legacy-route contract, rewritten Threading paragraph
- `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` - three-slot layout handoff wording and four realtime bullets (CRLF kept, 5 lines added, 1 changed)
- `.planning/PROJECT.md` - "OSD Phase 5 follow-ups" row
- `.planning/REQUIREMENTS.md` - five requirements ticked with a "Complete (Phase 5)" line each; Traceability rows Complete
- `.planning/STATE.md` - the two Phase 5 deferred rows marked resolved
- `examples/demo/DemoComponent.h` - one comment no longer says RTSF-02 is a Phase 5 item (see deviation 1)

## Decisions Made

- Followed the plan as written for the README text and the order of the steps.
- Recorded the run times as measured in this run rather than the SUMMARY figures from earlier plans, since the `[tsan]` subset and the whole suite grew after those were taken (860 s then, 992 s now for the subset).
- Task 3 is committed as one commit holding the requirement ticks and the STATE rows, after this evidence section existed on disk.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Stale comment deferring RTSF-02 to Phase 5 in the demo header**
- **Found during:** Task 2 (the final grep for any sentence still deferring a Phase 5 item)
- **Issue:** `examples/demo/DemoComponent.h` said the cross-thread read of `TrajectoryEngine` output "is RTSF-02, Phase 5, and the demo avoids it". RTSF-02 is fixed, so the comment was false.
- **Fix:** Two comment lines reworded (the getters are lock-free atomics since RTSF-02; copying on the message thread stays the recommended flow). Comment only, outside the plan's `files_modified`, and outside the `[rt][rule]` scan (which reads `src` and `include`).
- **Files modified:** `examples/demo/DemoComponent.h`
- **Verification:** `git grep -n "RTSF-01\|Phase 5" -- examples docs README.md CLAUDE.md .claude` shows no remaining deferral.
- **Committed in:** `817276a`

---

**Total deviations:** 1 auto-fixed (Rule 1, comment only)
**Impact on plan:** None on behaviour or scope.

## Issues Encountered

- The first read of the plan file through the file-read hook returned only its first line; it was read in full through the shell instead. No effect on the work.
- A `sed -i` substitution failed on BSD sed with an extra-characters error; the edit was redone with Python. No effect on the tree.
- Expected noise, all with exit status 0: "Leaked objects detected: 7 instance(s) of class FFT" and one JUCE assertion line at the end of each Debug run (D-08, the deliberate process-lifetime `SharedFFTCache`), and the 43 skipped allocation-trap cases in the ThreadSanitizer tree.

## Known Stubs

None. No placeholder values, empty data sources or TODO text were added.

## Threat Flags

None. Documentation and planning edits only; no new endpoint, auth path, file access or schema change. T-05-19 (overclaimed proof) is mitigated: the scope sentence is in README and the same facts are in CLAUDE.md, the guide and the skill, and the deferred items are listed above. T-05-20 (a gate nobody runs) is mitigated: the `[tsan]` subset is the routine check and the whole suite runs only at phase end and before a tag, with its run time written next to the command.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Phase 5 is complete from the plan side: all eight plans have summaries and the five requirements are ticked. The phase-end gate (`/gsd-verify-work`) can reuse the README check as written.
- Phase 6 inherits: Windows and Linux for the realtime proof, the fallback-FFT spinlock, CI that runs the tests (CI-01), and the OSD migration hand-off row.
