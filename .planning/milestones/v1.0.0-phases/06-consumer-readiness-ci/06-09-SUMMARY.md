---
phase: 06-consumer-readiness-ci
plan: 09
subsystem: docs
tags: [readme, claude-md, integration-guide, requirements, ci, harness, coverage, shared-fft-cache]

requires:
  - phase: 06-consumer-readiness-ci
    provides: "06-01..06-08: umbrella headers, unsupplied-gains counter, harness, five CI jobs, coverage tooling and the 93% engine target"
provides:
  - "README, CLAUDE.md, integration guide and architecture skill describe the five CI jobs, the consumer contract and the real reach of the realtime-safety proof"
  - "README Consumer harness and Coverage sections; the harness command run as written and passing"
  - "SharedFFTCache.h platform note with the sourced fallback-FFT finding, and the OSD claim checked against OSD's git history"
  - "REQUIREMENTS/ROADMAP carry the D-02, D-08, D-16 amendments; OQ-4 closed; Out of Scope rows corrected; PROJECT.md OSD Phase 6 follow-ups row; STATE resolved rows"
  - "User memory note no longer says CI runs zero tests"
affects: [06-10 phase close (ticks INTG-01, INTG-02, CI-01, TEST-01; records the protection mode; the push and CI confirmation of 06-08), OSD migration]

actuals:
  tokens: 4900
  tasks: 3
  commits: 3

plan_head_before: abf1176fc54b0d7112ba33cd5b79b0fe6f0cd795
plan_head_after: 89ec9733c1258f37971f054f41ba74d8db1e91a3

tech-stack:
  added: []
  patterns:
    - "A platform-reach statement in a document is written from the SUMMARY of the plan that measured it, and a claim about another repository is checked in that repository's git history before it is written"

key-files:
  created: []
  modified:
    - README.md
    - CLAUDE.md
    - docs/integration-guide.md
    - .claude/skills/spatialcore-architecture/spatialcore-architecture.md
    - include/SpatialCore/Binaural/SharedFFTCache.h
    - .planning/REQUIREMENTS.md
    - .planning/ROADMAP.md
    - .planning/PROJECT.md
    - .planning/STATE.md

key-decisions:
  - "The 'OSD v1.0 ships the same SharedFFTCache' claim was verified in OpenSpatialDelay's git history (tag v1.0.0, e3eb766) and is written as verified; the claim that OSD's own JUCE build selects the fallback FFT was not checked and is not made"
  - "No requirement is ticked and branch protection is not described as applied; PROJECT.md says the protection mode is recorded by Plan 06-10"
  - "The 'Out of Scope' row 'Windows / Linux CI matrices' is replaced by 'Running the test suite on Windows' rather than deleted, so the old scope line stays traceable"

patterns-established:
  - "Stale-phrase grep over the four consumer-facing files is the acceptance check for platform-reach wording"

requirements-completed: [INTG-01, INTG-02, CI-01, TEST-01]

coverage:
  - id: D1
    description: "README describes the CI that exists (all five job names), has Consumer harness and Coverage sections, and its harness command runs as written and passes"
    requirement: CI-01
    verification:
      - kind: integration
        ref: "HARNESS_WORK_DIR=/tmp/sc-p6-09-harness examples/harness/scripts/build-consumer.sh 310fb9358cf00cac9430fb4ddf286fe2a4c19942 -- -DFETCHCONTENT_SOURCE_DIR_JUCE=... -DFETCHCONTENT_SOURCE_DIR_MYSOFA=...: exit=0, 'HarnessCheck: PASS (23 formats, 12 algorithms, profile 5 embedded)', 'warnings: 0 in the harness and in SpatialCore', submodule at 310fb93"
        status: pass
      - kind: other
        ref: "grep for the five job names in README.md; stale-phrase grep count 0; git ls-files --eol README.md w/crlf; python3 -I scripts/coverage-report.py --self-test passes"
        status: pass
    human_judgment: false
  - id: D2
    description: "CLAUDE.md, integration guide, architecture skill and SharedFFTCache.h state the consumer contract and platform reach as shipped, with no stale phrase"
    requirement: INTG-02
    verification:
      - kind: other
        ref: "stale-phrase grep over the four files prints no line; getUnsuppliedGainsBlockCount in CLAUDE.md; SpatialCoreUI.h in the guide; FFTFallback in SharedFFTCache.h; header diff is comment lines only; skill file still CRLF"
        status: pass
      - kind: unit
        ref: "cmake --build build-release --target SpatialCoreTests; ./build-release/tests/SpatialCoreTests '[fftcache],[umbrella]': All tests passed (44 assertions in 6 test cases)"
        status: pass
    human_judgment: false
  - id: D3
    description: "REQUIREMENTS, ROADMAP, PROJECT and STATE carry the user's amendments, OQ-4 is closed, the OSD hand-off row exists, nothing is ticked, and the memory note is updated"
    requirement: TEST-01
    verification:
      - kind: other
        ref: "Task 3 automated check prints VERIFY-OK; grep for ticked INTG-01/INTG-02/CI-01/TEST-01 prints 0"
        status: pass
    human_judgment: false
  - id: D4
    description: "The wording of the amended requirement sentences and the OSD hand-off row says what the user decided"
    verification: []
    human_judgment: true
    rationale: "Whether the amendment prose matches the user's intent is a reading judgment; the before/after text is listed in this SUMMARY for review"

duration: 8 min
completed: 2026-10-24
status: complete
---

# Phase 6 Plan 09: Documentation truth Summary

**README, CLAUDE.md, the integration guide, the architecture skill and the SharedFFTCache note now describe the five CI jobs, the two-umbrella consumer contract and a macOS-only realtime proof, with the README harness command run as written (pass) and OpenSpatialDelay's SharedFFTCache claim verified in its git history; the planning record carries the user's D-02, D-08 and D-16 amendments and OQ-4 closed at 93%.**

## Performance

- **Duration:** about 8 min (2026-10-24T14:17:53Z to 14:25:45Z; the harness run was about 3 min of it)
- **Tasks:** 3
- **Files modified:** 9 tracked files, plus two memory files outside the repository (not committed)

## Accomplishments

- README: integration snippet shows both umbrellas; the local-gate section names `macos-gate`, `linux-tests`, `windows-build` and `coverage`; the realtime section states the proof reaches macOS only; new `### Consumer harness` and `### Coverage` sections; a Windows zlib line beside "zlib (system)".
- CLAUDE.md, the guide, the skill and `SharedFFTCache.h` state the same facts (consumer contract, CI, platform reach, fallback-FFT finding).
- REQUIREMENTS.md, ROADMAP.md, PROJECT.md and STATE.md carry the amendments; the user's memory note no longer says CI runs zero tests.

## Harness run (Task 1)

Run as README writes it, with the orchestrator's two substitutions (the pushed commit instead of `HEAD`, because HEAD is not on GitHub yet and the orchestrator pushes once after this plan; the `FETCHCONTENT_SOURCE_DIR_*` pass-through that README documents):

`HARNESS_WORK_DIR=/tmp/sc-p6-09-harness examples/harness/scripts/build-consumer.sh 310fb9358cf00cac9430fb4ddf286fe2a4c19942 -- -DFETCHCONTENT_SOURCE_DIR_JUCE=$PWD/build/_deps/juce-src -DFETCHCONTENT_SOURCE_DIR_MYSOFA=$PWD/build/_deps/mysofa-src`

Result: **exit=0**, `HarnessCheck: PASS (23 formats, 12 algorithms, profile 5 embedded)`, `warnings: 0 in the harness and in SpatialCore; 2 line(s) mentioning 'warning:' in dependencies`, submodule status ` 310fb9358cf00cac9430fb4ddf286fe2a4c19942 SpatialCore (310fb93)`. Log: `/tmp/sc-p6-09-harness.log`. `python3 -I scripts/coverage-report.py --self-test` also passes. Note that the harness source comes from this working tree and SpatialCore comes from `310fb93`; no harness or library file changed between `310fb93` and this plan's commits (this plan changed one comment block in `SharedFFTCache.h` and documents).

## OpenSpatialDelay SharedFFTCache verification

Read-only, in `/Users/andrewrahman/conductor/repos/openspatialdelay` (git objects only, nothing pushed, no CI run):

- Tag `v1.0.0` (commit `e3eb766`, 2026-04-17, "Merge pull request #206 ... retag-v100") contains `Source/SharedFFTCache.h`: `struct SharedFFTCache { juce::SpinLock lock; std::map<int, std::shared_ptr<juce::dsp::FFT>> cache; getOrCreate(int) }` and a function-local-static `getSharedFFTCache()` that never destroys it. This is the same design as `include/SpatialCore/Binaural/SharedFFTCache.h`.
- The same tag has `.github/workflows/build-windows.yml` (Windows x64 VST3) and its release notes list "VST3 + AU for macOS and Windows". So OSD's first release already ships this cache on Windows.
- **Outcome: the claim holds and is written as verified** (CLAUDE.md, `SharedFFTCache.h`, PROJECT.md). Not checked, and therefore not written: that OSD's own JUCE version selects the fallback FFT on Windows (the JUCE 9.0.0 finding is from SpatialCore's CI logs). OSD's workflow uses the vcpkg triplet `zlib:x64-windows-static`, while SpatialCore's CI uses `x64-windows-static-md`; PROJECT.md says so.

## Stale phrases replaced (file, before, after)

| File | Before | After |
|---|---|---|
| README.md | "CI currently runs zero tests (this is fixed in Phase 6), so this local gate is the one that counts." | CI runs this same gate on every push and pull request: `macos-gate`, `linux-tests`, `windows-build`, `coverage`; run the local gate before pushing anyway; `ctest --test-dir build-release` from the build root finds the tests too |
| README.md | "Proved on macOS (Apple Silicon); Windows and Linux are checked in Phase 6." | Proved on macOS (Apple Silicon) only; Linux runs the suite in CI but the trap is macOS-only; Windows is built, not run; fallback-FFT `SpinLock` (`juce_FFT.cpp:115`) on Linux and Windows; no claim covers a real DAW |
| README.md | "CI runs none of this until Phase 6." | CI runs steps 1-5 as `realtime-nightly` (nightly once on main, and on demand) with the measured runner times from 06-05 (46 s, 1887 s, 58 s, 3183 s; 100 min job) |
| README.md | "**zlib** (system) — compression for SOFA files" | system on macOS and Linux; on Windows vcpkg `zlib:x64-windows-static-md` and `zs.lib` through `ZLIB_LIBRARY` |
| CLAUDE.md | "Proved on macOS (Apple Silicon); Windows and Linux are checked in Phase 6 (D-13), and no claim covers CI or a real DAW" | macOS-only proof, Linux suite without the trap (44 cases skip), Windows build-only (D-08), fallback-FFT finding in one clause with OSD verification, `realtime-nightly`, "No claim covers a real DAW" |
| CLAUDE.md | Tests bullet: "CI runs zero tests until Phase 6 (CI-01), so this local gate is the only gate." | five CI jobs described, local gate as the quick check, root `ctest` works, `SPATIALCORE_BUILD_TESTS` OFF when consumed, `SPATIALCORE_ENABLE_COVERAGE` and `build-cov/`, the 93% engine target |
| CLAUDE.md | Dependencies: "zlib (system)" | system on macOS and Linux, Windows supplies it (vcpkg recipe); CMake 4 policy flag for a consumer's own libmysofa |
| docs/integration-guide.md | "CI runs zero tests until Phase 6." (Testing) | the four per-push jobs and `realtime-nightly`, the local gate before pushing, and the advice to assert `getUnsuppliedGainsBlockCount() == 0` |
| docs/integration-guide.md | "(proved on macOS, Apple Silicon, by the `[rt]` trap tests and a ThreadSanitizer run; Windows and Linux are checked in Phase 6)" (line 460, **found beyond the plan's list**) | Linux without the trap, Windows built and not run, fallback-FFT `SpinLock`, no real-DAW claim |
| spatialcore-architecture.md | "Proved on macOS (Apple Silicon); Windows and Linux are checked in Phase 6" | macOS-only proof with the Linux and Windows facts |
| SharedFFTCache.h | "... it is checked in Phase 6 (D-13), not claimed here." | the sourced finding: `FFTFallback` (`juce_FFT.cpp` line 89, priority -1), `SpinLock` in `perform()` at line 115, engines ranked above it and why Linux and Windows builds here use the fallback, vDSP takes no lock, not measured, OSD already ships the same design, not fixed in v1 (comment lines only) |
| .planning/STATE.md | "CI 'green' is test-vacuous ... Owned by Phase 6." and the deferred row "Open — Phase 6" | "RESOLVED in Phase 6 (06-01, 06-05)" and "Resolved — Phase 6 (06-01, 06-05)" |

## Amended requirement sentences (before / after)

- **INTG-01 acceptance.** Before: "includes only `<SpatialCore/SpatialCore.h>`". After: "includes `<SpatialCore/SpatialCore.h>` and `<SpatialCore/SpatialCoreUI.h>` (the two umbrella headers; amended 2026-10-17, D-02)", plus an "Amended 2026-10-17 (D-02, D-01)" bullet naming the real submodule from the GitHub remote.
- **CI-01 scope note.** Before: "this supersedes the 2026-08-09 'Windows/Linux CI is out of scope' line only for macOS + JUCE 9. Cross-platform matrices remain out of scope." After: macOS is the full gate, Linux runs the suite, Windows builds only, a Windows test run is deferred (D-08).
- **TEST-01 UI note.** Before: "`src/UI/*.cpp` is excluded by design ... Covering it needs a separate UI test target — post-v1." After: UI code is measured separately through `SpatialCoreUITests` (exists since Phase 4), shown and not a rule (D-16).
- **OQ-4.** Before: "**Blocked** — no coverage tooling exists to measure against". After: "**Closed 2026-10-24 — engine line coverage 93%, UI report-only**" with the CI figures (94.19% engine, 25.89% UI) and the enforcing file. The Traceability row for TEST-01 reads "OQ-4 closed (engine 93%, UI report-only)".
- **Out of Scope.** "Windows / Linux CI matrices | v1 verifies macOS only" became "Running the test suite on Windows | CI builds Windows (no test run) and runs the suite on macOS and Linux"; "UI test target for `src/UI/*.cpp` ... Post-v1" became "**Done** — `SpatialCoreUITests` exists since Phase 4".
- **ROADMAP.** Phase 6 criterion 1 names the two umbrella headers; the OQ-4 paragraph records the 93% target and that CI enforces it.

## Provenance of the "14" layouts row

The Out of Scope row "Only the 14 built-in layouts" entered `.planning/REQUIREMENTS.md` in commit **`f0b14f4`** (2026-08-10, "docs: rewrite plan against real branch; triage all 14 GitHub issues", found with `git log -S'Only the 14 built-in layouts'`). On that same day CLAUDE.md said "13 ITU-R layouts", so 14 did not come from CLAUDE.md; it reads as a hand count that matched neither document. The true figure is **15**: `NUM_LAYOUT_DEFS == 15` is pinned by a `static_assert` in `include/SpatialCore/IO/SpeakerLayout.h:106`, and CLAUDE.md was corrected from 13 to 15 by `0ab3930` (2026-08-14, "freeze output-format count contract end-to-end"). The row now reads 15 and carries this trail.

## Task Commits

1. **Task 1 (tracer): README** - `3d47008` (docs)
2. **Task 2: CLAUDE.md, integration guide, skill, SharedFFTCache.h** - `d219198` (docs)
3. **Task 3: planning documents** - `89ec973` (docs); the memory note (outside the repository) is not committed

**Plan metadata:** the docs commit that adds this SUMMARY.

## Files Created/Modified

See `key-files`. Outside the repository: `/Users/andrewrahman/.claude/projects/-Users-andrewrahman-Projects-parked-SpatialCore/memory/spatialcore-ship-and-verify-flow.md` (description, `modified` date, first bullet) and the matching `MEMORY.md` index line.

## Decisions Made

See `key-decisions`. The README gate command block is unchanged (`git diff` shows hunks only outside the block); the README still writes `HEAD` in its harness command, and the substitution to the pushed commit is only in the run recorded above.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical] Windows zlib prerequisite documented**
- **Found during:** Task 1 and 2 (orchestrator carry-forward from 06-06)
- **Issue:** README and CLAUDE.md said "zlib (system)", true only on macOS and Linux; a Windows consumer would not know to supply it.
- **Fix:** documented the vcpkg `zlib:x64-windows-static-md` recipe and `zs.lib` in README Dependencies, CLAUDE.md Dependencies and a fifth note in the integration guide's Step 3.
- **Files modified:** README.md, CLAUDE.md, docs/integration-guide.md
- **Committed in:** `3d47008`, `d219198`

**2. [Rule 1 - Bug] A stale "Windows and Linux are checked in Phase 6" remained at integration-guide.md line 460**
- **Found during:** Task 2 (stale-phrase grep)
- **Issue:** the plan listed other sites, not this one; the grep for `checked in Phase 6` caught it.
- **Fix:** rewritten to the 06-06 truth (see the table).
- **Files modified:** docs/integration-guide.md
- **Committed in:** `d219198`

**3. [Rule 1 - Bug] A first CLAUDE.md wording added a new `1.x.y` string**
- **Found during:** Task 2 acceptance (`grep -c -E "1\.[0-9]+\.[0-9]+" CLAUDE.md` went from 2 to 3 because of "OpenSpatialDelay v1.0.0")
- **Fix:** reworded to "OpenSpatialDelay's first release ... read in its git history 2026-10-24"; the count is 2 again. `SharedFFTCache.h` still names the tag `v1.0.0` (a comment, OSD's version, not SpatialCore's).
- **Committed in:** `d219198`

### Orchestrator overrides (not auto-fixes)

**4. Harness run used the pushed commit `310fb93` instead of `$(git rev-parse HEAD)`** (HEAD is not on GitHub; the orchestrator pushes once after this plan), with the `FETCHCONTENT_SOURCE_DIR_*` flags the README documents.

**5. "Protection mode recorded by Plan 06-10" wording** in PROJECT.md row item (7) and the nightly-on-main caveat in README/CLAUDE.md, because the user may skip branch protection and `ci.yml` is not on `main` yet. No requirement ticked.

---

**Total deviations:** 3 auto-fixed (1 missing-critical, 2 bugs), 2 orchestrator instructions. **Impact on plan:** documentation only; no library code changed (one comment block in `SharedFFTCache.h`), no test changed.

## Issues Encountered

- `.github/workflows/ci.yml`'s header comment still says the `coverage` job "is report-only until scripts/coverage-targets.json exists", which stopped being true in 06-08. This plan's file list does not include `ci.yml`, so it was left as is; 06-10 (or the orchestrator's push) may fix that one comment line.
- The README's "nightly" statement carries the caveat that a schedule only fires from `main` (06-05 known gap), so it stays true before and after the merge.

## Known Stubs

None.

## Threat Flags

None. Documentation and one header comment only; no network endpoint, auth path or file-access change. T-06-24 (overclaim) is mitigated: every platform statement was written from the 06-05 and 06-06 SUMMARYs, the stale-phrase greps print nothing, and no document claims a Windows test run, a Linux allocation-trap proof or a real-DAW check.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 06-10: push (orchestrator), confirm the CI `coverage` job shows `Target: 93% - PASS` and that `macos-gate` stays green on the pushed commit; tick INTG-01, INTG-02, CI-01, TEST-01; record the branch-protection mode (the PROJECT.md row points at it); merge with `main` (conflicts expected in STATE.md, RenderEngine.h, tests/CMakeLists.txt per the handoff).
- The memory note says CI takes 20-35 min per run; that is from the handoff, not re-measured here.

## Self-Check: PASSED

- Files: README.md, CLAUDE.md, docs/integration-guide.md, the skill file, SharedFFTCache.h, REQUIREMENTS.md, ROADMAP.md, PROJECT.md and STATE.md exist and are committed; the memory files were edited in place.
- Commits `3d47008`, `d219198`, `89ec973` are ancestors of HEAD; `git rev-list --count abf1176..89ec973` prints 3.
- Task 1 automated checks passed (harness exit 0 and PASS, self-test, five job names, stale phrases 0, README still CRLF; `### Consumer harness` and `### Coverage` each match once with the CR form, `SpatialCoreUI.h` appears once).
- Task 2 automated checks passed (stale-phrase grep empty, counter/umbrella/FFTFallback present, header diff comment-only, `[fftcache],[umbrella]` pass after a Release rebuild, skill file CRLF, `1.x.y` count in CLAUDE.md still 2).
- Task 3 automated check printed VERIFY-OK; no requirement is ticked.

---
*Phase: 06-consumer-readiness-ci*
*Completed: 2026-10-24*
