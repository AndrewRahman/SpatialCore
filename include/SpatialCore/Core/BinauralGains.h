#pragma once

// BinauralGains now lives in Types.h (consolidated in Phase 8 Plan 08-02 —
// see .planning/phases/08-spatialcore-dsp-extraction/08-02-PLAN.md). This
// header is kept as a compatibility shim so existing #include paths continue
// to resolve without forcing every consumer to be touched in this plan's
// scope (SpatialCore.h, ADMOSCReceiver.h are out of scope for 08-02 — they
// move in later waves).
#include <SpatialCore/Core/Types.h>
