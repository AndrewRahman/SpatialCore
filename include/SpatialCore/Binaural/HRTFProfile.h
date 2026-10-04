#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>

namespace spatialcore
{

//==============================================================================
// HRTF profile table (DATA-01, D-07)
//
// Profile numbering is OpenSpatialDelay's and consumers persist it in saved
// sessions. NEVER renumber a profile and never change a display name: a saved
// session reopens with whatever profile its stored index now names.
//
//   0  Simple (Low CPU)     Woodworth ITD + ILD, no convolution, no file
//   1  sadie_d2_ku100       Immersive
//   2  cipic_subject_003    Natural
//   3  hutubs_pp2           Precise
//   4  bernschuetz_ku100    Spatial
//   5  mit_kemar_large_pinna Studio Reference (always embedded)
//
// kHRTFProfiles is the only place a profile index meets a filename.
//==============================================================================
inline constexpr int kHRTFProfileSimple = 0;
inline constexpr int kNumHRTFProfileSlots = 6;

/** The one profile that is embedded even when SPATIALCORE_EMBED_ALL_HRTF is OFF. */
inline constexpr int kAlwaysEmbeddedHRTFProfile = 5;

struct HRTFProfileInfo
{
    const char* fileName;      // shared-folder / source-tree file name; nullptr for profile 0
    const char* resourceName;  // BinaryData symbol name (file name with '.' -> '_'); nullptr for profile 0
    const char* displayName;
};

inline constexpr HRTFProfileInfo kHRTFProfiles[kNumHRTFProfileSlots] = {
    { nullptr,                      nullptr,                    "Simple (Low CPU)"  },
    { "sadie_d2_ku100.sofa",        "sadie_d2_ku100_sofa",      "Immersive"         },
    { "cipic_subject_003.sofa",     "cipic_subject_003_sofa",   "Natural"           },
    { "hutubs_pp2.sofa",            "hutubs_pp2_sofa",          "Precise"           },
    { "bernschuetz_ku100.sofa",     "bernschuetz_ku100_sofa",   "Spatial"           },
    { "mit_kemar_large_pinna.sofa", "mit_kemar_large_pinna_sofa", "Studio Reference" },
};

/** True for 0..5 (Simple plus the five SOFA profiles). */
inline constexpr bool isValidHRTFProfileIndex (int index)
{
    return index >= 0 && index < kNumHRTFProfileSlots;
}

//==============================================================================
// Status types (resolver result and engine-reported state)
//==============================================================================
enum class HRTFLoadState : uint8_t { Idle, Loading, Ready, Failed };

enum class HRTFProfileSource : uint8_t { None, Simple, SharedFolder, Embedded };

enum class HRTFProfileProblem : uint8_t
{
    None,
    InvalidIndex,
    NotFound,
    SharedFileUnreadableUsedBuiltIn,
    SharedFileUnreadableNoBuiltIn,
    LoadFailed   // the load itself threw (for example out of memory); appended, existing values unchanged
};

struct HRTFProfileStatus
{
    int requestedProfile = 0;
    int activeProfile = 0;
    HRTFLoadState state = HRTFLoadState::Idle;
    HRTFProfileSource source = HRTFProfileSource::Simple;
    HRTFProfileProblem problem = HRTFProfileProblem::None;
};

/** Human-readable one-line status for a plugin UI (D-06, D-09). Built only from the
    static profile table and numbers, never from file contents or paths (T-03-12).
    Allocates: call from the message thread, never the audio thread. */
juce::String describeHRTFProfileStatus (const HRTFProfileStatus& status);

} // namespace spatialcore
