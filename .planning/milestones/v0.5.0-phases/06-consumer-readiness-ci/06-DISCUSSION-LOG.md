# Phase 6: Consumer Readiness & CI - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in 06-CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-10-17
**Phase:** 06-consumer-readiness-ci
**Areas discussed:** Test plugin shape, Gains safety (INTG-02), CI scope, Coverage target

---

## Test plugin shape

| Question | Options | Selected |
|----------|---------|----------|
| Where does the harness live? | In this repo via real submodule (rec) / Separate repo / In this repo, local folder | In this repo via submodule |
| Umbrella header vs UI headers | Two headers: main + new UI umbrella (rec) / UI into main header / Amend wording only | Two headers |
| Use the five opt-in features? | Yes, all five (rec) / Minimal | Yes, all five |
| Sound check or build only? | Build + headless auto sound check (rec) / Build only / Build + REAPER | Build + auto sound check |

## Gains safety (INTG-02)

| Question | Options | Selected |
|----------|---------|----------|
| How to stop the trap | Keep off, detect and report (rec) / Default on / Both | Keep off, detect and report |
| Release behaviour | Count only, keep playing (rec) / Fall back to computing | Count only |
| Other opt-in flags | Gains only + documented set (rec) / One-call helper | Gains only + docs |

## CI scope

| Question | Options | Selected |
|----------|---------|----------|
| Machines | Mac + Linux tested, Windows build-only (rec) / Mac only / All three tested | Mac + Linux tested, Windows build-only |
| Per-push macOS checks | Debug+Release+UI+harness (rec) / plus TSan+rt / Release only | Debug+Release+UI+harness |
| TSan + rt cadence | Nightly + on demand (rec) / By hand / Every push | Nightly + on demand |
| Block merging on red CI | Require green (rec) / Advisory | Require green |

**Notes:** The first "Machines" answer was "I don't fully understand this; I always have a Mac but not Windows or Linux." Re-asked in plain terms: CI runs on GitHub's free hosted machines (the repo is public), so the user never needs Windows or Linux.

## Coverage target

| Question | Options | Selected |
|----------|---------|----------|
| How to pick the target | Measure first, user picks mid-phase (rec) / No-drop rule / Fixed number now | Measure first, pick mid-phase |
| CI gate | Fail below target (rec) / Report only | Fail below target |
| Where reports show | Inside GitHub only (rec) / External service | Inside GitHub only |
| Count UI code | Yes, separate number (rec) / Engine only | Yes, separate number |

## Claude's Discretion

- Coverage tool, harness folder name and target type, D-05 detection rule and counter name, runner images and caching, Windows LFS handling.
- The missing OSD#50 test and the red-first re-check of #89/#96/#131/#234 (user agreed to Phase 5's D-07 rule).

## Deferred Ideas

- Full Windows test runs; detectors for other flags or a one-call helper; external coverage service or badge; real-DAW multi-instance check (OSD repo).
