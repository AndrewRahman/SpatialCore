---
phase: "03"
slug: "binaural-defects-hrtf-packaging"
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (the blocking gate)
threats_open: 0
asvs_level: 1
created: "2026-10-05"
---

# Phase 03 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.
> Register built from the `<threat_model>` blocks of 03-01 to 03-11 (authored at plan time) and the
> `## Threat Flags` sections of every SUMMARY (all "None" beyond the register). ASVS L1,
> `security_block_on: high`. Verification is L1 depth: each mitigation located in code, tests,
> build files or phase artifacts by grep.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| shared HRTF folder → SpatialCore | Any process able to write `/Library/Application Support/Spatial Media Lab/HRTF/` (or `%ProgramData%` on Windows) supplies SOFA bytes that libmysofa parses | untrusted binary file (up to 256 MB) |
| build machine → shipped binary | `HRTF/*.sofa` at configure time is embedded in every consumer | Git LFS content |
| GitHub (hoene/libmysofa) → build | FetchContent downloads parser source at configure time | third-party source code |
| message thread → loader thread | Profile requests and the folder override | indices, a path (lock off the audio thread) |
| loader thread ↔ audio thread | Prepared renderer handed over via an atomic mailbox; free flags handed back | renderer ownership |
| consumer positions → audio thread | `RenderSources::objects` (possibly ADM-OSC-driven) drive cue weights | floats, possibly non-finite |
| consumer context flags → engine dispatch | `engineSelectsHRTF` changes who picks the render path | booleans |
| status text → plugin UI | `describeHRTFProfileStatus` output | strings from a static table |
| docs → future sessions and consumers | CLAUDE.md, skills, integration guide | API/option names |
| local planning text → public issue trackers | `gh issue` comment/create/close | public text |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation (evidence) | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-03-01 | Tampering (verdict integrity) | test thresholds | medium | mitigate | Bounds from 03-RESEARCH measurements; `[hrtf-embed][signature]` present in `tests/Binaural/HRTFEmbeddedTests.cpp` | closed |
| T-03-02 | Tampering (supply chain) | `HRTF/*.sofa` LFS content | low | mitigate | HDF5 8-byte signature check `HRTFEmbeddedTests.cpp:42`; CI guard in `.github/workflows/ci.yml` | closed |
| T-03-03 | Tampering (verdict integrity) | per-profile goldens | medium | mitigate | Captured goldens with in-test negative controls (03-02-SUMMARY) | closed |
| T-03-04 | DoS (audio-thread allocation) | `BinauralRenderer::updateSourceHRIR` | medium | mitigate | `ensureScratchCapacity()` (6 refs) in `prepare()`/`setProfile()`; `[renderer][scratch]` test | closed |
| T-03-05 | Tampering (shipped sound) | convolver transition timing | low | mitigate | `[convolver][transition]` test | closed |
| T-03-06 | DoS (non-finite audio) | cue weights from positions | medium | mitigate | `sanitizeSources` before the cue bank; `[bug01][weights]` NaN/inf test | closed |
| T-03-07 | Repudiation (unannounced audible change) | Simple-mode sound | low | mitigate | `[bug01][identity]` bit-identical front half; OSD release-note row in PROJECT.md:97; listening check Phase 999.4 | closed |
| T-03-08 | Tampering / EoP | libmysofa parse of shared-folder file | **high** | mitigate | Parse only via `resolveHRTFProfile` on the `HRTFProfileLoader` thread (`RenderEngine.cpp:62,248`); embedded fallback; `[hrtf-resolve][broken]`; libmysofa v1.3.5; review fixes add regular-file `fstat`/`S_ISREG` + `O_NOCTTY` + pre-resample shape check (`HRTFDatabase.cpp:75-87`) | closed |
| T-03-09 | DoS (memory) | oversized shared file | medium | mitigate | `kMaxSharedHRTFFileBytes` = 256 MB (`HRTFProfileResolver.h:45`) checked before read; injected-cap test | closed |
| T-03-10 | Tampering (path) | lookup filename | medium | mitigate | Names only from `kHRTFProfiles`; index validated; empty/non-directory folder skipped | closed |
| T-03-11 | Tampering (supply chain) | LFS pointer embedded in release | **high** | mitigate | Configure-time `FATAL_ERROR` "git lfs pull" guard (`CMakeLists.txt:96`); CI guard; `[hrtf-embed][signature]` | closed |
| T-03-12 | Information disclosure | `describeHRTFProfileStatus` | low | mitigate | Text from static table + integers only | closed |
| T-03-13 | Tampering | non-admin write to `%ProgramData%\...\HRTF` (Windows) | medium | transfer | Folder ACLs owned by the signed installer (SUITE-01); SpatialCore only reads and falls back | closed |
| T-03-14 | Tampering (data race) | loader writing a renderer audio reads | **high** | mitigate | `readyRenderer_` / `rendererFree_` release/acquire pairs; `[hrtf-switch][threads]`; TSan zero reports (03-07-SUMMARY) | closed |
| T-03-15 | DoS (audio dropout) | audio-thread blocking | **high** | mitigate | Audio side uses atomic load/store/exchange only; worker polls (`juce::Thread::sleep(2)`, RenderEngine.cpp:401) instead of being signalled | closed |
| T-03-16 | DoS (UI freeze / hang) | SOFA parse and shutdown | medium | mitigate | Parse on worker; `stopThread(15000)` (RenderEngine.cpp:308,460); `[hrtf-switch][prepare]` | closed |
| T-03-17 | Repudiation (silent failure) | failed loads | medium | mitigate | Lock-free `HRTFProfileStatus` with problem code; review WR fix stops "Loading" hang on failure | closed |
| T-03-18 | Tampering (silent re-baseline) | golden/characterisation constants | medium | mitigate | 03-08-MYSOFA-DELTA before/after table; user chose `keep-upgrade` 2026-10-04; nothing re-baselined | closed |
| T-03-19 | Tampering / EoP | malformed SOFA reaching unhardened parser | **high** | mitigate | libmysofa pinned to v1.3.5 (`CMakeLists.txt:52`) plus 03-05 controls | closed |
| T-03-20 | DoS (CPU spike) | dual-path render during Simple↔HRTF fade | low | accept | See Accepted Risks AR-03-01 | closed |
| T-03-21 | DoS (audio-thread allocation) | Simple scratch buffers | medium | mitigate | `simpleWetL_` / `simpleWetR_` sized in `prepare()` | closed |
| T-03-22 | Tampering (info integrity) | CLAUDE.md, skills, guide | medium | mitigate | API/option names grep-checked; stale strings negative-checked (03-10-SUMMARY) | closed |
| T-03-23 | Tampering (host system) | OSD build installing plugins | medium | mitigate | Non-installing targets only; OSD status and plug-in folder digest unchanged (03-10-SUMMARY) | closed |
| T-03-24 | Repudiation / Info disclosure | `gh issue` comment/create/close | medium | mitigate | Human-approved text; 0 `/Users/` matches in 03-11-OUTWARD.md; posted text kept | closed |
| T-03-25 | Tampering (wrong target) | `--repo` arguments | low | mitigate | Only `AndrewRahman/SpatialCore` written; OSD #234 comment-only (03-11-SUMMARY) | closed |
| T-03-26 | Tampering (data race) | mailbox claim/crossfade under concurrency | **high** | mitigate | `[hrtf-switch][threads]` with live render thread; TSan over every `[hrtf-switch]` case: zero reports | closed |
| T-03-27 | DoS (hang) | destruction mid-load / deadlock | medium | mitigate | `[hrtf-switch][shutdown]` bounded at 15 s with watchdog | closed |
| T-03-SC (03-08) | Tampering (supply chain) | libmysofa FetchContent tag | medium | mitigate | `v1.3.5` verified to resolve to `6cc5b15a73e9bd97810d03767082edda7f315881` via `ls-remote` and fetched `rev-parse HEAD` (03-08-MYSOFA-DELTA:10,122) | closed |
| T-03-SC (others) | Tampering | npm / pip / cargo installs | low | accept | See Accepted Risks AR-03-02 | closed |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above workflow.security_block_on count toward threats_open*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-03-01 | T-03-20 | One extra Woodworth pass (no convolution) for about 85 ms per Simple↔HRTF switch; HRTF↔HRTF fades already render two renderers | plan 03-09 threat model | 2026-10-04 |
| AR-03-02 | T-03-SC | No package-manager install occurs in plans 03-01..07, 03-09..11 | plan threat models | 2026-10-04 |

*Accepted risks do not resurface in future audit runs.*

### Non-blocking observations (L1 audit, 2026-10-05)

- T-03-SC (03-08): `CMakeLists.txt` pins `GIT_TAG v1.3.5`, a mutable tag. The planned control (one-time SHA check) was done, but a future re-tag upstream would not be caught. Pinning `GIT_TAG 6cc5b15a73e9bd97810d03767082edda7f315881` would make it durable.
- T-03-19: a consumer that supplies its own `mysofa-static` (OSD pins v1.3.2) bypasses the v1.3.5 pin; tracked as the OSD Phase 3 follow-up in PROJECT.md:97.

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-10-05 | 29 | 29 | 0 | /gsd-secure-phase 3 (orchestrator, L1 grep verification; auditor short-circuited per ASVS L1 rule) |

## Security Audit 2026-10-05
| Metric | Count |
|--------|-------|
| Threats found | 29 |
| Closed | 29 |
| Open | 0 |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-10-05
