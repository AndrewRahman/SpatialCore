---
phase: "05"
slug: "realtime-safety-hardening"
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (the blocking gate)
threats_open: 0
asvs_level: 1
created: "2026-10-17"
---

# Phase 05 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| host audio thread → `RenderEngine::renderBlock` | The realtime contract (DR-1), crossed on every block | audio samples, block length (untrusted: a host may exceed the prepared size) |
| test-only tooling → shipped libraries | The malloc hook and pthread interposer must stay on the test side | none in a shipped build |
| consumer → `BinauralRenderer` (escape-hatch use) | A consumer driving a renderer directly may break the prepare contract | HRIR data, scratch sizes |
| message / loader thread → audio thread | Profile, format and algorithm requests; renderer handoff through the mailbox and `rendererFree_` | renderer ownership, layout snapshots |
| timer / message thread → any reader thread | Trajectory final positions every tick | az / el / dist floats |
| plugin instance ↔ plugin instance (one process) | Process-global FFT cache and font resources | FFT objects, typefaces |
| repository docs → consumers and future sessions | Realtime-safety claims plugin authors rely on | documentation claims |

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-05-01 | Tampering | SpatialCoreRTGuard (malloc_logger hook, pthread interposer) | high | mitigate | `tests/CMakeLists.txt:73-75` builds it only under `APPLE AND NOT -fsanitize`, linked only into `SpatialCoreTests`; `git grep -e malloc_logger -e __interpose -- src include CMakeLists.txt` prints nothing | closed |
| T-05-02 | Repudiation | trap reporting a zero it cannot see | high | mitigate | `requireRealtimeTrapCanary()` before every trap case (`tests/Engine/RealtimeSafetyTests.cpp:224`, `:230`, …) | closed |
| T-05-03 | Denial of service | Ambisonics NFC temporary allocating on the audio thread | medium | mitigate | In-place `std::array<float, 4>` assignment (`src/Engine/RenderEngine.cpp:1651`); `[rt][rt-alloc][rt-lock][ambi]` case (`RealtimeSafetyTests.cpp:253`) | closed |
| T-05-04 | Tampering / DoS | HRTF fixed buffers vs an oversized host block | high | mitigate | Pieces of at most `preparedMaxBlock_` (`RenderEngine.h:603`, `:751`, `:864`); `[oversized]` cases in `RenderEngineTests.cpp` and `RealtimeSafetyTests.cpp` | closed |
| T-05-05 | Denial of service | audio-thread `resize`/`assign` in five guards | high | mitigate | `BinauralRenderer::scratchFits` (`src/Binaural/BinauralRenderer.cpp:38`, `:204`); `getScratchTooSmallCount()`; Release trap cases | closed |
| T-05-06 | Tampering | skipped guard leaving stale or partial state | medium | mitigate | Skip returns before any write (`BinauralRenderer.h:49`, `:57`); twin-renderer bit comparison in `BinauralRendererTests.cpp` | closed |
| T-05-07 | Repudiation | a format-, profile-, size- or algorithm-specific allocation slipping through | medium | mitigate | `[rt]` matrix in `RealtimeSafetyTests.cpp` and `BinauralRendererTests.cpp` | closed |
| T-05-08 | Denial of service | a SpinLock or new mutex reaching the audio path unseen by the interposer | medium | mitigate | `[rt][rule]` lock / blocking-call inventory in `RealtimeSafetyTests.cpp` | closed |
| T-05-09 | Tampering | TrajectoryEngine final arrays (torn position) | medium | mitigate | `std::atomic<float> finalAz_/finalEl_/…` with `is_always_lock_free` static_assert (`TrajectoryEngine.h:130-133`); `[trajectory][threads]` in `[tsan]` | closed |
| T-05-10 | Repudiation | sanitizer gate reading only exit status or only the log | medium | mitigate | README gate requires exit 0 and `grep -c "WARNING: ThreadSanitizer"` = 0 (`README.md:154`, `:158`, `:163`, `:166`) | closed |
| T-05-11 | Tampering / DoS | legacy load into the active or fading renderer | high | mitigate | In-use mark: `HRTFDatabase::isInUseByAudioThread()` / `getMutationWhileInUseCount()` (`HRTFDatabase.h:29`, `:130`), Debug assert; documented flow in `RenderEngine.h`. Residual risk accepted (AR-02) | closed |
| T-05-12 | Denial of service | legacy swap / prepare-index call from the render thread | medium | mitigate | `noteLegacyRouteCall` (`RenderEngine.cpp:1848`) with `getLegacyRouteWrongThreadCount()` (`RenderEngine.h:429`) | closed |
| T-05-13 | Repudiation | in-use mark still set when the loader takes a renderer | medium | mitigate | Mark cleared before each `rendererFree_` release store (`RenderEngine.cpp:148`, `:481`) and in `prepare()` | closed |
| T-05-14 | Tampering / DoS | cross-instance shared HRTF state (OSD#96 class) | high | mitigate | Per-renderer `HRTFDatabase hrtfDatabase` member (`BinauralRenderer.h:33`); `[multi][isolation][tsan]` (`tests/Engine/MultiInstanceTests.cpp:210`) | closed |
| T-05-15 | Denial of service | concurrent first use of `SharedFFTCache` | high | mitigate | `juce::SpinLock lock` in `getOrCreate` (`SharedFFTCache.h:20`, `:42`); `[fftcache][tsan]` cases (`tests/Binaural/SharedFFTCacheTests.cpp:27-83`) | closed |
| T-05-16 | Denial of service | shared FFT on JUCE's fallback engine (Windows/Linux) | medium | transfer | Transferred to Phase 6 CI-01 (D-13); platform limit documented in `SharedFFTCache.h:36` and README "Proved on macOS (Apple Silicon); Windows and Linux are checked in Phase 6" | closed |
| T-05-17 | Denial of service | per-instance typeface duplication (OSD#131 class) | medium | mitigate | `juce::SharedResourcePointer<MonoFaces>` (`SpatialMapComponent.h:148-153`); construction-count test (`tests/UI/MultiMapTests.cpp:68-75`) | closed |
| T-05-18 | Information disclosure / Tampering | new public test accessors | low | accept | Read-only pointer identity and count (`SpatialMapComponent.h:100`, `:105`); see AR-03 | closed |
| T-05-19 | Repudiation | overclaimed proof in README, CLAUDE.md, integration guide | medium | mitigate | D-13 scope sentence at `README.md:134` and `CLAUDE.md:131` ("no claim covers CI or a real DAW") | closed |
| T-05-20 | Denial of service | a realtime-safety gate nobody runs | medium | mitigate | `[tsan]` subset for routine use, full suite at phase end / tag, run times beside the commands (`README.md:152-166`) | closed |
| T-05-SC | Tampering | npm / pip / cargo installs | low | accept | No package-manager install in any plan; existing FetchContent pins only; see AR-01 | closed |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above workflow.security_block_on count toward threats_open*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| AR-01 | T-05-SC | No package-manager installs; JUCE, Catch2 and libmysofa are the existing FetchContent pins reused from the local cache | plan threat model (05-01..05-08) | 2026-10-17 |
| AR-02 | T-05-11 (residual) | The legacy load route cannot be blocked without breaking OpenSpatialDelay (D-09); misuse is detected and counted, not prevented, until the next major removes the route | plan threat model (05-05) | 2026-10-17 |
| AR-03 | T-05-18 | Test accessors are read-only, return a pointer identity and a count, change no state and have no consumer use | plan threat model (05-07) | 2026-10-17 |

*Accepted risks do not resurface in future audit runs.*

---

## Security Audit Trail

| Audit Date | Threats Total | Closed | Open | Run By |
|------------|---------------|--------|------|--------|
| 2026-10-17 | 21 | 21 | 0 | gsd-secure-phase (orchestrator, ASVS L1 grep verification) |

---

## Sign-Off

- [x] All threats have a disposition (mitigate / accept / transfer)
- [x] Accepted risks documented in Accepted Risks Log
- [x] `threats_open: 0` confirmed
- [x] `status: verified` set in frontmatter

**Approval:** verified 2026-10-17
