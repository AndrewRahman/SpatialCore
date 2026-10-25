# Phase 7: Process, docs and consumer close-out - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-10-25
**Phase:** 07-process-docs-consumer-closeout
**Areas discussed:** Everyday test tier, Issue #9 closure

---

## Everyday test tier

| Option | Description | Selected |
|--------|-------------|----------|
| Split | While iterating: build plus the changed area's tests; before merging to main: full Release suite (~2 min) | ✓ |
| Changed area only | Only the changed area's tests, even before merging; full suite only before a release | |
| Keep full Release suite | No change: ~2 min on every change | |

**User's choice:** Split (recommended)

---

## Issue #9 closure

| Option | Description | Selected |
|--------|-------------|----------|
| Close now | Accept SpatialCore's own green JUCE 9.0.0 runs; OSP's CI failure stays in OSP#4; no GitHub run | ✓ |
| Leave open | Keep open until OSP's CI runs green (needs a GitHub run, asked first) | |

**User's choice:** Close now (recommended)

---

## Claude's Discretion

- #24 closes now: openspatialdelay#23 is closed and OSD main reads `getBlockLayout()` at pin 2f16bae (checked 2026-10-25).
- #13: `PresetBrowser` is menu chrome with no state; document the contract, state format stays with Phase 11 (#2).
- #37 skill figure: mark dated or remove, no re-measure.
- CI wording: fix every "tag push starts CI" surface; ci.yml is `workflow_dispatch` only since PR #46.
- Cross-repo order unchanged (SpatialCore first, consumer pin bump after); handoffs as consumer-repo issues.

## Deferred Ideas

- Re-measure 32-source processBlock time.
- OSD/OSP adopting the tiers in their own repos.
- OSP's failing main CI (OpenSpatialPanner#4).
