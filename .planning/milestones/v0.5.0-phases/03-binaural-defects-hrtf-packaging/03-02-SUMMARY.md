---
phase: 03-binaural-defects-hrtf-packaging
plan: 02
subsystem: testing
tags: [catch2, binaural, hrtf, golden, fingerprint, tolerance, libmysofa]

requires:
  - phase: 03-binaural-defects-hrtf-packaging
    provides: "Plan 03-01 test foundation; Debug 220/221 (only HutubsPP2Tests.cpp:47 failing), Release 221/221"
provides:
  - "HRIRFingerprint, fingerprintHRIRPair, fingerprintAlignedHRIR, fingerprintMatches, printFingerprint in tests/Binaural/BinauralTestUtilities.h"
  - "Five per-profile goldens compared by tolerance fingerprint (kGoldenFingerprint) instead of a byte-exact FNV hash"
  - "Gain-change (x1.0001) and one-sample-shift negative controls inside each golden"
  - "Debug suite fully green (221/221), closing the Phase 2 deferred HUTUBS Debug failure"
affects: [03-08]

actuals:
  tokens: 5900
  tasks: 2
  commits: 2

plan_head_before: 12d692909a3a4ecef695774b63cf92b72d1c4518
plan_head_after: 5dd8622e15d6cd9121a16b23e345025b3b9aa4ba
commits: 2

tech-stack:
  added: []
  patterns:
    - "Golden = captured tolerance fingerprint (length, delays, per-ear energy, peak value and index) with provenance comment; reports which field moved and by how much"
    - "In-test negative controls proving a golden still discriminates (0.01% gain, one-sample shift)"

key-files:
  created: []
  modified:
    - tests/Binaural/BinauralTestUtilities.h
    - tests/Binaural/HutubsPP2Tests.cpp
    - tests/Binaural/SadieD2KU100Tests.cpp
    - tests/Binaural/CipicSubject003Tests.cpp
    - tests/Binaural/BernschuetzKU100Tests.cpp
    - tests/Binaural/MitKemarLargePinnaTests.cpp

key-decisions:
  - "Fingerprint tolerances fixed as planned (length and peak index exact, delays 1e-4 samples, energy and peak 1e-5 relative); no tolerance was widened because Debug and Release agreed for all five profiles"
  - "Golden test case titles changed from 'golden HRIR checksum' to 'golden HRIR fingerprint'; tags are untouched so tag filters still select them"
  - "fingerprintMatches result is computed before INFO(why) so the failing field is attached to the assertion message"

patterns-established:
  - "Golden values are captured from the tree, never chosen; any later move is a D-17 event"

requirements-completed: [EXTR-02]

coverage:
  - id: D1
    description: "HUTUBS PP2 golden passes in Debug and Release (the Phase 2 deferred Debug failure at HutubsPP2Tests.cpp:47 is closed)"
    requirement: "EXTR-02"
    verification:
      - kind: unit
        ref: "build/tests/SpatialCoreTests [binaural][hutubs] (Debug) and build-release/tests/SpatialCoreTests [binaural][hutubs] (Release)"
        status: pass
    human_judgment: false
  - id: D2
    description: "All five profile goldens (SADIE, CIPIC, HUTUBS, Bernschuetz, KEMAR) use one tolerance fingerprint with values captured from this tree, Debug and Release agreeing"
    requirement: "EXTR-02"
    verification:
      - kind: unit
        ref: "tests/Binaural/*Tests.cpp#golden HRIR fingerprint at az=90deg (5 test cases)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Each fingerprint still rejects a 0.01% gain change and a one-sample shift"
    requirement: "EXTR-02"
    verification:
      - kind: unit
        ref: "tests/Binaural/*Tests.cpp#SECTION negative control (10 sections across 5 files)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Full Debug suite has zero failures and Release ctest is 100% passed"
    verification:
      - kind: integration
        ref: "build/tests/SpatialCoreTests (221 test cases, exit 0); ctest --test-dir build-release/tests (221/221)"
        status: pass
    human_judgment: false

duration: 6min
completed: 2026-10-04
status: complete
---

# Phase 3 Plan 02: Binaural Golden Fingerprints Summary

**All five per-profile HRIR goldens now compare a tolerance fingerprint (delays, per-ear energy, peak value and index) instead of a byte-exact FNV hash, so they pass in Debug and Release and each proves it still rejects a 0.01% gain change and a one-sample shift**

## Performance

- **Duration:** 6 min
- **Started:** 2026-10-04T13:09:56Z
- **Completed:** 2026-10-04T13:15:51Z (task work; SUMMARY follows)
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

- Replaced the HUTUBS byte-hash golden (which failed in Debug only, by about one ulp on one sample) with a tolerance fingerprint, captured from this tree.
- Applied the same fingerprint to SADIE, CIPIC, Bernschuetz and KEMAR; Debug and Release fingerprints were identical to the printed precision for all four, and within 1.5e-7 relative for HUTUBS. No tolerance was widened.
- Added two in-test negative controls per golden (x1.0001 gain, one-sample shift), both asserting the fingerprint fails.
- Debug suite now 221/221 with exit 0, Release 221/221; the pre-existing Debug failure is gone.
- Fingerprint reports which field moved and both values, which gives Plan 03-08 (libmysofa v1.3.5) a before/after comparison a hash could not.

## Evidence

Red, before the change (Debug, `[binaural][hutubs][golden]`):

```
HutubsPP2Tests.cpp:47: FAILED:
  CHECK( checksum == kGoldenChecksum )
with expansion:
  8806157918509638672 (0x7a35c1f848c2a410)
  ==
  11402032843575911607 (0x9e3c2875eeade4b7)
test cases: 1 | 1 failed
```

The same tag passed in Release before the change (hash 0x9e3c2875eeade4b7).

Captured fingerprints at az=90, el=0 (field order: irLength, delayL, delayR, energyL, energyR, peakL, peakR, peakIndexL, peakIndexR), libmysofa v1.3.2, tree at 12d6929:

| Profile | Fingerprint |
|---|---|
| HUTUBS PP2 | `{ 279, 18f, 48f, 197.710969, 3.40356553, 6.14737415f, 0.711916804f, 27, 21 }` |
| SADIE D2 KU100 | `{ 256, 72f, 102f, 40.8401279, 0.798788257, 2.41877556f, 0.425458729f, 100, 102 }` |
| CIPIC 003 | `{ 218, 25f, 54f, 8.71853987, 0.107548025, 1.64910412f, -0.127475545f, 26, 42 }` |
| Bernschuetz KU100 | `{ 128, 11f, 43f, 5.42644277, 0.302691799, 1.26062512f, 0.268568307f, 11, 16 }` |
| MIT KEMAR Large Pinna | `{ 558, 31f, 61f, 2.00355748, 0.163567331, 0.515830636f, -0.121116042f, 39, 52 }` |

HUTUBS Debug vs Release (the only profile that differs at all): energyL 197.710969 vs 197.71094, energyR 3.40356553 vs 3.4035652, peakL 6.14737415 vs 6.14737368. Largest relative difference about 1.5e-7, roughly 70x under the 1e-5 tolerance.

## Task Commits

1. **Task 1: Tracer, HUTUBS golden as a tolerance fingerprint** - `04ecf6b` (test)
2. **Task 2: Same fingerprint for SADIE, CIPIC, Bernschuetz and KEMAR, full suites** - `5dd8622` (test)

**Plan metadata:** committed separately (docs: complete plan)

The tracer gate: the tracer `<verify>` is automated-only; it was re-run end to end in Debug and Release (`[binaural][hutubs]`, 18 assertions in 3 test cases, both green) before expanding to Task 2.

## Files Created/Modified

- `tests/Binaural/BinauralTestUtilities.h` - fingerprint struct and helpers; `fnv1aHash` and `hashHRIRPair` kept
- `tests/Binaural/HutubsPP2Tests.cpp` - golden by fingerprint, negative controls
- `tests/Binaural/SadieD2KU100Tests.cpp` - same; native-delay check kept
- `tests/Binaural/CipicSubject003Tests.cpp` - same
- `tests/Binaural/BernschuetzKU100Tests.cpp` - same
- `tests/Binaural/MitKemarLargePinnaTests.cpp` - same; `[itd-passthrough]` case kept

## Decisions Made

- Tolerances kept exactly as planned; Debug and Release agreement made widening unnecessary.
- Golden test case titles say "fingerprint" instead of "checksum"; tags unchanged. Comments that still described the golden as a checksum (SADIE, CIPIC) were reworded to match.
- The replaced FNV checksum value is recorded in each provenance comment so the old pin is not lost.

## Deviations from Plan

### Notes on plan text that did not match the tree

**1. Acceptance criterion "`[binaural][golden]` lists exactly 5 test cases" is stale**
- **Found during:** Task 2 verification
- **Issue:** Plan 03-01 added three Woodworth fallback tests tagged `[binaural][golden][woodworth]`, so the filter now selects 8 cases. The five profile goldens are all present and pass (listed by title above); the three extra are Plan 03-01's.
- **Fix:** None needed in code; the intent (all five profile goldens still selected by `[binaural][golden]`) holds. Plan 03-08 should expect 8, or filter `[binaural][golden]~[woodworth]` for the five.
- **Files modified:** none

---

**Total deviations:** 0 auto-fixed. One plan-text discrepancy noted above.
**Impact on plan:** None. Only test files changed; `git diff --stat 12d6929..HEAD -- src include CMakeLists.txt tests/CMakeLists.txt` is empty.

## Issues Encountered

- `gsd_run` is not on PATH in this shell; `node ~/.claude/gsd-core/bin/gsd-tools.cjs` was used for the protected-branch check and state updates.
- Debug runs print the expected libmysofa traces and a `juce_LeakedObjectDetector.h` assertion line at exit (3 FFT instances); exit status is 0 and the run reports all passed.

## Known Stubs

None.

## Threat Flags

None. T-03-03 (tampering with the golden verdict) is mitigated as planned: tolerances about 100x the measured build-type difference, values captured not chosen, negative controls in each golden.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Plan 03-03 onward starts from a fully green Debug suite (221/221) and Release (221/221).
- Plan 03-08 can compare field-by-field against `kGoldenFingerprint` in each profile file; any move is governed by D-17.

## Self-Check

Run after writing; result appended below.

## Self-Check: PASSED

All six modified test files exist; commits 04ecf6b and 5dd8622 found in git log; acceptance greps (no kGoldenChecksum, kGoldenFingerprint present, fingerprintMatches present) and plan verification (Debug full 221/221, Release ctest 221/221) re-run and passing.
