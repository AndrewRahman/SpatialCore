# External Integrations

**Analysis Date:** 2026-08-10

> Verified against branch `gsd-remap` (HEAD `8d1868e`) by reading `include/SpatialCore/OSC/*.h`, `.gitattributes`, `HRTF/` directory listing (`ls -la` + `file`), `.github/workflows/ci.yml`, and `CMakeLists.txt`.

## APIs & External Services

SpatialCore makes no outbound network/cloud API calls. It is a DSP library; its only "external" surfaces are local network protocols and local file formats, detailed below. `JUCE_USE_CURL=0` is set on both build targets (`CMakeLists.txt:159`, `CMakeLists.txt:242`), explicitly disabling JUCE's optional HTTP/URL support since it is unused.

## Protocols

**ADM-OSC (Audio Definition Model over OSC):**
- Receive side: `include/SpatialCore/OSC/ADMOSCReceiver.h`, implementation `src/OSC/ADMOSCReceiver.cpp`
  - Wraps `juce::OSCReceiver` privately and implements `juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>` on itself (`ADMOSCReceiver.h:25-26`) — decoupled from any plugin processor per the header comment (`ADMOSCReceiver.h:9-13`).
  - Parses per-object position updates addressed as `/adm/obj/N/azim|elev|dist|aed|xyz|x|y|z` (`ADMOSCReceiver.h:36`) plus OSD-custom object properties (`enabled`, `doppler`, etc.) and global params under `/osd/global/<property>` (`ADMOSCReceiver.h:43-48`).
  - Listener callback interface (nested `Listener` struct) decouples wire-format parsing from application logic — callers implement position-update and property-update handlers (`ADMOSCReceiver.h:30-48`).
  - `connect(port)` returns bool and tracks `connected` state (`ADMOSCReceiver.h:59`); `disconnect()` at `ADMOSCReceiver.h:64`.
  - Test-only synchronous entry point: `testProcessOSCMessage(const juce::OSCMessage&)` (`ADMOSCReceiver.h:77`), gated behind `JUCE_UNIT_TESTS=1` (set in `tests/CMakeLists.txt:35`), mirrors the original OpenSpatialDelay plugin processor's test shim.
- Send side: `include/SpatialCore/OSC/ADMOSCSender.h`, implementation `src/OSC/ADMOSCSender.cpp`
  - Wraps a `juce::OSCSender` member (`ADMOSCSender.h:25`). Per CLAUDE.md, broadcasts at 30Hz (not independently verified by line-level evidence in this pass — treat as design intent from project docs, not confirmed in code comments read).
- Port-conflict validation: `include/SpatialCore/OSC/OSCPortValidation.h`, implementation `src/OSC/OSCPortValidation.cpp`
  - Standalone free function `oscPortsConflict` (not a method on Receiver/Sender) added for issue #179 (`OSCPortValidation.h:8-22`) — detects the case where the OSC receive port collides with the OSC send port when the send host is loopback, which would cause the plugin to re-receive its own broadcasts (message storm / dead OSC).
- Tests: `tests/OSC/ADMOSCReceiverTests.cpp`, `tests/OSC/ADMOSCSenderTests.cpp`, `tests/OSC/OSCPortValidationTests.cpp` (registered in `tests/CMakeLists.txt:14-16`).

## Data Formats

**SOFA (Spatially Oriented Format for Acoustics):**
- Parsed via `include/SpatialCore/Binaural/HRTFDatabase.h` / `src/Binaural/HRTFDatabase.cpp`, backed by libmysofa v1.3.2 (see STACK.md).
- 5 embedded SOFA HRTF profiles under `HRTF/`, confirmed as **real HDF5 data, not Git LFS pointer stubs** — verified via `file HRTF/*.sofa` (all report "Hierarchical Data Format (version 5) data") and `ls -la` byte sizes:
  | File | Size |
  |------|------|
  | `HRTF/bernschuetz_ku100.sofa` | 19,749,401 bytes (~19.7 MB) |
  | `HRTF/cipic_subject_003.sofa` | 1,837,391 bytes (~1.8 MB) |
  | `HRTF/hutubs_pp2.sofa` | 1,658,802 bytes (~1.6 MB) |
  | `HRTF/mit_kemar_large_pinna.sofa` | 1,161,691 bytes (~1.2 MB) |
  | `HRTF/sadie_d2_ku100.sofa` | 36,591,729 bytes (~36.6 MB) |
  - All exceed the 100KB pointer-stub threshold by 10-350x; none contain the LFS pointer text signature.
- Git LFS tracking confirmed via `.gitattributes`: `HRTF/*.sofa filter=lfs diff=lfs merge=lfs -text`.
- CI enforces this invariant explicitly (`.github/workflows/ci.yml:16-33`, step "Verify LFS objects resolved to real bytes (not pointer text)"): checks out with `lfs: true`, greps each `.sofa` file for the LFS pointer spec string, and fails if any file is under 100,000 bytes — guards against a documented "2026-04 LFS-pointer-in-binary incident class."
- Per-profile test coverage: `tests/Binaural/SadieD2KU100Tests.cpp`, `CipicSubject003Tests.cpp`, `HutubsPP2Tests.cpp`, `BernschuetzKU100Tests.cpp`, `MitKemarLargePinnaTests.cpp`, plus `WoodworthFallbackTests.cpp` for the non-SOFA analytic ITD fallback (`tests/CMakeLists.txt:19-24`). Tests load profiles synchronously via `HRTFDatabase::loadFromFile()` using the `SPATIALCORE_HRTF_DIR` compile definition pointing at the real `HRTF/` directory (`tests/CMakeLists.txt:28-35`).

## Embedded Binary Data (JUCE BinaryData)

**Fonts:**
- `juce_add_binary_data(SpatialCoreUIFontData ...)` target (`CMakeLists.txt:207-219`) embeds 8 TrueType font files as C++ BinaryData, generating header `SpatialCoreUIFontData.h` in namespace `SpatialCoreUIFontData`:
  - `fonts/DM_Sans-Regular.ttf`, `fonts/DM_Sans-Medium.ttf`, `fonts/DM_Sans-SemiBold.ttf`, `fonts/DM_Sans-Bold.ttf`
  - `fonts/JetBrains_Mono-Regular.ttf`, `fonts/JetBrains_Mono-Medium.ttf`, `fonts/JetBrains_Mono-Bold.ttf`
  - `fonts/Roboto-Medium.ttf`
- Consumed by `src/UI/SMLLookAndFeel.cpp` (per `CMakeLists.txt:203-206` comment: "SMLLookAndFeel's constructor loads these via SpatialCoreUIFontData:: symbols"). Relocated from OpenSpatialDelay's own `fonts/` directory during the v2 extraction (per CMake comment, `CMakeLists.txt:200-206`); OSD's parallel BinaryData target was removed in the same commit to avoid a dual-BinaryData font bug.
- SOFA HRTF files are **not** embedded as BinaryData — they remain on-disk under `HRTF/` and are loaded at runtime via `HRTFDatabase::loadFromFile()` (confirmed by `tests/CMakeLists.txt:28-30` passing `SPATIALCORE_HRTF_DIR` as a filesystem path, not a compiled-in resource). **This contradicts CLAUDE.md's "Build System" section**, which claims "HRTF data: 5 SOFA files embedded as BinaryData" — the tree shows raw `.sofa` files loaded from disk at runtime, not compiled into the binary.

## Third-Party Libraries

| Library | Version | Fetch mechanism | Purpose |
|---------|---------|------------------|---------|
| JUCE | 9.0.0 | FetchContent (fallback) or consumer-provided | Framework: core, audio-basics, audio-formats, dsp, osc (DSP target); gui-basics, graphics (UI target) |
| libmysofa | v1.3.2 | FetchContent, guarded by `if(NOT TARGET mysofa-static)` | SOFA HRTF file parsing (`CMakeLists.txt:44-56`) |
| zlib | system | `find_package(ZLIB REQUIRED)` | Required by libmysofa's SOFA/HDF5-lite reader |
| Catch2 | v3.7.1 | FetchContent | Test framework (`tests/CMakeLists.txt:2-6`) |

## Data Storage

**Databases:** None detected — no SQL/NoSQL client libraries, no `find . -iname "*.db"` or ORM-style headers found.

**File Storage:**
- Local filesystem only. HRTF profiles read from `HRTF/*.sofa` at runtime via `HRTFDatabase::loadFromFile()`.
- No cloud storage SDK (S3, GCS, Azure Blob) references found.

**Caching:**
- `SharedFFTCache` (per CLAUDE.md's Binaural component table) is an in-process singleton for FFT plan/buffer reuse, not an external cache service — this is in-memory only, no Redis/Memcached or similar external dependency found.

## Authentication & Identity

Not applicable — SpatialCore is a linked static library with no network-facing auth surface, no user accounts, and no API keys/secrets in the tree (`ls -la .env* credentials* secrets* 2>/dev/null` at repo root returns nothing).

## Monitoring & Observability

**Error Tracking:** None — no Sentry/Bugsnag/Crashlytics SDK found.

**Logs:** No centralized logging framework found; per CLAUDE.md's stated design principle ("no malloc, locks, or logging in any function called from processBlock"), logging is deliberately excluded from the real-time audio path.

## CI/CD & Deployment

**Hosting:** Not applicable — library only, not a deployed service.

**CI Pipeline:**
- GitHub Actions, single workflow `.github/workflows/ci.yml`, job `build-and-test` on `ubuntu-latest`.
- Triggers: push/PR to `main` and `spatialcore-v2-extraction` branches, plus manual `workflow_dispatch`.
- Steps: checkout with `lfs: true` → verify SOFA files are real bytes → install Linux GUI/audio dev packages (see STACK.md) → `cmake -B build` (standalone JUCE 9.0.0 fallback) → build `SpatialCore` + `SpatialCoreTests` → build `SpatialCoreUI` separately → `ctest --output-on-failure`.
- No macOS or Windows CI job present in this workflow file, despite `CMakeLists.txt` containing macOS (`if(APPLE)`) and MSVC-specific branches.

## Environment Configuration

**Required env vars:** None — build is fully driven by CMake cache variables; no `.env`/environment-variable-based runtime configuration found.

**Secrets location:** Not applicable — no secrets present or referenced in the tree.

## Webhooks & Callbacks

**Incoming:** ADM-OSC UDP messages via `ADMOSCReceiver` (see Protocols above) — not HTTP webhooks, but functionally analogous (external control-surface input over a network protocol).

**Outgoing:** ADM-OSC UDP broadcast via `ADMOSCSender` (see Protocols above).

---

*Integration audit: 2026-08-10*
