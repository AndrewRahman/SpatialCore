---
phase: 04
review: 04-REVIEW.md
titles: json
findings:
  - id: WR-01
    severity: warning
    disposition: fixed
    title: "The IN-02 `static_assert` does not catch an enumerator appended to `ADMPositionQuery`"
  - id: WR-02
    severity: warning
    disposition: fixed
    title: "`RecordingCapture::settle()` returns silently on timeout, so the \"zero is a fact\" assertions can pass without proof"
  - id: WR-03
    severity: warning
    disposition: fixed
    title: "The NaN \"axis not sent\" contract is cited as documented on `Listener::admPositionReceived`, but the header does not document it"
  - id: WR-04
    severity: warning
    disposition: fixed
    title: "`/xyz` overflows float for large components and returns a wrong elevation; it is also the only position form with no input range limit"
  - id: WR-05
    severity: warning
    disposition: fixed
    title: "Range hardening is partial: `/osd/global/*`, `/doppler`, `/pitch`, `/speed` are forwarded unbounded, and `wrapAzimuth` still loops without bound"
  - id: WR-06
    severity: warning
    disposition: fixed
    title: "New network tests bind fixed UDP ports and judge counts by \"quiet period\" heuristics"
  - id: IN-01
    severity: info
    disposition: fixed
    title: "Stale port comment left over from WR-06"
  - id: IN-02
    severity: info
    disposition: fixed
    title: "The two-character object-number cap is a hidden coupling to `MAX_SOURCES`"
  - id: IN-03
    severity: info
    disposition: fixed
    title: "`findFreeUdpPort` comment overstates what the OS guarantees; the Windows branch leaks a WSA reference"
  - id: IN-04
    severity: info
    disposition: fixed
    title: "`admObjectParamReceived` doc says the value is \"otherwise UNBOUNDED\", but `/x`, `/y`, `/z` are clamped to [-1, 1]"
  - id: IN-05
    severity: info
    disposition: fixed
    title: "Font loading is per instance and a null typeface falls back silently"
  - id: IN-06
    severity: info
    disposition: fixed
    title: "Demo shares audio state between message and audio paths without a guard"
  - id: IN-07
    severity: info
    disposition: fixed
    title: "Font provenance rule scans comments and hard-codes a resource count"
  - id: IN-08
    severity: info
    disposition: fixed
    title: "Magic numbers in the dead-band"
  - id: IN-21
    severity: info
    disposition: fixed
    title: "`ADMOSCSender::sendHost` and `sendPort` are written by `connect()` and never read"
  - id: WR-11
    severity: warning
    disposition: fixed
    title: "`ADMOSCSender::connect` leaves queued replies in place, so a reply can be sent to a different destination than the one it was queued for"
  - id: WR-12
    severity: warning
    disposition: fixed
    title: "In every sender test the `OSCReceiver` is declared before `RecordingCapture`, so the new `FAIL` in `settle()` unwinds into a destroyed listener"
  - id: IN-11
    severity: info
    disposition: fixed
    title: "The demo keeps its own copy of `findFreeUdpPort` that still has the Winsock reference leak fixed in the test copy"
open: 0
total: 18
recorded: 2026-10-05T14:58:01.829Z
---

# Phase 04: Code Review Disposition

| Finding | Severity | Disposition | Source |
|---------|----------|-------------|--------|
| WR-01 | warning | fixed | 04-REVIEW-FIX.iter4.md (not in the current review) |
| WR-02 | warning | fixed | 04-REVIEW-FIX.iter4.md (not in the current review) |
| WR-03 | warning | fixed | 04-REVIEW-FIX.iter2.md (not in the current review) |
| WR-04 | warning | fixed | 04-REVIEW-FIX.iter2.md (not in the current review) |
| WR-05 | warning | fixed | 04-REVIEW-FIX.iter2.md (not in the current review) |
| WR-06 | warning | fixed | 04-REVIEW-FIX.iter2.md (not in the current review) |
| IN-01 | info | fixed | 04-REVIEW-FIX.iter4.md (not in the current review) |
| IN-02 | info | fixed | 04-REVIEW-FIX.iter4.md (not in the current review) |
| IN-03 | info | fixed | 04-REVIEW-FIX.iter4.md (not in the current review) |
| IN-04 | info | fixed | 04-REVIEW-FIX.iter4.md (not in the current review) |
| IN-05 | info | fixed | 04-REVIEW-FIX.iter2.md (not in the current review) |
| IN-06 | info | fixed | 04-REVIEW-FIX.iter2.md (not in the current review) |
| IN-07 | info | fixed | 04-REVIEW-FIX.iter2.md (not in the current review) |
| IN-08 | info | fixed | 04-REVIEW-FIX.iter2.md (not in the current review) |
| IN-21 | info | fixed | 04-REVIEW-FIX.md (not in the current review) |
| WR-11 | warning | fixed | 04-REVIEW-FIX.iter5.md (not in the current review) |
| WR-12 | warning | fixed | 04-REVIEW-FIX.iter5.md (not in the current review) |
| IN-11 | info | fixed | 04-REVIEW-FIX.iter5.md (not in the current review) |

Dispositions: `open` (recorded, not yet triaged), `fixed`, `skipped`, `deferred`.
Set `deferred` by hand and put the reason in the Source cell; both are preserved. A `|` in the reason is kept as prose and escaped on the next run.
Re-running the gate keeps every row it can. A row the current review no longer reports is kept and its Source cell flagged, so a finding does not leave this record silently. ONE exception: when a finding id is REUSED by a different finding, the earlier decision cannot keep a row — the id is taken — and it is dropped. A RECORDED decision (anything but `open`) is named on the console when that happens; a row still at `open` is replaced silently, because `open` records no decision to lose.
