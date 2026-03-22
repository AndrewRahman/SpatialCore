#pragma once

// Core
#include <SpatialCore/Core/Types.h>
#include <SpatialCore/Core/SourcePosition.h>
#include <SpatialCore/Core/BinauralGains.h>

// Algorithms
#include <SpatialCore/Algorithms/SpatializationAlgorithm.h>
#include <SpatialCore/Algorithms/VBAPAlgorithm.h>
#include <SpatialCore/Algorithms/VBIPAlgorithm.h>
#include <SpatialCore/Algorithms/KNNAlgorithm.h>
#include <SpatialCore/Algorithms/DBAPAlgorithm.h>
#include <SpatialCore/Algorithms/MDAPAlgorithm.h>
#include <SpatialCore/Algorithms/AmbisonicsAlgorithm.h>
#include <SpatialCore/Algorithms/DirectBinauralAlgorithm.h>

// Binaural
#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <SpatialCore/Binaural/PartitionedConvolver.h>
#include <SpatialCore/Binaural/BinauralRenderer.h>

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

// DSP
#include <SpatialCore/DSP/Utilities.h>

// UI
#include <SpatialCore/UI/SpatialMapComponent.h>
#include <SpatialCore/UI/SMLLookAndFeel.h>
#include <SpatialCore/UI/ReverseSlider.h>
#include <SpatialCore/UI/IndicatorToggle.h>
#include <SpatialCore/UI/StyledButton.h>
