#pragma once

#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <SpatialCore/Binaural/HRTFProfile.h>

#include <juce_core/juce_core.h>

namespace spatialcore
{

//==============================================================================
// HRTF profile resolution (DATA-01)
//
// One profile index becomes loaded SOFA data through a fixed chain, re-run on every
// call (D-11, no caching):
//
//   1. shared folder  a file in getSharedHRTFFolder() whose name is the profile's
//                     built-in file name replaces that profile (D-08). Any other file
//                     in the folder is ignored. Whether "same name" is case-blind
//                     follows the filesystem (D-18): a plain exists check, no directory
//                     listing.
//   2. embedded copy  the BinaryData compiled into SpatialCore. Used silently when the
//                     folder or the file is absent; used with
//                     SharedFileUnreadableUsedBuiltIn when a same-name shared file was
//                     there but unusable (D-09).
//   3. reported error NotFound / SharedFileUnreadableNoBuiltIn, never a silent failure
//                     (D-06).
//
// This reads files and parses up to tens of MB: never call it from the audio or the
// message thread. The engine's loader thread is the caller. SpatialCore only READS the
// shared folder; the installer owns it (SUITE-01).
//==============================================================================

/** Outcome of one resolution. `loaded` is true when the database now holds the profile
    (or, for profile 0, when Simple was selected and the database was emptied). */
struct HRTFResolveResult
{
    bool loaded = false;
    HRTFProfileSource source = HRTFProfileSource::None;
    HRTFProfileProblem problem = HRTFProfileProblem::None;
};

/** A shared-folder file larger than this is treated as unreadable without being read
    (T-03-09). The largest built-in profile is 36 MB. */
inline constexpr juce::int64 kMaxSharedHRTFFileBytes = 256 * 1024 * 1024;

/** The system-wide shared HRTF folder (D-10), computed on every call (D-11):
    macOS   /Library/Application Support/Spatial Media Lab/HRTF
    Windows %ProgramData%\Spatial Media Lab\HRTF (implemented, not verified)
    other   juce::File() (no folder; the resolver goes straight to the embedded copy)
    No per-user folder is ever consulted. The folder is never created. */
juce::File getSharedHRTFFolder();

/** Resolve `profileIndex` into `database` at `sampleRate`.

    - an index outside 0..5: database unloaded, { false, None, InvalidIndex }.
    - 0 (Simple): database unloaded, { true, Simple, None }, no file touched.
    - 1..5: shared folder, then embedded copy, then error, as described above.

    An empty or non-directory `sharedFolder` is skipped, never resolved against the
    current directory. `maxSharedFileBytes` is injectable so tests can exercise the cap. */
HRTFResolveResult resolveHRTFProfile (int profileIndex,
                                      HRTFDatabase& database,
                                      float sampleRate,
                                      const juce::File& sharedFolder,
                                      juce::int64 maxSharedFileBytes = kMaxSharedHRTFFileBytes);

} // namespace spatialcore
