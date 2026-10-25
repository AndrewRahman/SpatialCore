#pragma once

// Umbrella include for the DSP library (target SpatialCore): every public
// header except the UI ones. For the spatial map, the SML look-and-feel and
// the widgets, link SpatialCoreUI and include <SpatialCore/SpatialCoreUI.h>.
// The two umbrellas are kept apart on purpose, so a DSP-only consumer never
// pulls in JUCE GUI headers (D-02). tests/Core/UmbrellaTests.cpp keeps both
// complete.

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
#include <SpatialCore/Binaural/TransitionTiming.h>
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
#include <SpatialCore/OSC/ADMPositionQuery.h>
#include <SpatialCore/OSC/OSCPortValidation.h>

// Trajectory
#include <SpatialCore/Trajectory/TrajectoryEngine.h>
#include <SpatialCore/Trajectory/DopplerVelocity.h>

// Engine (the consumer-facing render facade)
#include <SpatialCore/Engine/RenderEngine.h>
