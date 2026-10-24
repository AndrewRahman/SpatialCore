# Phase 06 deferred items

## One-off failure of the profile-switch churn test (found in 06-10, 2026-10-24)

- **Test:** `Profile switch: every switch completes while a paced render thread runs, claims and frees included` (`[hrtf-switch][churn]`, `tests/Engine/ProfileSwitchTests.cpp`, checks at lines 952 and 960).
- **What happened:** in the first full Debug `SpatialCoreTests` run after merging `origin/main`, 15 of 16 switches completed. The last request (profile 1, SADIE) never became active within the test's 20 s wait, so `peakByProfile[1]` stayed 0. No non-finite sample, largest sample 1.3077, slowest `setHRTFProfile` call 0.025 ms.
- **Not reproduced:** 3 + 10 single Debug runs, 10 single Release runs, 2 runs of `[hrtf-switch]` (25 cases), a second full Debug run (401 cases, 0 failed) and the Release ctest (418 of 418) all passed 16 of 16.
- **Cause:** not found. It may be a load-timing effect in the full Debug run, or a lost last request in the engine's profile loader. The merge (SC-20 single-load fix touches `renderBlock` only) is not shown to be the cause or to be clear of it.
- **Action taken:** none to the code or the test; nothing loosened. If it shows up again, capture the log of the failing run (the status after the timeout would say whether the load failed or never started) before touching anything.
- **Log of the failing run:** `/tmp/sc-debug-tests-run1-failed.log` (temporary file, may be gone).
