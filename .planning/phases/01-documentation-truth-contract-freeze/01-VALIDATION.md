---
phase: 1
slug: documentation-truth-contract-freeze
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: validated
nyquist_compliant: true
wave_0_complete: true
created: 2026-08-11
validated: 2026-10-17
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7.1 (FetchContent) [VERIFIED: `tests/CMakeLists.txt:2-7`] |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cmake --build build --target SpatialCoreTests && ./build/tests/SpatialCoreTests "[counts],[outputlimiter],[bug03]"` |
| **Full suite command** | `cmake --build build --target SpatialCoreTests && ./build/tests/SpatialCoreTests` |
| **Estimated runtime** | a few minutes full suite (326 `TEST_CASE`s on 2026-10-17); ~2 seconds tag-filtered |

---

## Sampling Rate

- **After every task commit:** Run `./build/SpatialCoreTests "[spatialmath]"` for limiter/counts
  edits; run `grep -rn "issue #[0-9]" include/ src/` after each BUG-03 comment-rewrite batch.
- **After every plan wave:** Run `cmake --build build --target SpatialCoreTests && ./build/SpatialCoreTests`
- **Before `/gsd-verify-work`:** Full suite must be green (DR-4, pre-existing project rule)
- **Max feedback latency:** 45 seconds

**Compile-time sampling note (D-10):** the `static_assert` count sentinels are validated by the
build itself, not by a test run. A wrong count literal fails `cmake --build` before any test
executes — the fastest feedback loop in this phase. Treat a successful build as a passed
validation step for API-01/API-04, with the runtime `[counts]` test as the DR-4 backstop.

---

## Per-Task Verification Map

> Task IDs are seeded by the planner. Rows below map phase **requirements** to their verification
> command; the executor fills `Task ID` as plans are authored.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 01-01-T1, T2 | 01 | 1 | API-01 | — | N/A | compile-time `static_assert` + unit | `./build/tests/SpatialCoreTests "[counts]"` | ✅ `tests/Core/CountsTests.cpp` | ✅ green |
| 01-01-T1, T2; 01-05-T2 | 01, 05 | 1, 3 | API-04 | — | N/A | compile-time `static_assert` + unit | `./build/tests/SpatialCoreTests "[counts]"` | ✅ `tests/Core/CountsTests.cpp` | ✅ green |
| 01-04-T1 | 04 | 3 | API-02 | — | N/A | unit | `./build/tests/SpatialCoreTests "[outputlimiter]"` | ✅ `tests/Core/SpatialMathTests.cpp` (6 named cases) | ✅ green |
| 01-03-T1..T3 | 03 | 2 | API-03 | — | N/A | unit (profile table) + doc phrasing (manual) | `./build/tests/SpatialCoreTests "[hrtf-embed][table]"` | ✅ `tests/Binaural/HRTFEmbeddedTests.cpp` (added Phase 3) | ✅ green |
| 01-03-T1..T3; 01-05-T1, T2 | 03, 05 | 2, 3 | API-05 | — | N/A | doc review | — (manual, see below) | N/A | manual-only |
| 01-02-T1, T2 | 02 | 2 | BUG-03 | — | N/A | source-tree scan | `./build/tests/SpatialCoreTests "[bug03]"` | ✅ `tests/Core/IssueReferenceTests.cpp` (added 2026-10-17) | ✅ green |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

**API-04 numeric contract (settled 2026-08-11):** the sentinels assert **23** output formats and
**15** speaker layouts. The prior 25/14 pair was a mapper miscount and is not the contract — see
CONTEXT.md D-04 and RESEARCH.md Open Question 1 (closed).

---

## Wave 0 Requirements

- [x] A counts `TEST_CASE` (new, tag `[counts]`) asserting the algorithm-count, output-format-count,
      and `NUM_LAYOUT_DEFS` sentinels at runtime as a DR-4 backstop to the `static_assert`s —
      covers API-01 and API-04.
- [x] Decision recorded (plan 01-04: split into named cases) on whether `outputLimiter`'s existing parameterized test at
      `tests/Core/SpatialMathTests.cpp:66-98` already satisfies Success Criterion 2 as evidence, or
      needs splitting into explicitly-named at/below/above-ceiling and non-finite cases — covers
      API-02. Existing loop already spans `{-5, -1.2589, -0.5, 0, 0.5, 1.2589, 2, 5, 100}`.
- [x] No framework install needed — Catch2 already wired into `tests/CMakeLists.txt`.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| CLAUDE.md's factual claims are reproducible from the tree | API-05 | Prose cannot be Catch2-asserted | Walk RESEARCH.md's line-by-line Tier A table; confirm each cited line now matches the tree (JUCE 9.0.0, `Engine/` row present, HRTF loads from disk, no phantom `DSP/` row, counts 23/15) |
| HRTF profile count stated with the two-number D-05 phrasing | API-03 | Phrasing requirement, not a value check | Confirm each doc surface says "5 SOFA HRTF profiles ship; `profileIndex` is 0–5, where 0 = Simple (Woodworth) and 1–5 select the SOFA profiles" |
| Every qualified `#NNN` resolves in the tracker it names | BUG-03 | The *form* is now automated by `[bug03]`; whether the number exists upstream needs a network lookup | Spot-check new `Spatial-Media-Lab/OpenSpatialDelay#NNN` references with `gh issue view NNN -R Spatial-Media-Lab/OpenSpatialDelay` (234 and 100 checked 2026-10-17) |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies (API-05 prose is manual-only by design)
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 45s (tag-filtered)
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** validated 2026-10-17 (`/gsd-validate-phase 1`)

## Validation Audit 2026-10-17

| Metric | Count |
|---|---|
| Gaps found | 1 |
| Resolved | 1 |
| Escalated | 0 |
