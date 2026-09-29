---
schema_version: 1
open_count: 1
waived_count: 0
fixed_count: 0
total_count: 1
last_updated: 2026-08-15T00:40:34.759Z
---

# Broken Windows Ledger

> Cross-phase defect register. `/gsd-ship` blocks while `open_count > 0`.
> Waive with `gsd-tools windows waive <id> "<reason>"` (reason required).
> Mark fixed with `gsd-tools windows fixed <id>`.

| id | phase | kind | file | line | description | status | reason | recorded_at | resolved_at |
|----|-------|------|------|------|-------------|--------|--------|-------------|-------------|
| 1 | 01 | unrun-verify | tests/Binaural/HutubsPP2Tests.cpp | 47 | Pre-existing HUTUBS PP2 golden-checksum test failure, unrelated to plan 01-01's files; full-suite run is red (146/147) though [counts]-filtered run is green | open |  | 2026-08-15T00:40:34.759Z |  |

````json
[
  {
    "id": 1,
    "kind": "unrun-verify",
    "phase": "01",
    "file": "tests/Binaural/HutubsPP2Tests.cpp",
    "line": 47,
    "description": "Pre-existing HUTUBS PP2 golden-checksum test failure, unrelated to plan 01-01's files; full-suite run is red (146/147) though [counts]-filtered run is green",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-08-15T00:40:34.759Z",
    "resolved_at": null
  }
]
````
