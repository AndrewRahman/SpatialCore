---
phase: 02-algorithm-format-verification
plan: 02
subsystem: testing
tags: [reference-oracles, scipy, numpy, ear, itu-r-bs2127, sn3d, vbap, vbip, dbap, mdap, python]
status: complete

requires:
  - phase: 02-algorithm-format-verification
    provides: "02-01 left the layoutDefs table untouched (it only added functions below makeLayoutFromDef), so the parser reads the current table"
provides:
  - "tests/reference/ offline oracles: layouts_from_cpp.py, gen_sh_reference.py, gen_ear_reference.py, gen_panning_reference.py, README.md"
  - "ShReference.h — 49 SN3D literals at az 64 el 10 (kShRef_az64_el10, kShRefAzimuthDeg, kShRefElevationDeg)"
  - "EarReference.h — ear 2.1.0 nadir-cap vectors (EarCase, kEarCases_S5_1_2 / S7_1_4 / S9_1_6, kEarNumSpeakers_*)"
  - "PanningReference.h — textbook VBAP/VBIP/DBAP, MDAP port, 7.1.4 3D VBAP pins (Vbap3DCase, kVbap3D_S7_1_4)"
affects: [02-04 EAR oracle tests, 02-05 panning-law tests, 02-06 SN3D literal tests]

plan_head_before: c79d1cbe55ed7583cfac59b03a6b1a0ebe922227
plan_head_after: c5999dd4afc0c0bca9ca3bf2ef0ad98e7197988c

actuals:
  tokens: 9445
  tasks: 3
  commits: 2

tech-stack:
  added: ["numpy 2.5.3, scipy 1.18.1, ear 2.1.0 (offline dev-machine venv only, never shipped, never in CI)"]
  patterns:
    - "Generated literal headers: generator stdout is the header, diagnostics go to stderr, and output is buffered until every self-check has passed, so a failed run never writes a partial header"
    - "Oracle inputs are parsed from the real source table (layoutDefs), never hand-copied"
    - "Regenerate-and-diff is the integrity gate for checked-in reference data"

key-files:
  created:
    - tests/reference/README.md
    - tests/reference/.gitignore
    - tests/reference/layouts_from_cpp.py
    - tests/reference/gen_sh_reference.py
    - tests/reference/gen_ear_reference.py
    - tests/reference/gen_panning_reference.py
    - tests/reference/ShReference.h
    - tests/reference/EarReference.h
    - tests/reference/PanningReference.h
  modified: []

key-decisions:
  - "Task 1 package gate answered approved-recreate (user delegated the decision to the orchestrator): .context/venv deleted and rebuilt from the pinned recipe; numpy/scipy/ear provenance verified against PyPI metadata"
  - "Generator self-checks use sys.exit, not assert, and stdout is emitted only after all checks pass, so python -O cannot strip a check and a failure never truncates a header"
  - "ear exposes no __version__; the EAR header records importlib.metadata.version('ear') instead"
  - "The 7.1.4 3D-pin uniqueness check mirrors computeVBAPGains3D exactly (raw unnormalised gain sum, enclosing at >= -1e-6, |det| >= 0.01) over every speaker triple; margins 0.0793 / 0.0964 / 0.0353 / 0.1464, all well above 1e-4"

patterns-established:
  - "Offline oracle generators live in tests/reference/, are checked in, and are never run in CI"
  - "Test files include reference data as \"../reference/<Name>.h\", namespace spatialcore_ref"

requirements-completed: [EXTR-01, EXTR-03, VERIFY-01]

coverage:
  - id: D1
    description: "Four checked-in generators and a README with the pinned install recipe, regenerate commands and oracle-discipline rule; the parser reads all 15 real layouts from src/IO/SpeakerLayout.cpp"
    requirement: "VERIFY-01"
    verification:
      - kind: other
        ref: ".context/venv/bin/python tests/reference/layouts_from_cpp.py | wc -l | grep -qx ' *15' && .context/venv/bin/python -m py_compile tests/reference/*.py"
        status: pass
      - kind: other
        ref: "grep -c 'no-deps ear==2.1.0' / 'only-binary' tests/reference/README.md (1 / 1); git status --porcelain .github/ (empty)"
        status: pass
    human_judgment: false
  - id: D2
    description: "ShReference.h, EarReference.h and PanningReference.h compile standalone as C++17, carry 49 ACN lines (SH), and regenerate byte-identically from their scripts"
    requirement: "EXTR-03"
    verification:
      - kind: other
        ref: "Task 3 <verify>: c++ -std=c++17 -fsyntax-only on each header, 49 '// ACN ' lines, three regenerate-and-diff commands print nothing (exit 0)"
        status: pass
      - kind: other
        ref: "test-style TU including all three as \"../reference/*.h\" with -Wall -Wextra -Wpedantic -Werror (compiled, then deleted)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Generated values match every RESEARCH anchor: all 49 SH values equal Reference Data B exactly; VBAP/VBIP/DBAP/MDAP anchors (Reference Data A) and EAR 7.1.4 anchors (Reference Data C) match to the last printed digit"
    requirement: "EXTR-01"
    verification:
      - kind: other
        ref: "anchor cross-check script (parses RESEARCH §Reference Data B table and the headers): SH max diff 0.0; 12 anchor checks OK, 0 mismatches"
        status: pass
    human_judgment: false
  - id: D4
    description: "Package gate: numpy, scipy and ear were vetted as the upstream projects before use, and the venv was rebuilt from the pins"
    verification:
      - kind: manual_procedural
        ref: "Task 1 checkpoint answered approved-recreate; pip freeze after rebuild shows exactly the 10 pinned packages"
        status: pass
    human_judgment: true
    rationale: "The user delegated the legitimacy decision to the orchestrator, which checked PyPI metadata. A human should confirm that the delegation is an acceptable discharge of the blocking-human gate."

duration: 4min
completed: 2026-10-01
---

# Phase 2 Plan 02: Offline Reference Oracles Summary

**I checked in four offline Python oracles (a scipy SN3D generator, the PyPI ear 2.1.0 nadir-cap generator, textbook VBAP/VBIP/DBAP formulas with an MDAP port, and a `layoutDefs` parser). Their output is three self-checked C++17 headers that regenerate byte-identically and match every RESEARCH anchor.**

## Performance

- **Duration:** 4 min (this continuation, Tasks 2-3; Task 1 was resolved by the orchestrator beforehand)
- **Started:** 2026-10-01T06:37:36Z
- **Completed:** 2026-10-01T06:42:27Z
- **Tasks:** 3 (1 checkpoint resolved, 2 executed)
- **Files modified:** 9 created, 0 modified

## Task 1 answer (package-legitimacy gate, blocking-human)

**Resume signal: `approved-recreate`.**

Provenance, verbatim from the orchestrator: the user delegated the decision ("You decide about 1 and 2, these are technical questions and they are not my expertise. Pick your recommendations."). The orchestrator verified PyPI metadata before choosing:
- numpy 2.5.3: source github.com/numpy/numpy, BSD-3-Clause AND 0BSD AND MIT AND Zlib AND CC0-1.0, uploaded 2026-09-06
- scipy 1.18.1: source github.com/scipy/scipy, BSD (Enthought copyright), uploaded 2026-08-21
- ear 2.1.0: author EBU, github.com/ebu/ebu_adm_renderer, BSD-3-Clause-Clear, uploaded 2022-01-26
- the existing venv already held exactly the pinned versions (lxml 6.1.3, attrs 26.1.0, multipledispatch 1.0.0, PyYAML 6.0.3, ruamel.yaml 0.19.1, setuptools 80.10.2, six 1.17.0, numpy, scipy, ear)

The orchestrator chose "approved-recreate" over reuse for two reasons: the venv had been installed before vetting, and rebuilding from the pins also proves that the install recipe the README documents is reproducible.

**Correction to that provenance:** the pre-existing venv also held **`pypdf==6.19.0`**, which is not in the pin list, so it did not hold *exactly* the pinned set. The recreate removed it. The rebuilt venv holds only the 10 pinned packages, listed below.

### `pip freeze` of the venv used (rebuilt with the RESEARCH recipe, Python 3.14.7)

```
attrs==26.1.0
ear==2.1.0
lxml==6.1.3
multipledispatch==1.0.0
numpy==2.5.3
PyYAML==6.0.3
ruamel.yaml==0.19.1
scipy==1.18.1
setuptools==80.10.2
six==1.17.0
```

Install order: `--only-binary=:all: lxml==6.1.3`, then `--no-deps ear==2.1.0`, then the remaining pins. pip printed the expected "ear 2.1.0 requires numpy~=1.14 / lxml~=4.4 / attrs<22 / multipledispatch~=0.5" conflict warnings, and all installs succeeded. No pin was substituted.

## Accomplishments

- `layouts_from_cpp.py` parses all 15 layouts from `src/IO/SpeakerLayout.cpp`, using a path two directories above the script. It works from any cwd.
- `gen_sh_reference.py`: the two scipy routes agree to 3.3e-16, and the SN3D addition theorem holds to 1.8e-15 at orders 0-6. It emits 49 values at az 64 el 10, one per line, each tagged `// ACN n`.
- `gen_ear_reference.py`: all 13 cases fall in ear's VirtualNgon region, and all 3 meridian checks give exactly 1. It emits `EarCase` arrays for 5.1.2, 7.1.4 and 9.1.6 in `layoutDefs` order.
- `gen_panning_reference.py` takes Quad, 5.0 and 7.1.4 positions from the parser. Its checks pass: VBAP/VBIP power is 1, the VBIP energy vector points at the source, and the DBAP dist-0.5 value reproduces the code golden to within 1e-6. Each 7.1.4 pin's triplet is the unique minimum-sum enclosing triple by at least 0.035.
- The README holds the pinned recipe (the `--only-binary` lxml / `--no-deps ear` split), the regenerate commands, the oracle-discipline rule, the header-to-tag table, and the MDAP-port caveat.

## Anchor cross-check results (Task 3)

These values come from a parse of the generated headers compared against RESEARCH. Nothing was transcribed into a header.

| Anchor | Expected (RESEARCH) | Generated | Result |
|---|---|---|---|
| `kShRef_az64_el10` vs Reference Data B (all 49) | table | max abs diff **0.0** | MATCH |
| `kShRef_az64_el10[0]` | 1.000000000 | 1.000000000 | MATCH |
| smallest \|Y\| | ~0.0721261 at ACN 17 | 0.072126116 at ACN 17 | MATCH |
| `kEarCases_S7_1_4` (30, -90) | 0.3779645 x7, then 0 x4 | same | MATCH |
| `kEarCases_S7_1_4` (30, -60) [0] | 0.8300495 | 0.8300495 | MATCH |
| `kVbip_Quad_az30` | 0.8880738, 0.4597008, 0, 0 | same | MATCH |
| `kDbap_Quad_az30_dist1` (order 45, -45, 135, -135) | 0.9984304, 0.0459007, 0.0270259, 0.0173052 | same | MATCH |
| `kVbap_S5_0_az10` idx 0 / idx 2 | 0.4527072 / 0.8916592 | same | MATCH |
| `kVbap_Quad_az30`, `kVbip_S5_0_az10`, `kVbap/kVbip_S5_0_az50`, `kDbap_Quad_az30_dist05`, `kMdapPort_Quad_az30`, `kMdapPort_S5_0_az10` | Reference Data A | same | MATCH |
| `kVbap3D_S7_1_4` (4 pins) | Reference Data A 3D pins | same (e.g. (0, 30): idx2 0.7157629, idx7/8 0.4938033) | MATCH |

No mismatch was found, so no generator or parser defect needed investigating.

## Task Commits

1. **Task 1: Verify numpy, scipy and ear are the upstream projects**: no commit (checkpoint, no files). Resolved as `approved-recreate`.
2. **Task 2: Check in the generators, the layout parser and the README**: `a97382b` (feat)
3. **Task 3: Generate the three reference headers**: `c5999dd` (feat)

**Plan metadata:** recorded in the docs commit that adds this SUMMARY.

## Files Created/Modified

- `tests/reference/layouts_from_cpp.py`: `layoutDefs` parser (15-layout and per-layout speaker-count asserts)
- `tests/reference/gen_sh_reference.py`: scipy SN3D oracle, prints `ShReference.h`
- `tests/reference/gen_ear_reference.py`: ear 2.1.0 nadir-cap oracle, prints `EarReference.h`
- `tests/reference/gen_panning_reference.py`: textbook panning laws plus the MDAP port, prints `PanningReference.h`
- `tests/reference/README.md`: install recipe, pins, regenerate commands, oracle discipline, tag table, caveats
- `tests/reference/.gitignore`: ignores the `__pycache__/` created when the generators import the parser
- `tests/reference/ShReference.h`, `EarReference.h`, `PanningReference.h`: generated, do not edit

## Decisions Made

- **approved-recreate** (Task 1, delegated): rebuild rather than reuse. This also proved the recipe reproducible and removed the stray `pypdf`.
- Self-checks use `sys.exit(...)`, not bare `assert`, inside the generators, and all stdout is buffered until every check passes. A failed run therefore exits non-zero and writes nothing into the redirected header. The parser keeps its asserts as the plan specified.
- The EAR header's version line uses `importlib.metadata.version('ear')`, because ear 2.1.0 has no `__version__`.
- The 3D-pin uniqueness check replicates `computeVBAPGains3D`'s selection rule (raw gain sum, minimum wins, enclosing at >= -1e-6) over all triples with |det| >= 0.01. That set is a superset of the code's triangulation, so uniqueness here implies uniqueness in the code.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added `tests/reference/.gitignore` for `__pycache__/`**
- **Found during:** Task 2 (dry run of the generators)
- **Issue:** Importing `layouts_from_cpp` from the generators, and the plan's own `py_compile` verify, create `tests/reference/__pycache__/`. The root `.gitignore` does not cover it, so generated bytecode would have been left untracked.
- **Fix:** One-line `tests/reference/.gitignore` containing `__pycache__/`.
- **Files modified:** tests/reference/.gitignore
- **Verification:** `git status --short` is clean after running every generator.
- **Committed in:** a97382b

**2. [Rule 2 - Missing critical] Added `kEarNumSpeakers_<layout>` constants to EarReference.h**
- **Found during:** Task 2
- **Issue:** `EarCase::gains[16]` zero-pads past each layout's real speaker count, so without a count a consumer test cannot tell a pinned zero from an unused slot.
- **Fix:** The generator also emits `inline constexpr int kEarNumSpeakers_S5_1_2 = 7;` (and 11 and 15 for the other two layouts). This adds data and changes nothing the plan specified.
- **Files modified:** tests/reference/gen_ear_reference.py, tests/reference/EarReference.h
- **Verification:** included in the regenerate-and-diff gate.
- **Committed in:** a97382b, c5999dd

---

**Total deviations:** 2 auto-fixed (1 blocking, 1 missing critical)
**Impact on plan:** Both are small additions. No planned artifact, name or value changed.

## Issues Encountered

- The orchestrator's provenance note said the pre-existing venv held "exactly the pinned versions". It also held `pypdf==6.19.0`. The recreate removed it, so this had no effect on any generated value. It is recorded above for the audit trail.
- Compiling a header as its own translation unit (`c++ -fsyntax-only -x c++ <header>`) prints `#pragma once in main file` warnings. This is expected for that invocation, and the command still exits 0. Included the normal way from a test-style TU, all three headers compile cleanly under `-Wall -Wextra -Wpedantic -Werror`.
- No C++ source or CMake file changed, and nothing includes the headers yet. The SpatialCore test suite therefore has nothing new to exercise, and its baseline (156 cases, the sole known failure being the HUTUBS PP2 checksum) is unaffected.

## User Setup Required

None. The venv is a gitignored dev-machine tool, and nothing in CI uses it.

## Next Phase Readiness

- Plans 02-04 (`EarReference.h`), 02-05 (`PanningReference.h`) and 02-06 (`ShReference.h`) can now `#include "../reference/<Name>.h"`.
- Reminder for 02-05: the `kMdapPort_*` arrays are a cross-check of the port, not an oracle. Compare them at 5e-4.

---
*Phase: 02-algorithm-format-verification*
*Completed: 2026-10-01*

## Self-Check: PASSED

- All 9 files in key-files.created exist on disk.
- Commits a97382b and c5999dd exist on HEAD (`git rev-list --count c79d1cb..HEAD` = 2).
- Task 2 and Task 3 `<verify>` commands exit 0, and `git status --porcelain .github/` is empty.
