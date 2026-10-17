# Phase 5: Realtime Safety Hardening - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-10-05
**Phase:** 05-realtime-safety-hardening
**Areas discussed:** How the safety proof runs, What a 'can't happen' case does, Multi-instance proof depth, Old HRTF loading route

---

## How the safety proof runs

| Option | Description | Selected |
|--------|-------------|----------|
| Separate check, every phase gate | Third build (optimised + race checker) and no-allocation test at each phase end gate and before releases | ✓ |
| Every test run | Folded into the normal gate; adds 10-20 minutes per gate | |
| Once, for Phase 5 only | Run once, not kept as a standing check | |

| Option | Description | Selected |
|--------|-------------|----------|
| Both, belt and braces | Test-only allocation trap plus Clang realtime checker where supported | |
| Test-only guard | Own allocation trap only | |
| You decide | Claude picks after research | ✓ |

| Option | Description | Selected |
|--------|-------------|----------|
| Every output format and switch | All 5 paths, 23 formats, profile/format switches, trajectories | ✓ |
| One representative per path | Faster, could miss a format-specific allocation | |

## What a 'can't happen' case does

| Option | Description | Selected |
|--------|-------------|----------|
| Keep last good sound | Skip the update; source keeps previous HRTF filter | ✓ |
| Silence that source for the block | Simple, may click | |
| You decide | Claude picks per site | |

| Option | Description | Selected |
|--------|-------------|----------|
| Count it, assert in Debug | Lock-free counter + Debug assert (SC-17 pattern) | ✓ |
| Debug assert only | Silent in shipped builds | |

## Multi-instance proof depth

| Option | Description | Selected |
|--------|-------------|----------|
| Automated stress test only | Many engines in one process under the race checker | ✓ |
| Stress test + real DAW check | Also OSD loaded several times in Reaper | |

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, include UI | Several SpatialMapComponents created and painted at once | ✓ |
| No, audio only | Treat #131 as OSD-side | |

| Option | Description | Selected |
|--------|-------------|----------|
| Show it fails on the old code | Reintroduce old bug, show failure, restore | ✓ |
| Passing test is enough | Prove today's code passes | |

## Old HRTF loading route

| Option | Description | Selected |
|--------|-------------|----------|
| Keep, document, detect misuse | Stays working; thread contract documented; misuse counter + Debug assert | ✓ |
| Make it fully race-proof | Internal safety even for misuse | |
| Remove it now | Breaking change, major bump | |

| Option | Description | Selected |
|--------|-------------|----------|
| No — must be bit-identical | Prove against Phase 3 goldens; stop and ask if impossible | ✓ |
| Small change OK if inaudible | Re-baseline within tolerance | |

## Claude's Discretion

- Allocation detector mechanism (test-only trap, RealtimeSanitizer, or both).
- RTSF-02 fix shape (atomic floats vs atomically swapped struct).
- Counter names and shared helper location.

## Deferred Ideas

- Real-DAW multi-instance check — with the OSD migration.
- Removing the legacy HRTF route — next major.
- Sanitizer build in CI — Phase 6.
