# Phase 7 close-out (prepared 2026-10-25)

This file holds the outward actions of Phase 7. Plan 07-04 prepared it; sections 2 and 3 were run on 2026-10-25 after PR #51 merged as `3a03e2b` (#42, #37 and #13 closed; notes posted on OpenSpatialPanner#4 and openspatialdelay#27).

## 1. Run after the merge to main

These steps run only after Phase 7 is merged to `main`, with the go-ahead of the user (D-14, D-BP), normally from `/gsd-ship`.

Replace `<merge-sha>` with the short merge commit on `main`. Read it with `git rev-parse --short origin/main` after the merge.

None of the commands below starts a GitHub Actions run. Each command uses a single-quoted body, so the bodies contain no apostrophes.

## 2. Close the SpatialCore issues (D-14)

```
gh issue close 42 -R AndrewRahman/SpatialCore --comment 'Done in <merge-sha> (Phase 7). SpatialCore now runs its tests in tiers chosen by the moment (decision P7-D01): no test run for a doc-only change, the light tier (Release build plus the tests of the changed area) while iterating, the full Release ctest before merging or pushing to main, and the full tier (Debug, [rt], build-tsan, coverage, the consumer harness, the speaker-rig and DAW checks, a manual CI run) only when preparing a release. The tiers and commands are written once, in README "Local test gate"; CLAUDE.md, the integration guide and the handoff Done-when lists point at it. Landing order and handoffs are decision P7-D02 (README "Updating SpatialCore in a consumer"). Every doc that said a tag push starts CI now says CI runs only when started by hand (workflow_dispatch since PR #46).'
```

```
gh issue close 37 -R AndrewRahman/SpatialCore --comment 'Done in <merge-sha> (Phase 7): docs/development-roadmap.md clones https://github.com/AndrewRahman/SpatialCore.git and states 23 formats and 15 layouts (banner kept and dated); BinauralGains.h and SourcePosition.h cite commit 0b35e55 instead of a missing .planning path; the spatial-audio-dsp skill row is a dated 12-source figure, not re-measured for 32 sources; the scratch-guard test is now titled "...skips, counts and outputs silence" with matching comments.'
```

```
gh issue close 13 -R AndrewRahman/SpatialCore --comment 'Done in <merge-sha> (Phase 7): the PresetBrowser contract is documented in include/SpatialCore/UI/PresetBrowser.h and in docs/integration-guide.md "PresetBrowser contract". PresetBrowser is menu chrome only: a consumer-supplied Entry list in, originalIndex out, and a save overlay that hands back a trimmed name. It stores no state, so the state format, save and load, and versioning belong to the consumer. ROADMAP Phase 11 (#2) builds the OpenSpatialPanner state format on this contract.'
```

## 3. Consumer notes (D-06)

Same note on both consumer handoff issues. SpatialCore makes no edit in either repo.

```
gh issue comment 4 -R AndrewRahman/OpenSpatialPanner --body 'Note from SpatialCore Phase 7 (<merge-sha>): SpatialCore now runs its tests in tiers (decision P7-D01; README "Local test gate"): the light tier while iterating, the full Release ctest before main, and the full tier only before a release. SpatialCore handoffs to this repo now ask for the light tier in their Done-when unless they are release prep (P7-D02). Whether this repo adopts the same tiers is decided here; SpatialCore makes no edits in this repo.'
```

```
gh issue comment 27 -R AndrewRahman/openspatialdelay --body 'Note from SpatialCore Phase 7 (<merge-sha>): SpatialCore now runs its tests in tiers (decision P7-D01; README "Local test gate"): the light tier while iterating, the full Release ctest before main, and the full tier only before a release. SpatialCore handoffs to this repo now ask for the light tier in their Done-when unless they are release prep (P7-D02). Whether this repo adopts the same tiers is decided here; SpatialCore makes no edits in this repo.'
```

## 4. Already closed (D-12, D-13)

Read by plan 07-04 on 2026-10-25 at 08:46 UTC:

- SpatialCore #24: CLOSED. D-12: openspatialdelay#23 is closed, and OpenSpatialDelay main pins 2f16bae and reads getBlockLayout(). Nothing to redo.
- SpatialCore #9: CLOSED. D-13: SpatialCore CI runs 39058330902 and 39074814315 were green on JUCE 9.0.0; the OpenSpatialPanner CI check stays in OpenSpatialPanner#4. Nothing to redo.

At the same time #42, #37 and #13 read OPEN.

## 5. Not part of the close-out

- No CI run. A release-candidate run is Phase 13, started by hand, with the go-ahead of the user.
- No edits in the OpenSpatialDelay or OpenSpatialPanner repos.
- This file contains no command that dispatches a workflow, pushes, or creates a tag.
