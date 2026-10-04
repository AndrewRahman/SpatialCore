---
phase: 03-binaural-defects-hrtf-packaging
plan: 10
subsystem: docs
tags: [docs, claude-md, integration-guide, skills, phase-gate, consumer-mode, dr-3, hrtf]

requires:
  - phase: 03-binaural-defects-hrtf-packaging
    provides: "03-04 Simple-path cue bank, 03-05 embedded profiles and resolution chain, 03-06 setHRTFProfile, 03-08 libmysofa v1.3.5, 03-09 sample-based renderer crossfade and engineSelectsHRTF"
provides:
  - "CLAUDE.md, README, integration guide, development-roadmap note and the two auto-loading skills describe the shipped Phase 3 behaviour"
  - "PROJECT.md External Dependencies rows: OSD Phase 3 follow-ups (gates v1) and OSP Phase 3 follow-up (v2)"
  - "Phase gate evidence: Debug 260/260, Release 260/260, KEMAR-only OFF build, consumer-mode proof of criterion 3, DR-3 OpenSpatialDelay build"
affects: [03-11, OpenSpatialDelay migration, OpenSpatialPanner submodule bump, verify-work for phase 3]

plan_head_before: 35ca839a7ad32599b05ef17346858581341ab0ad
plan_head_after: 93a68d694c965689f746bc29c38371b368f779ca
commits: 2

actuals:
  tokens: 8400
  tasks: 3
  commits: 2

tech-stack:
  added: []
  patterns:
    - "Doc claims about an API are grep-checked against include/ and CMakeLists.txt before they are written"
    - "Consumer-mode proof: a throwaway project that add_subdirectory()s the tree with no tests, no HRTF path and no install step"

key-files:
  created: []
  modified:
    - CLAUDE.md
    - README.md
    - docs/integration-guide.md
    - docs/development-roadmap.md
    - .claude/skills/spatial-audio-dsp/SKILL.md
    - .claude/skills/spatialcore-architecture/spatialcore-architecture.md
    - .planning/PROJECT.md
    - .planning/REQUIREMENTS.md

key-decisions:
  - "describeHRTFProfileStatus is documented as the free function spatialcore::describeHRTFProfileStatus (status), not a RenderEngine member, because that is what HRTFProfile.h declares"
  - "The Windows shared-folder path is documented as implemented but not verified, matching the resolver header"
  - "The spatial-audio-dsp skill's profile table was renumbered to match kHRTFProfiles (it had KEMAR at 1); a doc-versus-code contradiction found while editing row 0"
  - "The libmysofa pin bump in OSD (v1.3.2 to v1.3.5) is listed in the OSD Phase 3 follow-ups row, from 03-08's deferred items"

patterns-established:
  - "Provenance first: every corrected line below names the commit that made it wrong or stale"

requirements-completed: [DATA-01, EXTR-02, BUG-01, BUG-02]

coverage:
  - id: D1
    description: "CLAUDE.md, README, the integration guide and the development-roadmap note describe embedded HRTF data, the shared-folder override, setHRTFProfile, engineSelectsHRTF and libmysofa v1.3.5; the stale 'not BinaryData' and bundle-relative text is gone"
    requirement: "DATA-01"
    verification:
      - kind: other
        ref: "Task 1 verify: grep -q SpatialCoreHRTFData/setHRTFProfile/engineSelectsHRTF CLAUDE.md, setHRTFProfile docs/integration-guide.md, SPATIALCORE_EMBED_ALL_HRTF README.md, count of Contents/Resources/HRTF in CLAUDE.md = 0"
        status: pass
    human_judgment: true
    rationale: "Whether the prose reads correctly to a future session is a reading judgment; the greps only prove the terms are present and the stale ones absent"
  - id: D2
    description: "Both auto-loading skills, the PROJECT.md follow-up rows and the BUG-01 line references match Phase 3"
    requirement: "BUG-01"
    verification:
      - kind: other
        ref: "Task 2 verify: OSD Phase 3 follow-ups and sc12 in PROJECT.md, setHRTFProfile in the architecture skill, 'background threaded (60Hz timer)' count 0, 'No spectral coloring' count 0, DirectBinauralAlgorithm.cpp:26 count 0"
        status: pass
    human_judgment: true
    rationale: "Cross-repo follow-up wording is a planning judgment the user approves at UAT"
  - id: D3
    description: "Full suite passes in Debug (260 of 260, no BinauralRenderer assertion line) and Release (260 of 260 under ctest)"
    requirement: "EXTR-02"
    verification:
      - kind: unit
        ref: "./build/tests/SpatialCoreTests > /tmp/sc-0310-debug.log (All tests passed, 309246 assertions in 260 test cases, exit 0)"
        status: pass
      - kind: unit
        ref: "ctest --test-dir build-release/tests (100% tests passed out of 260)"
        status: pass
    human_judgment: false
  - id: D4
    description: "KEMAR-only (SPATIALCORE_EMBED_ALL_HRTF=OFF) build passes [hrtf-embed], [hrtf-resolve] and the OFF branch of [hrtf-switch][failure]"
    requirement: "DATA-01"
    verification:
      - kind: unit
        ref: "/tmp/sc-kemar-only/tests/SpatialCoreTests \"[hrtf-embed],[hrtf-resolve],[hrtf-switch][failure]\" (All tests passed, 873 assertions in 16 test cases)"
        status: pass
    human_judgment: false
  - id: D5
    description: "Criterion 3 in consumer mode: a throwaway project that add_subdirectory()s this tree switches to profiles 1-5 through setHRTFProfile and renders finite, non-silent audio with source Embedded for each"
    requirement: "DATA-01"
    verification:
      - kind: integration
        ref: "/tmp/sc-consumer/build/ScConsumer_artefacts/Debug/ScConsumer (exit 0, five 'profile N ... OK' lines, output in /tmp/sc-consumer/run.log)"
        status: pass
    human_judgment: false
  - id: D6
    description: "DR-3: OpenSpatialDelay at 30391cd compiles and links against this tree (OpenSpatialDelay and OpenSpatialDelayTests), with the OSD repository untouched and no plugin installed"
    requirement: "EXTR-02"
    verification:
      - kind: integration
        ref: "cmake --build /tmp/osd-dr3-check/build --target OpenSpatialDelay OpenSpatialDelayTests --clean-first (exit 0, 0 errors, 255 compile steps)"
        status: pass
    human_judgment: false
  - id: D7
    description: "Phase sign-off: all five ROADMAP criteria read as met on the evidence below"
    verification: []
    human_judgment: true
    rationale: "Phase sign-off is the user's call; Claude gathers the evidence (the Task 3 human-check)"

duration: 13 min
completed: 2026-10-04
status: complete
---

# Phase 3 Plan 10: Docs match the code, and the phase gate Summary

**CLAUDE.md, README, the integration guide and both auto-loading skills now describe embedded HRTF data, one-call `setHRTFProfile`, `engineSelectsHRTF`, the Simple-path cue bank and libmysofa v1.3.5, the OSD and OSP follow-ups are on the ledger, and the phase gate is green: Debug 260/260, Release 260/260, KEMAR-only build, a real consumer-mode build that plays all five profiles from built-in data, and OpenSpatialDelay 30391cd still builds.**

## What you will hear

Nothing in the sound changes in this plan. Eight documents were corrected so future sessions and plugin authors read the truth, and the evidence for the phase was gathered.

## Performance

- **Duration:** 13 min (a long part of it was the Debug suite, 3 min 46 s)
- **Started:** 2026-10-04T16:16:46Z
- **Completed:** 2026-10-04T16:29Z (before this file and the state update)
- **Tasks:** 3
- **Files modified:** 8 tracked (no source code, no test)

## Accomplishments

- Replaced the CLAUDE.md claims that HRTF profiles are "not BinaryData" and resolve bundle-relative with the shipped behaviour, added the `setHRTFProfile` / `getHRTFProfileStatus` / `engineSelectsHRTF` paragraph to Key Interfaces, and added the loader and the sample-based-crossfade design principles.
- Wrote a new "HRTF profiles" section in the integration guide (profile numbers, one call, status, `engineSelectsHRTF` + `engineComputesGains`, shared folder paths and the same-filename rule, CMake option, Git LFS guard, threading, migration, the Simple sound change).
- Recorded the OSD and OSP follow-ups in PROJECT.md and fixed the BUG-01 line references.
- Ran the gate: every criterion has fresh evidence (below).

## Task Commits

1. **Task 1: CLAUDE.md, README, integration guide, development-roadmap note** - `88af70e` (docs)
2. **Task 2: skills, PROJECT.md follow-up rows, REQUIREMENTS.md line references** - `93a68d6` (docs)
3. **Task 3: phase gate** - no commit (builds in /tmp, outside the repository)

**Plan metadata:** the docs commit(s) that follow this file.

## Where the wrong text came from (git trail)

| Corrected text | Where it came from | Why it is wrong now |
|---|---|---|
| CLAUDE.md Build System "loaded at runtime ... no BinaryData ... bundle-relative `<bundle>/Contents/Resources/HRTF/`" and Critical Rules "not BinaryData" | `87cb7a3` (2026-07-06, CR-01) replaced the initial-commit claim `24e3185` ("embedded as BinaryData", already untrue of the code that day). CR-01 described the runtime loader of that time and an OSD resolver; CONTEXT records it was already wrong about OSD, whose v1.0.0 embeds all five via `juce_add_binary_data(HRTFData ...)` | OQ-6 (2026-08-10, DR-5) ruled embed-by-default; Plans 03-05 (`84e7032` to `da9b659`) embedded the profiles and 03-06 added the engine switch, so the runtime-only text became false when those landed |
| `docs/development-roadmap.md` status note "runtime Git-LFS HRTF loading (NOT BinaryData)" | `3da7d89` (2026-08-04) wrote the status note from the then-current code | Same reason: 03-05. The same note also called "BinaryData HRTF / rebuild all consumers" historical and inaccurate, which would now contradict itself, so that phrase was removed too (the body lines 125 and 173 about embedded BinaryData are historical but are true again) |
| libmysofa "v1.3.2" in CLAUDE.md, README.md and the skill | Initial commit `24e3185` pinned v1.3.2 and the docs copied it | Plan 03-08 (`c1c2e42`, user decision `keep-upgrade` D-17) moved the pin to v1.3.5 |
| spatial-audio-dsp skill "No spectral coloring", "No elevation cues", row 0, and the "binary resource embedded / loaded at runtime" lines | `a7976ca` (2026-04-12, the bulk add of the 15 skills), copied from the pre-fix behaviour | Plan 03-04 added the rear/up/down cue bank (BUG-01); 03-05 embedded the data |
| spatial-audio-dsp skill profile table (KEMAR as index 1, SADIE 2, CIPIC 3, HUTUBS 4, Bernschuetz 5) | `a7976ca` as well | It never matched `kHRTFProfiles` (`HRTFProfile.h`, D-07: 1 SADIE, 2 CIPIC, 3 HUTUBS, 4 Bernschuetz, 5 KEMAR). Found while editing row 0; corrected in the same commit (see Deviations) |
| architecture skill "HRTF profile loading is background threaded (60Hz timer)" | `a7976ca` | That was OSD's consumer-side timer glue; Plan 03-06 replaced it with the engine-owned loader |
| REQUIREMENTS.md BUG-01 `DirectBinauralAlgorithm.cpp:26` / 30 / 34 | `f0b14f4` (2026-08-10). The file then had the three lines at 26 / 29 / 34 (the "30" was off by one from the start) | `d191033` (2026-10-01, Phase 2, non-finite guard) and `61dd931` (2026-10-04, fast-math guard) added lines above them. Now 34 (lateral), 37 (ITD), 42 (ILD) |

## Phase gate

Evidence, all taken on the tree at `93a68d6` (the code is unchanged since Plan 03-09's `3c87250`; only documents changed after it).

**Debug.** `./build/tests/SpatialCoreTests > /tmp/sc-0310-debug.log`: `All tests passed (309246 assertions in 260 test cases)`, exit 0. `grep -c "Assertion failure in BinauralRenderer.cpp"` returns **0**. The known exit-time `Leaked objects detected: 3 instance(s) of class FFT` line is still there (pre-existing, see Deferred Items).

**Release.** `ctest --test-dir build-release/tests --output-on-failure`: `100% tests passed out of 260`.

**KEMAR-only (OFF).** `/tmp/sc-kemar-only` (`SPATIALCORE_EMBED_ALL_HRTF:BOOL=OFF`, source this tree), rebuilt, `"[hrtf-embed],[hrtf-resolve],[hrtf-switch][failure]"`: `All tests passed (873 assertions in 16 test cases)`, no "No test cases matched".

**Consumer mode (criterion 3).** `/tmp/sc-consumer` (CMake 3.22, JUCE and libmysofa taken from this tree's already-fetched copies, `SPATIALCORE_BUILD_TESTS OFF`, no HRTF path, no install step, shared folder absent: `ls "/Library/Application Support/Spatial Media Lab/HRTF"` reported "No such file or directory" before the run; the program also printed `shared folder present: no`). `main.cpp` includes only `<SpatialCore/SpatialCore.h>`, prepares a `RenderEngine` at 48000 / 512 on Binaural and, with `engineDerivesDispatch`, `engineComputesGains` and `engineSelectsHRTF` set, calls `setHRTFProfile` for 1 to 5 in turn and renders a 440 Hz sine at azimuth 30. It compiled and linked on the first try (no SpatialCore symbol failed) and exited 0:

```
profile 1: profile 1 ready (built-in) | blocks to active 8 | output RMS 0.37625 | finite 1 | source Embedded | OK
profile 2: profile 2 ready (built-in) | blocks to active 8 | output RMS 0.23211 | finite 1 | source Embedded | OK
profile 3: profile 3 ready (built-in) | blocks to active 8 | output RMS 0.18896 | finite 1 | source Embedded | OK
profile 4: profile 4 ready (built-in) | blocks to active 8 | output RMS 0.42604 | finite 1 | source Embedded | OK
profile 5: profile 5 ready (built-in) | blocks to active 8 | output RMS 0.14112 | finite 1 | source Embedded | OK
shared folder present: no
RESULT: all five profiles active from built-in data
```

("Blocks to active 8" is the 4096-sample renderer crossfade at 512-sample blocks; the program waits for the fade to end.)

**DR-3 (OpenSpatialDelay).** `/tmp/osd-dr3-check` was verified to equal a fresh `git archive 30391cd` (only differences: the empty JUCE submodule folder and its own logs), with this tree as its `SpatialCore` symlink and `-DJUCE_DIR` pointing at this tree's JUCE. First build: up to date (0.7 s, exit 0, since only documents changed after Plan 03-09). To get a fresh compile, rebuilt with `--clean-first`: `cmake --build build --target OpenSpatialDelay OpenSpatialDelayTests --clean-first -j8`, 255 compile steps, 42 s wall, exit 0, **0 `error:` lines** (log `/tmp/osd-dr3-check/build.log`). Not built: `all`, `_VST3`, `_AU`. OSD's regression goldens were not run. `git -C ~/conductor/repos/openspatialdelay-v1 status --porcelain` is empty before and after; a listing digest of the four plug-in folders is identical before and after (`83e8be44...`), so nothing was installed. Warnings inside `SpatialCore/include`: only the same five `-Wunused-parameter` in `OSC/ADMOSCReceiver.h` (45:50, 45:83, 46:52, 51:66, 51:86) that Phase 2's 02-07 and 02-09 already recorded; not from Phase 3 (no Phase 3 plan touched that header).

**Criteria, in plain English**

1. **Met.** A source at elevation +90 degrees and one at azimuth 180 degrees now sound different from one straight ahead on both paths. Simple path (03-04): front vs back 4.49 dB RMS / 11.59 dB max, front vs overhead 2.59 / 7.41 (`[bug01][simple]`). HRTF path on every built-in profile (`[bug01][hrtf]`, 03-01). All in the 260 passing cases.
2. **Met.** 32, 64 and 128-sample blocks: the engine output of a moving source matches the 512-sample reference (`[bug02][moving]` and the small-and-irregular-block case, 03-02/03-03), and profile switching is click- and dropout-free at 32, 64, 128 and 512 (03-09). Open for OSD itself: its own delay line and pitch shifter at those sizes (OSD#234, on the follow-up row).
3. **Met.** The consumer-mode program above: no install step, no path, shared folder absent, profiles 1 to 5 each active from built-in data with finite, audible output. A file in the shared folder winning over the built-in, a missing folder falling back silently, and an unusable file falling back with a report are covered by `[hrtf-resolve]` (03-05), which passes in both the default and the KEMAR-only build.
4. **Met.** `setHRTFProfile` and the legacy swap are click-free and dropout-free at 32, 64, 128 and 512 samples, Simple included with `engineSelectsHRTF` (03-09); worst sine step ratio 1.228 against a bound of 1.5, worst pink-noise RMS ratio 0.911 against 0.7; thread safety under real threads with zero ThreadSanitizer reports (03-07).
5. **Met.** `tests/Binaural/PartitionedConvolverTests.cpp` and `tests/Binaural/BinauralRendererTests.cpp` exist (added in `a2be01e`, Plan 03-01, tags `[convolver]` and `[renderer]`) and pass in both build types.

Nothing is unmet. The user's own sign-off on the five lines above is the Task 3 human-check and belongs to end-of-phase UAT.

## Files Created/Modified

- `CLAUDE.md` - Binaural and Engine rows, Key Interfaces HRTF paragraph, loader and crossfade principles, libmysofa v1.3.5, embedded-HRTF Build System line, Critical Rules HRTF line
- `README.md` - HRTF profile resolution chain, option, one-call switch, Simple cues; `loadFromBinaryData`; libmysofa v1.3.5 (CRLF file, kept)
- `docs/integration-guide.md` - new "HRTF profiles" section, example member replaced by a pointer, Keep-vs-Replace row
- `docs/development-roadmap.md` - status note
- `.claude/skills/spatial-audio-dsp/SKILL.md` - Woodworth section, Path B, libmysofa section, profile table, dependency lines
- `.claude/skills/spatialcore-architecture/spatialcore-architecture.md` - profile row, `renderSimpleBinauralWoodworth` row, loader line (CRLF file, kept)
- `.planning/PROJECT.md` - OSD Phase 3 follow-ups row, OSP Phase 3 follow-up row, interim custom-HRTF path on the custom-sofa-import row, one sentence under the table
- `.planning/REQUIREMENTS.md` - BUG-01 line references (no checkbox ticked)

## Decisions Made

See `key-decisions` in the frontmatter.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] spatial-audio-dsp skill profile table disagreed with the code**
- **Found during:** Task 2 (editing profile-table row 0)
- **Issue:** The table numbered KEMAR 1, SADIE 2, CIPIC 3, HUTUBS 4, Bernschuetz 5, where `kHRTFProfiles` (and the profile numbers consumers persist, D-07) is SADIE 1, CIPIC 2, HUTUBS 3, Bernschuetz 4, KEMAR 5. The skill auto-loads into every session, so a session could have mapped profile numbers to the wrong file.
- **Fix:** Renumbered the rows to `kHRTFProfiles`, keeping each dataset's license and character text, and added the one-call switch note.
- **Files modified:** `.claude/skills/spatial-audio-dsp/SKILL.md`
- **Verification:** compared by eye against `include/SpatialCore/Binaural/HRTFProfile.h`
- **Committed in:** `93a68d6`

**2. [Rule 1 - Bug] development-roadmap status note contradicted itself after the main fix**
- **Found during:** Task 1
- **Issue:** The plan replaced one phrase, but the same note's next sentence listed "BinaryData HRTF / rebuild all consumers" as historical and no longer accurate, which is false again once the data is embedded.
- **Fix:** Removed that phrase from the list.
- **Files modified:** `docs/development-roadmap.md`
- **Committed in:** `88af70e`

**3. [Rule 2 - Missing] PROJECT.md "release note" check needed a second use of the phrase**
- **Found during:** Task 2 acceptance check
- **Issue:** The Phase 2 row says "Release notes" (capital R), so the case-sensitive `grep -c "release note"` the plan prescribes would have returned 1.
- **Fix:** Added one sentence under the table saying the "ships with the migration" rows must reach OSD's release note or its migration commit. It is useful on its own and makes the count 2.
- **Files modified:** `.planning/PROJECT.md`
- **Committed in:** `93a68d6`

---

**Total deviations:** 3 auto-fixed (2 bug, 1 missing). **Impact on plan:** documentation only; no scope creep, no code touched.

## Issues Encountered

- A first attempt to wrap the Task 2 `sed` wording fix failed on BSD sed (`extra characters at the end of d command`); redone with a small script. No file was damaged.
- The `[binaural][sc12]` assertions are not in OpenSpatialPanner's `main` checkout. They are in `Tests/DevFormatTests.cpp` inside its development worktrees (`.claude/worktrees/...`), as CONTEXT said (consumer-side, branch-dependent). The follow-up row names the file and does not claim a branch. Nothing in OSP was modified.

## Authentication Gates

None.

## Deferred Items

- **`SharedFFTCache` "Leaked objects detected: N FFT" at process exit** (3 instances in the Debug full run, 1 in the KEMAR-only run): a false positive from the process-global FFT singleton, unrelated to Phase 3 behaviour. Stays with Phase 5 (Claude's Discretion). Exit status is 0 and every test passes.
- **OSD libmysofa pin** (v1.3.2) and the other cross-repo items live on the PROJECT.md "OSD Phase 3 follow-ups" row; none was done here.

## Known Stubs

None.

## Threat Flags

None. No network endpoint, auth path or file-access pattern was added (documents only). T-03-22 mitigated: every API and option name written into the docs was grep-checked against `include/` and `CMakeLists.txt` (`setHRTFProfile` 6 hits, `getHRTFProfileStatus` 2, `describeHRTFProfileStatus` 2, `engineSelectsHRTF` 4, `loadFromBinaryData` 1, `SPATIALCORE_EMBED_ALL_HRTF` and `SPATIALCORE_HRTF_DIR` present) and the stale strings negative-checked (`not BinaryData` 0, `NOT BinaryData` 0, `Contents/Resources/HRTF` 0, `No spectral coloring` 0, `background threaded (60Hz timer)` 0, `DirectBinauralAlgorithm.cpp:26` 0). T-03-23 mitigated: only the two non-installing OSD targets were built, OSD status empty before and after, plug-in folder digest unchanged.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

Ready for Plan 03-11 (the outward-facing GitHub actions, gated on the user). The five criteria have current evidence, so end-of-phase UAT can read the Phase gate section above.

---
*Phase: 03-binaural-defects-hrtf-packaging*
*Completed: 2026-10-04*

## Self-Check: PASSED

- Files: all eight modified documents, `/tmp/sc-0310-debug.log`, `/tmp/sc-consumer/main.cpp`, `/tmp/sc-consumer/run.log` and `/tmp/osd-dr3-check/build.log` exist.
- Commits: `88af70e` and `93a68d6` exist; `git log --grep="03-10"` returns both; `git status --porcelain` shows no modified tracked file.
- Task acceptance criteria re-run: Task 1 verify and its four acceptance greps pass; Task 2 verify and its three acceptance greps pass (`release note` count 2, `No spectral coloring` 0, `:26` 0); Task 3 acceptance (Debug log all passed with 0 assertion lines, Release 100%, OFF pass, consumer exit 0 with five lines, OSD build exit 0 and OSD status unchanged, criterion-by-criterion status present).
