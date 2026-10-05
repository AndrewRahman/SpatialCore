---
phase: 04-control-surface-ui
plan: 02
subsystem: osc
tags: [adm-osc, juce_osc, sender, rate-limit, schedule, catch2, udp]

requires:
  - phase: 04-control-surface-ui
    provides: "04-01 build trees (Debug build/, Release build-release/) and the route-test conventions"
provides:
  - "ADMOSCSender::tick (..., double nowSeconds): self-clocked 30 Hz send schedule with stall resync"
  - "ADMOSCSender::kSendIntervalSeconds, the broadcast interval constant"
  - "First-position, connect and re-enable sends; silence while objects are still"
  - "RecordingCapture, a thread-safe loopback capture with waitUntilQuiet, in the sender tests"
affects: [04-03]

plan_head_before: aa64f2fdbce58a0d88f1b99c186c7ed86d019629
plan_head_after: ee8d836b8a72a94ec3a8c953502d7147176a6991

actuals:
  tokens: 5300
  tasks: 2
  commits: 3

tech-stack:
  added: []
  patterns:
    - "Time-injected schedule: the five-argument tick forwards to the nowSeconds overload with the hi-res wall clock, so every rate rule is testable with a fake clock (D-10)"
    - "Schedule-based gate: the due time advances by exactly 1/30 per send and resyncs to now + 1/30 only when the caller is a full interval late"
    - "Per-object force-send flag, set at construction, connect() and while disabled, cleared on send"

key-files:
  created: []
  modified:
    - include/SpatialCore/OSC/ADMOSCSender.h
    - src/OSC/ADMOSCSender.cpp
    - tests/OSC/ADMOSCSenderTests.cpp

key-decisions:
  - "The five-argument tick OSD calls keeps its signature and now delegates to the new overload, so OSD's call site is unchanged and gets 30 Hz at any timer rate"
  - "forceSend_ is a std::array<bool, MAX_SOURCES> filled true by a constructor, so the first position of every object is sent even at exactly (0, 0, 0) (D-09)"
  - "The wall-clock test judges the rate against the elapsed time it measured, not a fixed 25 to 35 band, because a loaded machine stretches the sleeps"

patterns-established:
  - "Sender tests drive simulated time through the nowSeconds overload and wait for the loopback wire to go quiet after each simulated second"

requirements-completed: [EXTR-04]

coverage:
  - id: D1
    description: "A 60 Hz caller moving one object gets 30 messages per second on the wire and none while the position is unchanged"
    requirement: EXTR-04
    verification:
      - kind: integration
        ref: "tests/OSC/ADMOSCSenderTests.cpp#ADMOSCSender: a 60 Hz caller moving an object sends 30 messages per second, none while still"
        status: pass
    human_judgment: false
  - id: D2
    description: "Callers at 120, 60, 50 and 30 Hz with +-2 ms jitter, and at exactly 1/60 s, each get 300 +- 10 messages over 10 simulated seconds; a one second stall is followed by 30 messages, not a burst"
    requirement: EXTR-04
    verification:
      - kind: integration
        ref: "tests/OSC/ADMOSCSenderTests.cpp#[osc][send][rate] and [osc][send][rate][gap]"
        status: pass
    human_judgment: false
  - id: D3
    description: "First position is sent at (0, 0, 0); the 0.1 degree dead-band is strict; connect and reconnect send every enabled object once; re-enable sends once; disabled objects are never sent"
    requirement: EXTR-04
    verification:
      - kind: integration
        ref: "tests/OSC/ADMOSCSenderTests.cpp#[osc][send][first], [deadband], [static], [connect], [reenable]"
        status: pass
    human_judgment: false
  - id: D4
    description: "The five-argument tick on the wall clock stays at 30 messages per second; the three pre-existing sender cases pass unchanged"
    verification:
      - kind: integration
        ref: "tests/OSC/ADMOSCSenderTests.cpp#[osc][send][rate][realtime]; ./build/tests/SpatialCoreTests [osc][send]"
        status: pass
    human_judgment: false

duration: 18min
completed: 2026-10-05
status: complete
---

# Phase 4 Plan 02: ADMOSCSender 30 Hz Schedule Summary

**ADMOSCSender now keeps its own 30 Hz schedule with stall resync (300 messages per 10 s at 120, 60 and 50 Hz callers, 299 at 30 Hz with jitter), stays silent while objects are still, and sends first position, connect and re-enable once, proven with an injected clock over loopback UDP**

## Performance

- **Duration:** 18 min (08:09Z to 08:27Z)
- **Tasks:** 2
- **Files modified:** 3 (1 header, 1 source, 1 test file)
- **BASE:** `aa64f2fdbce58a0d88f1b99c186c7ed86d019629` (HEAD when this plan started)

## Accomplishments

- ROADMAP criterion 2 holds: SpatialCore broadcasts at 30 Hz whatever rate the caller's timer runs at (D-07) and sends nothing while positions are still (D-08c). The old rule (every second call, assuming 60 Hz) is gone, with no flag to restore it.
- An object's first position is always sent, including exactly (0, 0, 0) (D-09). At BASE an object starting inside the dead-band of zero was never sent.
- `connect()` and a reconnect send every enabled object at the next slot even if nothing moved; an object enabled again after a disable is sent once; a disabled object is never sent (D-08b).
- OSD's call is unchanged: the five-argument `tick` keeps its signature and forwards to the new overload with the hi-res millisecond counter.

## Measured message counts (injected clock, 10 simulated seconds, +-2 ms jitter from `juce::Random (42)`)

| Caller | Messages | Band |
|---|---|---|
| 120 Hz | 300 | 290 to 310 |
| 60 Hz | 300 | 290 to 310 |
| 50 Hz | 300 | 290 to 310 |
| 30 Hz | 299 | 290 to 310 |
| exactly 1/60 s, no jitter | 300 | 290 to 310 |
| 60 Hz tracer, 3 s moving | 90 (flush of the last move adds at most 1 after settling) | 87 to 93, then 0 while still |
| second after a 1 s stall | 30 | 29 to 31 |
| 5-argument tick on the wall clock | 34 over 1.108 s (about 30.7 per second) | 30 per second of measured time, +-5 |

These match the plan's simulation (120/60/50 Hz at 30.0 per second, 30 Hz at 29.9).

## Task Commits

1. **Task 1: Tracer, a 60 Hz caller moving one object sends 30 messages per second, none while still** - `9474c3b` (feat)
2. **Task 2: Every sender rule under the fake clock** - `6176601` (test)
3. **Line-ending restore for the header** - `ee8d836` (style)

**Plan metadata:** committed with this SUMMARY (docs: complete plan)

## Files Created/Modified

- `include/SpatialCore/OSC/ADMOSCSender.h` - `kSendIntervalSeconds`, the six-argument `tick`, `nextSendDue_`, `scheduleArmed_`, `forceSend_`, `consumeSendSlot`; the old call counter is removed
- `src/OSC/ADMOSCSender.cpp` - schedule gate with 1e-6 s tolerance and stall resync, force-send on first/connect/re-enable, dead-band unchanged (`> 0.1` deg, `> 0.001` distance); `connect()` arms a full resend, `disconnect()` disarms the schedule
- `tests/OSC/ADMOSCSenderTests.cpp` - `RecordingCapture`, `runMovingCaller`, `tickObjects`, and ten new test cases (13 sender cases in total)

## Decisions Made

- The wall-clock case compares the message count with 30 times its measured elapsed time (+-5) instead of a fixed 25 to 35 band; the fixed band is still checked when the loop ran under 1.1 s.
- `std::array<bool, MAX_SOURCES>` for `forceSend_`, filled in a constructor that replaces the defaulted one (the plan allowed either form).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Tracer test counted the flush of the last move as a message while still**
- **Found during:** Task 1 (first green run: `91 == 90`)
- **Issue:** the last azimuth change landed on an odd, non-slot call, so the next slot legitimately sent it; the test baselined before that flush.
- **Fix:** the test now ticks a few unchanged calls, lets the wire settle (at most 1 extra message), then asserts 0 further messages over 2 simulated seconds. The sender was correct (a change is sent at the next slot, D-08c).
- **Files modified:** `tests/OSC/ADMOSCSenderTests.cpp`
- **Committed in:** `9474c3b`

**2. [Rule 1 - Bug] Wall-clock test failed under full-suite load (36 messages, band 25 to 35)**
- **Found during:** Task 2 (full Debug run)
- **Issue:** 60 calls with `sleep (16)` took about 1.2 s under load, and 36 messages is exactly 30 per second over that time; the fixed band assumed a 1.0 s loop.
- **Fix:** assert the rate against the measured elapsed time (+-5), keep the plan's 25 to 35 band only when the loop ran under 1.1 s.
- **Files modified:** `tests/OSC/ADMOSCSenderTests.cpp`
- **Committed in:** `6176601`

**3. [Rule 3 - Blocking] Header line endings**
- **Found during:** SUMMARY preparation (`git diff --stat` showed every header line deleted)
- **Issue:** `ADMOSCSender.h` was CRLF at BASE; rewriting it converted it to LF, hiding the real change in a whole-file diff.
- **Fix:** restored CRLF; the net header diff is now 25 insertions, 2 deletions. Content unchanged.
- **Committed in:** `ee8d836`

---

**Total deviations:** 3 auto-fixed (2 test bugs, 1 line-ending restore). The sender design from the plan needed no change.
**Impact on plan:** none on scope; only `ADMOSCSender.h`, `ADMOSCSender.cpp` and its test file changed.

## Issues Encountered

- A Release ctest run that overlapped with my own Debug measurement runs failed three sender cases because both used the same UDP ports (9730 to 9742) at once. Re-run alone, all 282 pass. Ports are per test case, so the cases cannot run in parallel with a second copy of the suite.
- Debug builds print `JUCE Assertion failure in juce_MessageListener.cpp:50` once per `OSCReceiver::connect` in this file (3 from the pre-existing cases alone). It comes from running a loopback receiver without a MessageManager and is pre-existing noise; it does not fail the run.
- The Debug test executable still prints the known `SharedFFTCache` leak report at exit (deferred item from 02-04); exit code is 0.
- `[osc][send][first]` is stated red against BASE by inspection (BASE has no six-argument `tick`, and its dead-band skips an object whose first position is (0, 0, 0)); it was not rebuilt against BASE.

## Verification

- `[osc][send][rate][static][tracer]`: All tests passed (6 assertions)
- `[osc][send]`: All tests passed (90 assertions in 13 test cases), including the three pre-existing cases
- `[osc][send][first]`: passed
- Full Debug suite: All tests passed (309595 assertions in 282 test cases), exit 0 (baseline 272 plus 10 new)
- Release: `cmake --build build-release -j8` then `ctest --test-dir build-release/tests`: 100% tests passed out of 282
- Acceptance greps: `double nowSeconds` in header 2, `kSendIntervalSeconds` in cpp 4, `tickCounter` 0, `getMillisecondCounterHiRes` in cpp 1

## Known Stubs

None.

## Threat Flags

None. The only network surface is the existing outgoing UDP send to the consumer-configured host and port (T-04-04 accepted). T-04-03 (flood after a stalled timer) is mitigated by the schedule resync and proven by `[osc][send][rate][gap]` (30 messages in the second after a stall).

## Notes for OpenSpatialDelay

- OSD's wire behaviour changes: it now sends each object's first position, every enabled object on connect and reconnect, and a re-enabled object once. The 30 Hz rate and the dead-band are unchanged. This is an OSD release-note item (D-14 precedent).

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for Plan 04-03, which builds query replies on this gate. The sender's `consumeSendSlot` and `forceSend_` are private; 04-03 adds its own entry points on the same class.
- Both build trees are configured and current. Sender tests use UDP ports 9730 to 9742; Plan 04-03 must stay clear of them.

---
*Phase: 04-control-surface-ui*
*Completed: 2026-10-05*

## Self-Check: PASSED

- FOUND: include/SpatialCore/OSC/ADMOSCSender.h
- FOUND: src/OSC/ADMOSCSender.cpp
- FOUND: tests/OSC/ADMOSCSenderTests.cpp
- FOUND: commits 9474c3b, 6176601, ee8d836
