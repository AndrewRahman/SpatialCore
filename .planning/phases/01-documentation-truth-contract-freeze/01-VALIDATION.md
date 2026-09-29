---
phase: 1
slug: documentation-truth-contract-freeze
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-08-11
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Catch2 v3.7.1 (FetchContent) [VERIFIED: `tests/CMakeLists.txt:2-7`] |
| **Config file** | `tests/CMakeLists.txt` |
| **Quick run command** | `cmake --build build --target SpatialCoreTests && ./build/SpatialCoreTests "[spatialmath]"` |
| **Full suite command** | `cmake --build build --target SpatialCoreTests && ./build/SpatialCoreTests` |
| **Estimated runtime** | ~45 seconds full suite (144 `TEST_CASE`s); ~5 seconds tag-filtered |

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
| TBD | 01 | 0 | API-01 | — | N/A | unit (compile-time + Catch2) | `./build/SpatialCoreTests "[counts]"` | ❌ W0 — new counts test | ⬜ pending |
| TBD | 01 | 0 | API-04 | — | N/A | unit (compile-time + Catch2) | `./build/SpatialCoreTests "[counts]"` | ❌ W0 — new counts test | ⬜ pending |
| TBD | 01 | 1 | API-02 | — | N/A | unit | `./build/SpatialCoreTests "[outputlimiter]"` | ✅ `tests/Core/SpatialMathTests.cpp:66-98` | ⬜ pending |
| TBD | 01 | 1 | API-03 | — | N/A | doc + indirect unit | `./build/SpatialCoreTests "[binaural]"` | ✅ 5 per-profile test files present | ⬜ pending |
| TBD | 01 | 1 | API-05 | — | N/A | doc review | — (manual against RESEARCH.md line table) | N/A | ⬜ pending |
| TBD | 01 | 1 | BUG-03 | — | N/A | grep verification | `grep -rn "issue #[0-9]" include/ src/` → zero bare `#N` | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

**API-04 numeric contract (settled 2026-08-11):** the sentinels assert **23** output formats and
**15** speaker layouts. The prior 25/14 pair was a mapper miscount and is not the contract — see
CONTEXT.md D-04 and RESEARCH.md Open Question 1 (closed).

---

## Wave 0 Requirements

- [ ] A counts `TEST_CASE` (new, tag `[counts]`) asserting the algorithm-count, output-format-count,
      and `NUM_LAYOUT_DEFS` sentinels at runtime as a DR-4 backstop to the `static_assert`s —
      covers API-01 and API-04.
- [ ] Decision recorded on whether `outputLimiter`'s existing parameterized test at
      `tests/Core/SpatialMathTests.cpp:66-98` already satisfies Success Criterion 2 as evidence, or
      needs splitting into explicitly-named at/below/above-ceiling and non-finite cases — covers
      API-02. Existing loop already spans `{-5, -1.2589, -0.5, 0, 0.5, 1.2589, 2, 5, 100}`.
- [ ] No framework install needed — Catch2 already wired into `tests/CMakeLists.txt`.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| CLAUDE.md's factual claims are reproducible from the tree | API-05 | Prose cannot be Catch2-asserted | Walk RESEARCH.md's line-by-line Tier A table; confirm each cited line now matches the tree (JUCE 9.0.0, `Engine/` row present, HRTF loads from disk, no phantom `DSP/` row, counts 23/15) |
| HRTF profile count stated with the two-number D-05 phrasing | API-03 | Phrasing requirement, not a value check | Confirm each doc surface says "5 SOFA HRTF profiles ship; `profileIndex` is 0–5, where 0 = Simple (Woodworth) and 1–5 select the SOFA profiles" |
| Every `#NNN` resolves in the tracker it names | BUG-03 | Requires cross-repo issue lookup | `grep -rn "issue #[0-9]" include/ src/` must return zero bare `#N`; spot-check the rewritten `Spatial-Media-Lab/OpenSpatialDelay#NNN` form with `gh issue view` |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 45s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
