#pragma once

// Umbrella include — DSP modules only (Phase 8 locked scope, D-03 split).
// UI headers (SpatialMapComponent, SMLLookAndFeel, ReverseSlider,
// IndicatorToggle, StyledButton) are deliberately NOT included here; they
// land in a separate SpatialCoreUI umbrella in Phase 9. Consumers that need
// UI widgets include <SpatialCore/UI/*.h> directly for now.

// Core
#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Core/SourcePosition.h>
#include <SpatialCore/Core/BinauralGains.h>
#include <SpatialCore/Core/SpatialMath.h>
#include <SpatialCore/Core/SimpleBinauralCues.h>

// Algorithms
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>
#include <SpatialCore/Algorithms/VBAPAlgorithm.h>
#include <SpatialCore/Algorithms/VBIPAlgorithm.h>
#include <SpatialCore/Algorithms/KNNAlgorithm.h>
#include <SpatialCore/Algorithms/DBAPAlgorithm.h>
#include <SpatialCore/Algorithms/MDAPAlgorithm.h>
#include <SpatialCore/Algorithms/AmbisonicsAlgorithm.h>
#include <SpatialCore/Algorithms/DirectBinauralAlgorithm.h>
#include <SpatialCore/Algorithms/ConstantPowerAlgorithm.h>
#include <SpatialCore/Algorithms/AllAlgorithms.h>

// Binaural
#include <SpatialCore/Binaural/SharedFFTCache.h>
#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include <SpatialCore/Binaural/BinauralRenderer.h>
#include <SpatialCore/Binaural/HRTFProfile.h>
#include <SpatialCore/Binaural/HRTFProfileResolver.h>

// IO
#include <SpatialCore/IO/OutputFormat.h>
#include <SpatialCore/IO/OutputFormatRegistry.h>
#include <SpatialCore/IO/SpeakerLayout.h>
#include <SpatialCore/IO/AmbisonicsCodec.h>

// OSC
#include <SpatialCore/OSC/ADMOSCReceiver.h>
#include <SpatialCore/OSC/ADMOSCSender.h>

// Trajectory
#include <SpatialCore/Trajectory/TrajectoryEngine.h>
#include <SpatialCore/Trajectory/DopplerVelocity.h>

// Engine (Phase 8 Plan 08-06, CORE-01 — the per-object rendering engine facade)
#include <SpatialCore/Engine/RenderEngine.h>
