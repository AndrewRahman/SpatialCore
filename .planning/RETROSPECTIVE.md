# Project Retrospective

*A living document updated after each milestone. Lessons feed forward into future planning.*

## Milestone: v0.5.0 — SpatialCore extraction milestone

**Shipped:** 2026-10-24
**Phases:** 6 | **Plans:** 52 | **Sessions:** not tracked

### What Was Built
- A frozen count contract (8 algorithms, 23 formats, 15 layouts, 5 HRTF profiles) held by `static_assert` and a `[counts]` test.
- All 8 algorithms verified against independent oracles; ITU-R BS.2127 below-horizon VBAP; textbook VBIP; corrected SN3D at orders 4-6.
- Embedded HRTF data, one-call `setHRTFProfile`, sample-based crossfades, the Simple-path cue bank.
- ADM-OSC with a self-clocked 30 Hz sender and query replies, 13 trajectory shapes, lock-free final positions, a headless-tested spatial map.
- A realtime-safety proof (allocation and lock trap, ThreadSanitizer) and opt-in facade flags (`engineComputesGains`, `engineDerivesDispatch`, `engineSelectsHRTF`, `engineFadesFormatSwitch`).
- A consumer harness, macOS/Linux/Windows CI, coverage tooling with a 93% engine rule.

### What Worked
- Independent oracles (scipy, PyPI ear, textbook formulas) turned "looks right" into measured claims.
- Opt-in flags kept every change backward compatible for OpenSpatialDelay.
- Writing decisions down (D-numbers, DR-18) kept later phases from re-litigating them.
- Triage of every open issue into a phase or v2 left no orphan requirement (25/25).

### What Was Inefficient
- The first roadmap was planned against a branch missing 42 commits and rested on documentation counts that were wrong (25 formats / 14 layouts); Phase 1 existed to repair that.
- Phases 2-5 verification fingerprints went stale as later phases touched the same files, and refreshing them needs builds.
- Deferred-items files accumulated 10 bullets that were already fixed; they were only reconciled at close.
- The 12-slot to 32-slot change (SC-1) left stale "12 sources" text and a 12-colour map palette (SpatialCore#31).

### Patterns Established
- Count contracts live in code and fail the build when out of date.
- Every engine feature that changes behaviour is opt-in and default-off until the consumer migrates.
- Heavy gates (Debug run, coverage, TSan, GitHub CI) run only for a release candidate; the everyday gate is Release build plus Release ctest (D-18, D-19, D-20).
- Milestone label and library version match: v0.5.0 (labelled v1.0.0 until 2026-10-25; milestone labels stay 0.x until the first public release).

### Key Lessons
1. Verify a documented number against the tree before planning on it.
2. A verification report goes stale when later phases edit the same code; record that as layering, and do not refresh a fingerprint without re-checking.
3. Close deferred items when they are fixed, or the milestone close has to reconcile them.
4. Keep milestone labels from implying a release; state the library version next to them.

### Cost Observations
- Model mix: not tracked
- Sessions: not tracked
- Notable: 586 commits in the repo since 2026-03-22; the milestone's 52 plans ran 2026-08 to 2026-10.

---

## Cross-Milestone Trends

### Process Evolution

| Milestone | Sessions | Phases | Key Change |
|-----------|----------|--------|------------|
| v0.5.0 | n/a | 6 | Planning re-based on verified tree; heavy gates moved to release time (D-18/19/20) |

### Cumulative Quality

| Milestone | Tests | Coverage | Zero-Dep Additions |
|-----------|-------|----------|-------------------|
| v0.5.0 | 418 (Release, merged tree) | 94.19% engine (CI, 2026-10-17); UI 25.89% report-only | n/a |

### Top Lessons (Verified Across Milestones)

1. (Single milestone so far; none yet verified across milestones.)
