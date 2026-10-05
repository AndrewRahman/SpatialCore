# Phase 4: Control Surface & UI - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-10-05
**Phase:** 04-control-surface-ui
**Areas discussed:** How the map gets checked, Proving fonts without installed fonts, OSC broadcast timing and silence, "Reaches the renderer" bar

---

## How the map gets checked

| Option | Description | Selected |
|--------|-------------|----------|
| Both: automated tests + demo app | Fake drags checked every run, plus screenshots | ✓ |
| Automated tests only | No screenshots | |
| Demo app + screenshots only | Roadmap's "by interaction"; one-off | |
| Inside OpenSpatialDelay only | Tests OSD's copy, manual | |

Demo app location: **in this repo, off by default** (vs throwaway / you decide).
Screenshot review: **yes, one review** (vs numbers only).
Elevation drag: **keep as-is, no elevation drag in v1** (vs add it).

## Proving fonts without installed fonts

| Option | Description | Selected |
|--------|-------------|----------|
| Automated test + code rule | Typefaces must come from embedded data; no by-name lookups | ✓ |
| Clean macOS user account | One-off screenshot, needs admin password | |
| Temporarily move the fonts away | Touches user's font folder | |
| Automated test + clean-user screenshot | Strongest, most effort | |

No look-and-feel set: **map uses SML fonts on its own** (vs document only / you decide).

## OSC broadcast timing and silence

30 Hz clock: user asked for a plain-language explanation first; then chose **sender keeps its own stopwatch** (vs document "tick at 60 Hz").
Still objects: user said "whatever the standard for these types of connections is". Research: ADM-OSC uses query/response (no-argument message → reply on port 4002), no heartbeat. Re-asked; chose **answer position queries + send all on connect** (vs send-on-connect only / full query support for every message).
Zero bug: **fix it** (vs pin).
Proof: **automated test with a fake clock** (vs plus real network capture).

## "Reaches the renderer" bar

| Option | Description | Selected |
|--------|-------------|----------|
| The sound actually moves | Render audio, check left channel louder | ✓ |
| The position number arrives | Value-only check | |

Glue: **no library connector; tests and demo app wire it** (vs add a shared connector).
Reverse proof: **same path, opposite direction, point by point; Random only "moves and in range"** (vs moves-and-differs).
If reverse is wrong: **fix it, OSD release note** (vs pin / ask with before-after).

## Claude's Discretion

UI test target structure, demo app naming and CMake option, sender clock injection, query-reply data source and return port, "sound moved" thresholds and formats.

## Deferred Ideas

Elevation drag; shared OSC/trajectory/map→engine connector; full ADM-OSC query support beyond positions; ~50 Hz broadcast rate.
