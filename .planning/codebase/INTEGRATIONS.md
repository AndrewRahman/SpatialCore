# External Integrations

**Analysis Date:** 2026-08-09

## APIs & External Services

**ADM-OSC (Audio Definition Model - Open Sound Control):**
- **Service:** Open Sound Control (OSC) protocol for spatial audio object control
- **What it's used for:** Receive 3D position updates for audio objects from external spatial editors; broadcast 3D positions at 30Hz
- **SDK/Client:** JUCE OSCReceiver/OSCSender (from `juce::juce_osc`)
- **Protocol Details:**
  - **Receive endpoint:** `/adm/obj/N/azim`, `/adm/obj/N/elev`, `/adm/obj/N/dist`, `/adm/obj/N/aed`, `/adm/obj/N/xyz`
  - **Default receive port:** 4002 (configurable via `ADMOSCReceiver::connect(int port)`)
  - **Coordinate system:** Polar (azimuth/elevation/distance) or Cartesian (x/y/z)
  - **Conversion:** Automatic Cartesian-to-Polar conversion per ITU-R BS.2127-0
  - **Send broadcast:** `/adm/obj/N/aed` at 30Hz with position-change gating
  - **Implementation files:**
    - `include/SpatialCore/OSC/ADMOSCReceiver.h`
    - `include/SpatialCore/OSC/ADMOSCSender.h`
    - `src/OSC/ADMOSCReceiver.cpp`
    - `src/OSC/ADMOSCSender.cpp`

## Data Storage

**Databases:**
- None — SpatialCore is a stateless audio rendering library

**File Storage:**
- **HRTF Data:** Head-Related Transfer Functions (HRTF) from SOFA files
  - Format: SOFA (Spatially Oriented Format for Acoustics) v1.1+
  - Location: Embedded as BinaryData in compiled library (5 SOFA files)
  - Profiles included:
    - Simple (Woodworth ITD+ILD model — no file, procedural)
    - MIT KEMAR (public domain SOFA)
    - SADIE II D2 (license: CC BY)
    - CIPIC Subject003 (license: CC BY)
    - HUTUBS PP2 (license: CC BY)
    - Bernschuetz KU100 (license: CC BY)
  - Loading mechanism: `HRTFDatabase::loadFromMemory(const void* data, int dataSize, float targetSampleRate)`
  - Parser: libmysofa v1.3.2 (via `extern "C" #include "mysofa.h"`)
  - Resampling: Done at load time to match audio sample rate
  - Storage format: Not accessible via standard filesystem — embedded in binary

**Local filesystem only:**
- No remote file storage
- Consumer plugins may embed SpatialCore as git submodule, loading HRTF data from compiled binary

**Caching:**
- **Shared FFT Cache:** Process-global FFT singleton (`SharedFFTCache`) shared across all PartitionedConvolvers
  - Purpose: Avoid redundant FFT allocations in binaural rendering
  - Thread-safe: Lock-free audio path guarantees (no malloc/locks in processBlock)

## Authentication & Identity

**Auth Provider:**
- None — SpatialCore is library code with no user authentication

**Authorization:**
- Not applicable

## Monitoring & Observability

**Error Tracking:**
- None — No error telemetry or remote logging

**Logs:**
- Console output via JUCE Logger (debug builds only)
- No log aggregation or remote logging
- No structured logging framework

**Debugging:**
- Standard C++ exception handling (no try/catch in audio thread per lock-free constraint)
- Assertions for development
- No production telemetry

## CI/CD & Deployment

**Hosting:**
- Open-source: GitHub (https://github.com/Spatial-Media-Lab/SpatialCore)
- Distribution: Git submodule + CMake integration for consumer plugins

**CI Pipeline:**
- Not detected in this repository (CI likely managed by consumer plugins that link SpatialCore)

**Build System:**
- CMake 3.22+
- FetchContent for dependency management (JUCE, libmysofa, Catch2)

## Plugin Integration

**Consumer Plugins:**
- Plugins integrate SpatialCore via:
  1. Git submodule: `git submodule add https://github.com/Spatial-Media-Lab/SpatialCore.git SpatialCore`
  2. CMake: `add_subdirectory(SpatialCore)` + `target_link_libraries(MyPlugin PRIVATE SpatialCore)`
  3. Header: `#include <SpatialCore/SpatialCore.h>`

**Version Pinning:**
- Each plugin pins to a specific SpatialCore commit via submodule pointer
- Semantic versioning enforced: `vMAJOR.MINOR.PATCH`

## Environment Configuration

**Required env vars:**
- None — SpatialCore operates via CMake configuration at build time
- OSC receive port can be configured at runtime: `ADMOSCReceiver::connect(int port)`

**Secrets location:**
- Not applicable — No API keys, credentials, or secrets required

## Webhooks & Callbacks

**Incoming:**
- ADM-OSC UDP messages (port 4002 by default)
- Listener pattern: `ADMOSCReceiver::Listener` with `admPositionReceived()` callback

**Outgoing:**
- ADM-OSC UDP broadcasts at 30Hz from `ADMOSCSender`
- Target: External spatial editors or control systems

## Third-Party Data Sources

**HRTF Datasets:**
- **Public HRTF sources:**
  - MIT KEMAR HRTF Database (public domain)
  - SADIE II Head and Torso Simulator (CC BY license)
  - CIPIC HRTF Database (CC BY license)
  - HUTUBS HRTF Dataset (CC BY license)
  - Bernschuetz HRTF Database (CC BY license)
- **Integration:** SOFA files pre-processed and embedded as BinaryData
- **License compliance:** All third-party HRTF profiles bundled with their respective licenses

## Network & Communication

**Network Protocols:**
- OSC (Open Sound Control) over UDP
- No HTTP/HTTPS APIs
- No WebSocket connections
- No gRPC or other RPC mechanisms

**Port Configuration:**
- **OSC Receive:** Port 4002 (default, configurable)
- **OSC Send:** Broadcast destination configurable by consumer plugin

## Data Validation

**Input Validation:**
- SOFA file validation via libmysofa (file format validation)
- OSC message parsing via JUCE OSCReceiver (message format validation)
- Spatial coordinates: Azimuth (0-360°), Elevation (-90 to +90°), Distance (positive)

**Output Validation:**
- Gain computation: Normalization ensures consistent output levels
- HRTF convolution: PartitionedConvolver validates IR length and FFT size

## Security Considerations

**HRTF Data:**
- Embedded at build time (no runtime file I/O for HRTF data)
- No network fetch of HRTF profiles

**OSC Input:**
- UDP port 4002 listens for ADM-OSC messages
- No authentication on OSC receive (assumes trusted network)
- Consumer plugins should validate sender if deployed over untrusted networks

**Library Code:**
- No remote code execution
- No dynamic library loading
- Static linking prevents runtime DLL/SO injection attacks

---

*Integration audit: 2026-08-09*
