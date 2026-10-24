---
phase: 03-binaural-defects-hrtf-packaging
fixed_at: 2026-10-04T23:30:00Z
review_path: .planning/phases/03-binaural-defects-hrtf-packaging/03-REVIEW.md
iteration: 3
findings_in_scope: 2
fixed: 2
skipped: 0
status: all_fixed
---

# Phase 3: Code Review Fix Report

**Fixed at:** 2026-10-04T23:30:00Z
**Source review:** .planning/phases/03-binaural-defects-hrtf-packaging/03-REVIEW.md
**Iteration:** 3

**Summary:**
- Findings in scope: 2
- Fixed: 2
- Skipped: 0

## Fixed Issues

### IN-01: Custom-file limits documentation misstates the shipped IR length and omits the combined float budget

**Files modified:** `include/SpatialCore/Binaural/HRTFDatabase.h`, `docs/integration-guide.md`
**Commit:** 0636937
**Applied fix:** Every number was re-derived by probing the five shipped files with libmysofa (R=2 throughout) and applying `ceil (N * host / declared)`. The header now says KEMAR (profile 5) has the longest IR, 512 samples at its native 44.1 kHz, 558 at 48 kHz and about 2.2k at 192 kHz, and that Bernschuetz has the most positions (16020). The largest shipped decoded size at 192 kHz is stated as about 18 million floats (SADIE II, 8802 positions, 1024 samples) with Bernschuetz about 16 million; the hypothetical 77.8M figure is gone. The header's old "about 2.4k samples at 192 kHz" was also wrong (the probe gives 2230 for KEMAR) and was corrected in the same comment. The integration guide now states the combined bound (positions x resampled length x 2 <= 134 million floats, so 65536 positions allow at most 1024 samples and a 16384-sample IR allows at most 4096 positions) and replaces "the longest is 558 samples" with the host-rate-qualified figures. Documentation and comment only.

### IN-02: POSIX open lacks O_NOCTTY

**Files modified:** `src/Binaural/HRTFDatabase.cpp`
**Commit:** 5d2a92e
**Applied fix:** `readFileCapped` now opens with `O_RDONLY | O_NONBLOCK | O_NOCTTY` (plus the existing `O_CLOEXEC`), with a comment saying why. `<fcntl.h>` was already included.

## Verification

Verification ran in the main checkout (branch `gsd-remap`, no worktree), not an isolated worktree. Debug `build/` and Release `build-release/` both ran `SpatialCoreTests` to 266 of 266 test cases passed, 309483 assertions. The Debug run prints the known `juce_LeakedObjectDetector` FFT assertion at exit (SharedFFTCache false positive, present before these changes; exit status is unaffected). The consumer scratch build `/tmp/osd-dr3-check` (`OpenSpatialDelay`, `OpenSpatialDelayTests`) built with 0 errors.

---

_Fixed: 2026-10-04T23:30:00Z_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 3_
