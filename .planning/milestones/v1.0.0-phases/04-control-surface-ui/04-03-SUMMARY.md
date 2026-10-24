---
phase: 04-control-surface-ui
plan: 03
subsystem: osc
tags: [adm-osc, juce_osc, query, reply, input-validation, catch2, udp, wunused-parameter]

requires:
  - phase: 04-control-surface-ui
    provides: "04-02 self-clocked 30 Hz ADMOSCSender (tick with nowSeconds) and the RecordingCapture test helper"
provides:
  - "ADMPositionQuery { azim, elev, dist, aed, xyz } and ADMOSCReceiver::Listener::admPositionQueried (defaulted)"
  - "ADMOSCSender::queueReply (int, ADMPositionQuery, float, float, float): coalesced per (object, kind), flushed on the 30 Hz slot to the configured destination"
  - "ADMOSCReceiver input hardening: type-checked, finite, clamped, bounded azimuth wrap (D-21)"
  - "tests/tools/check_unused_params.py, the D-15 check (reused by Plan 04-08's gate)"
affects: [04-07, 04-08]

plan_head_before: 3ec206a6c235ba48dea8ef3bc15df1a6f2f6d14a
plan_head_after: 088621aec232e2050ac0193c0fe07072c1433bed

actuals:
  tokens: 12500
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "Query/reply split: the receiver reports a query to the Listener, the consumer answers from its own state through the sender, so the receiver keeps no position store (DR-16)"
    - "Pending-reply table: a fixed MAX_SOURCES x 5 array of {flag, a, b, c}, latest wins, flushed in object then kind order on the send slot, no allocation"
    - "Checked argument readers (readFloat, readNumeric): a typed getter is called only after the OSC type and finiteness are known"
    - "D-15 check as a script: recompile one including TU from compile_commands.json with -Wunused-parameter and count warnings from the header"

key-files:
  created:
    - tests/tools/check_unused_params.py
  modified:
    - include/SpatialCore/OSC/ADMOSCReceiver.h
    - src/OSC/ADMOSCReceiver.cpp
    - include/SpatialCore/OSC/ADMOSCSender.h
    - src/OSC/ADMOSCSender.cpp
    - src/Core/FloatSemanticsGuard.h
    - tests/OSC/ADMOSCReceiverTests.cpp
    - tests/OSC/ADMOSCSenderTests.cpp

key-decisions:
  - "Replies go to the sender's configured host:port, never the packet source (D-20): juce::OSCReceiver does not expose the source address, and a configured-only destination cannot be used for UDP reflection"
  - "disconnect() clears pending replies, so a reply queued for one connection cannot leak into the next"
  - "queueReply drops the whole reply if any of the three values is non-finite, whichever kind it is"
  - "A NaN or infinity anywhere in a received message drops the whole message, so NaN stays only the 'axis not sent' sentinel"
  - "Full /xyz is not clamped on input (its polar result is already in range); only partial /x /y /z are clamped to [-1, 1]"

patterns-established:
  - "Receiver tests that need a different library header copy the small helper they need into their own anonymous namespace instead of sharing a header"

requirements-completed: [EXTR-04]

coverage:
  - id: D1
    description: "A late-joining device that sends /adm/obj/4/xyz with no arguments over UDP gets one reply /adm/obj/4/xyz carrying object 4's position at the sender's configured destination"
    requirement: EXTR-04
    verification:
      - kind: integration
        ref: "tests/OSC/ADMOSCReceiverTests.cpp#ADM-OSC query: /adm/obj/4/xyz with no arguments over UDP is answered at the configured destination"
        status: pass
    human_judgment: false
  - id: D2
    description: "All five position properties (and the /osd/obj/ alias) with no arguments are reported once with the matching kind and never as a position; other no-argument messages and object 13 are ignored"
    requirement: EXTR-04
    verification:
      - kind: unit
        ref: "tests/OSC/ADMOSCReceiverTests.cpp#[osc][query] and [osc][query][edge]"
        status: pass
    human_judgment: false
  - id: D3
    description: "Replies mirror the query (1, 1, 1, 3, 3 float32 arguments, xyz inverse conversion round-trips within 1e-3), coalesce per (object, kind) so 1000 queries give 1 reply, go out in object then kind order, wait for the next slot, are dropped while disconnected or for bad input, and leave the dead-band untouched"
    requirement: EXTR-04
    verification:
      - kind: integration
        ref: "tests/OSC/ADMOSCSenderTests.cpp#[osc][send][query] (5 cases)"
        status: pass
    human_judgment: false
  - id: D4
    description: "Wrong-typed, non-finite and out-of-range input is rejected, clamped or wrapped; a forwarded azimuth from 1e10 or 1e30 is in [-180, 180] and wrapAzimuth returns; compliant float32 input forwards bit for bit"
    requirement: EXTR-04
    verification:
      - kind: unit
        ref: "tests/OSC/ADMOSCReceiverTests.cpp#[osc][edge] (6 new cases) and [osc][edge][compliant]"
        status: pass
    human_judgment: false
  - id: D5
    description: "ADMOSCReceiver.h raises no -Wunused-parameter warning (5 at BASE, 0 now)"
    verification:
      - kind: other
        ref: "python3 tests/tools/check_unused_params.py"
        status: pass
    human_judgment: false

duration: 14min
completed: 2026-10-05
status: complete
---

# Phase 4 Plan 03: ADM-OSC Query Replies and Receiver Hardening Summary

**A no-argument ADM-OSC position message is now a query that the consumer answers through `ADMOSCSender::queueReply`, replied on the next 30 Hz slot to the configured destination only; the receiver rejects wrong-typed and non-finite input, clamps ranges and wraps a 1e10 azimuth once, and its header is warning-clean (5 to 0)**

## Performance

- **Duration:** 14 min (08:28Z to 08:42Z)
- **Tasks:** 3
- **Files modified:** 8 (1 new script, 7 modified: 2 headers, 2 sources, `FloatSemanticsGuard.h`, 2 test files)
- **BASE:** `3ec206a6c235ba48dea8ef3bc15df1a6f2f6d14a` (HEAD when this plan started)

## Accomplishments

- ROADMAP late-joiner half (D-08a) works end to end over real UDP: a device sends `/adm/obj/4/xyz` with no arguments, the receiver reports it to the Listener, the consumer queues a reply from its own state, and the sender emits `/adm/obj/4/xyz -0.5 0 0` at the configured host and port on its next slot. The receiver gained no position store and no processor pointer (DR-16).
- Query flood is bounded (T-04-07): 1000 queries for one (object, kind) before a slot produce one reply carrying the last value. Replies are flushed only on the 30 Hz slot and never while disconnected.
- The unauthenticated input surface is hardened (D-21, T-04-05, T-04-06). `/osd/obj/1/enabled` with a string no longer reaches `getInt32()`, so the Debug assertion at `juce_OSCArgument.cpp` is gone (0 lines in the edge log). NaN and infinity drop the whole message. Elevation, distance and partial cartesian axes are clamped. An azimuth outside [-180, 180] is wrapped with one `std::remainder`.
- The hang is real and now closed: at BASE a forwarded 1e10 azimuth spins `wrapAzimuth` forever in float (`1e10 - 360 == 1e10`). The new test hung when I ran it against the pre-hardening receiver, which confirmed it detects the defect; it passes on the hardened receiver in no measurable time.
- D-15: `ADMOSCReceiver.h` unused-parameter warnings went from **5 to 0**, measured by the new `tests/tools/check_unused_params.py`.

## Task Commits

1. **Task 1: Tracer, /adm/obj/4/xyz with no arguments over UDP is answered at the configured destination** - `fd7e8cd` (feat)
2. **Task 2: Query grammar, reply formats, coalescing and ordering** - `523e99f` (test)
3. **Task 3: Harden the receiver and silence the D-15 warnings** - `088621a` (feat)

**Plan metadata:** committed with this SUMMARY (docs: complete plan)

## Files Created/Modified

- `include/SpatialCore/OSC/ADMOSCReceiver.h` - `ADMPositionQuery`, `Listener::admPositionQueried` (defaulted), unnamed parameters on the three defaulted virtuals (D-15)
- `src/OSC/ADMOSCReceiver.cpp` - no-argument query branch, `readFloat`, `readNumeric`, `wrapAzimuthBounded`, clamps; every branch reads arguments only through them
- `include/SpatialCore/OSC/ADMOSCSender.h` - `queueReply` with the D-20 deviation stated in its doc comment, the pending-reply table, `flushReplies`
- `src/OSC/ADMOSCSender.cpp` - `queueReply`, `flushReplies` (object then kind order, inverse xyz), flush at the end of a send slot, `disconnect()` clears pending replies
- `src/Core/FloatSemanticsGuard.h` - comment corrected: the receiver now has isfinite tests of its own
- `tests/OSC/ADMOSCReceiverTests.cpp` - `QueryGlue`, a copy of `RecordingCapture`, the tracer, 3 query cases, 6 hardening cases
- `tests/OSC/ADMOSCSenderTests.cpp` - 5 `[osc][send][query]` cases, `RecordingCapture::at`
- `tests/tools/check_unused_params.py` - the D-15 check (stdlib only)

## Receiver branches whose accepted types changed

| Property | At BASE | Now |
|---|---|---|
| `/azim /elev /dist /aed /xyz /x /y /z` | float32 only, no finiteness check | float32 only, finite; clamped or wrapped (unchanged for in-range input) |
| `/doppler /pitch /speed` | float32 only | float32 only, finite |
| `/enabled /trajectory /direction /input` | float32, else `getInt32()` on any other type (string: 0 in Release, assertion in Debug) | finite float32 or int32; every other type ignored |
| `/osd/global/*` | same float-or-else-int rule | finite float32 or int32; every other type ignored |
| any position property, no arguments | ignored | a query (new callback) |

Behaviour a compliant sender can see: none for in-range float32 input (asserted bit for bit in `[osc][edge][compliant]`). A sender that wrote an out-of-range azimuth (for example 190) now gets -170 forwarded; the old value was passed through unwrapped. This belongs in the OSD release note (Plan 04-08).

## Decisions Made

- Replies leave to the configured destination only (D-20). The deviation from the ADM-OSC text is stated in `ADMOSCSender.h` where the API is.
- `queueReply` requires all three values finite, even for a one-value kind, so a consumer holding a NaN placeholder gets no reply rather than a half-valid one.
- The full `/xyz` input is not clamped: `cartesianToPolar` already limits distance to [0, 1], and a 1e30 triple comes out finite (azimuth and elevation from `atan2`, distance 1).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] `FloatSemanticsGuard.h` comment became false**
- **Found during:** Task 3
- **Issue:** the header says `ADMOSCReceiver.cpp` "has no isfinite / isnan test of its own"; the hardening adds them, and they are exactly what the guard protects against fast-math.
- **Fix:** rewrote that paragraph to name both reasons (IN-16 NaN sentinel, D-21 isfinite rejection). Comment only; the `#error` guard is unchanged.
- **Files modified:** `src/Core/FloatSemanticsGuard.h`
- **Committed in:** `088621a`

**2. [Rule 2 - Missing critical] `disconnect()` clears pending replies**
- **Found during:** Task 2
- **Issue:** the plan gates `queueReply` on being connected but says nothing about a reply that is still pending when the sender disconnects and reconnects; it would be sent to the new destination.
- **Fix:** `disconnect()` clears the table. Proven in `[osc][send][query]` (a reply queued, disconnect, reconnect, nothing sent).
- **Files modified:** `src/OSC/ADMOSCSender.cpp`
- **Committed in:** `fd7e8cd`

---

**Total deviations:** 2 auto-fixed (1 comment correction, 1 missing-state guard). **Impact on plan:** none on scope or API. `FloatSemanticsGuard.h` is outside the plan's `files_modified` list, comment only.

## Issues Encountered

- To confirm the hardening tests fail against the old receiver I restored the Task 2 receiver source and ran `[osc][edge]`; it hung (the 1e10 `wrapAzimuth` loop). I killed the process and restored the hardened source from a saved copy before the final runs, so the committed tree is the hardened one. The same method (mutating `flushReplies` to not clear the pending flag) made 3 sender assertions fail, so the query tests can fail.
- The `[osc][query]` filter spelled as one comma-joined string works as the plan wrote it.
- The Debug executable still prints the known `SharedFFTCache` leak report at exit and the `juce_MessageListener.cpp:50` assertion per loopback receiver (both pre-existing, noted in 04-02); exit code is 0.
- The tracer passed on its first run. Its red state was the compile failure (`ADMPositionQuery` and `queueReply` unknown), confirmed before any implementation.

## Verification

- `[osc][query][udp][tracer]`: All tests passed (12 assertions in 1 test case)
- `[osc][query],[osc][send][query]`: 9 test cases, All tests passed (108 assertions)
- `[osc][edge]`: 15 test cases (9 before Task 3 plus 6 new), All tests passed (200 assertions); `grep -c juce_OSCArgument.cpp build/osc-edge.log` is 0
- `[osc]`: All tests passed (322 assertions in 51 test cases); `[route][osc]`: All tests passed (36 assertions in 6 test cases)
- `python3 tests/tools/check_unused_params.py`: 5 before the header edit, `ADMOSCReceiver.h unused-parameter warnings: 0` and exit 0 after
- Full Debug suite: All tests passed (309860 assertions in 297 test cases), exit 0 (baseline 282 plus 15 new)
- Release: `cmake --build build-release -j8` then `ctest --test-dir build-release/tests`: 100% tests passed out of 297 (Debug and Release were run one after the other, never together)
- Acceptance greps: `admPositionQueried` in `ADMOSCReceiver.h` 2, `enum class ADMPositionQuery` 1, `queueReply` in `ADMOSCSender.h` 1, `D-20` in `ADMOSCSender.h` 1, pure-virtual `admPositionQueried` 0, `isFloat32() ? ` in the receiver source 0, `git diff BASE..HEAD -- include/SpatialCore/Trajectory/TrajectoryEngine.h` empty
- Line endings kept: `ADMOSCSender.h` is still CRLF throughout (88 of 88 lines); the other edited files are LF as at BASE

## Known Stubs

None.

## Threat Flags

None beyond the plan's register. T-04-05 and T-04-06 (high) are mitigated and proven by `[osc][edge]`; T-04-07 by `[osc][send][query]` (1000 queries, 1 reply, configured destination only). T-04-08 (self-feedback) is a consumer duty through `oscPortsConflict`, taken up by the demo in Plan 04-07. The only new traffic is the outgoing reply UDP to the existing configured destination.

## Notes for OpenSpatialDelay

- `ADMOSCReceiver::Listener` gained a defaulted virtual, so OSD compiles unchanged and simply never answers queries until it overrides `admPositionQueried` and calls `ADMOSCSender::queueReply`.
- OSD's receive behaviour changes only for non-compliant input: wrong types, NaN or infinity, and out-of-range values (listed in the table above). Release-note item (D-14 precedent).
- The five `-Wunused-parameter` warnings the header raised in every OSD translation unit are gone.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for Plan 04-04. The demo in Plan 04-07 supplies a real `admPositionQueried` answer and checks `oscPortsConflict` before connecting (T-04-08).
- `tests/tools/check_unused_params.py` is ready for Plan 04-08's gate; it needs `build/compile_commands.json` (exit 2 with the re-run hint if missing).
- UDP ports used here: 9750 to 9756. Both build trees are configured and current.

---
*Phase: 04-control-surface-ui*
*Completed: 2026-10-05*

## Self-Check: PASSED

- FOUND: include/SpatialCore/OSC/ADMOSCReceiver.h
- FOUND: src/OSC/ADMOSCReceiver.cpp
- FOUND: include/SpatialCore/OSC/ADMOSCSender.h
- FOUND: src/OSC/ADMOSCSender.cpp
- FOUND: tests/OSC/ADMOSCReceiverTests.cpp
- FOUND: tests/OSC/ADMOSCSenderTests.cpp
- FOUND: tests/tools/check_unused_params.py
- FOUND: commits fd7e8cd, 523e99f, 088621a
