#include <SpatialCore/Binaural/HRTFProfileResolver.h>

namespace spatialcore
{

//==============================================================================
// Shared folder (D-10): the system folder only. The per-user library is a deferred idea
// and is deliberately not read. Computed on every call (D-11), never created.
//==============================================================================
juce::File getSharedHRTFFolder()
{
#if JUCE_MAC
    // Literal path: JUCE maps commonApplicationDataDirectory to /Library on macOS, and the
    // installer (SUITE-01) writes to /Library/Application Support/Spatial Media Lab/HRTF.
    return juce::File ("/Library/Application Support/Spatial Media Lab/HRTF");
#elif JUCE_WINDOWS
    // %ProgramData%\Spatial Media Lab\HRTF. Implemented, not verified (PROJECT.md Out of Scope).
    return juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
               .getChildFile ("Spatial Media Lab")
               .getChildFile ("HRTF");
#else
    // No shared folder on other platforms: the resolver goes straight to the embedded copy.
    return {};
#endif
}

//==============================================================================
// Resolution chain: shared folder -> embedded copy -> reported error
//==============================================================================
HRTFResolveResult resolveHRTFProfile (int profileIndex,
                                      HRTFDatabase& database,
                                      float sampleRate,
                                      const juce::File& sharedFolder,
                                      juce::int64 maxSharedFileBytes)
{
    if (! isValidHRTFProfileIndex (profileIndex))
    {
        database.unload();
        return { false, HRTFProfileSource::None, HRTFProfileProblem::InvalidIndex };
    }

    if (profileIndex == kHRTFProfileSimple)
    {
        database.unload();
        return { true, HRTFProfileSource::Simple, HRTFProfileProblem::None };
    }

    // 1. Shared folder. The file name comes only from kHRTFProfiles (T-03-10); the folder is
    //    skipped when it is empty or not a directory, so nothing resolves against the working
    //    directory. A plain exists check follows the filesystem's own case rules (D-18).
    bool sharedFileBroken = false;

    if (sharedFolder.getFullPathName().isNotEmpty() && sharedFolder.isDirectory())
    {
        const juce::File sharedFile = sharedFolder.getChildFile (kHRTFProfiles[profileIndex].fileName);

        if (sharedFile.existsAsFile())
        {
            // The size cap is checked before any byte is read (T-03-09).
            if (sharedFile.getSize() > maxSharedFileBytes)
                sharedFileBroken = true;
            else if (database.loadFromFile (sharedFile, sampleRate))
                return { true, HRTFProfileSource::SharedFolder, HRTFProfileProblem::None };
            else
                sharedFileBroken = true;
        }
    }

    // 2. Embedded copy. Silent when the shared file was simply absent; flagged when a
    //    same-name shared file was there but unusable (D-09).
    if (database.loadFromBinaryData (profileIndex, sampleRate))
        return { true, HRTFProfileSource::Embedded,
                 sharedFileBroken ? HRTFProfileProblem::SharedFileUnreadableUsedBuiltIn
                                  : HRTFProfileProblem::None };

    // 3. Nowhere to get it from: report it (D-06). loadFromBinaryData left the database unloaded.
    return { false, HRTFProfileSource::None,
             sharedFileBroken ? HRTFProfileProblem::SharedFileUnreadableNoBuiltIn
                              : HRTFProfileProblem::NotFound };
}

//==============================================================================
// Status text (D-06, D-09). Built from the static table and integers only: never from file
// contents, and never with a path (T-03-12).
//==============================================================================
juce::String describeHRTFProfileStatus (const HRTFProfileStatus& status)
{
    const auto fileNameOf = [] (int profile) -> juce::String
    {
        if (profile >= 1 && profile < kNumHRTFProfileSlots)
            return kHRTFProfiles[profile].fileName;
        return "profile file";
    };

    const juce::String requested (status.requestedProfile);

    switch (status.problem)
    {
        case HRTFProfileProblem::InvalidIndex:
            return "profile " + requested + " failed: no such profile";

        case HRTFProfileProblem::NotFound:
            return "profile " + requested + " failed: file missing";

        case HRTFProfileProblem::SharedFileUnreadableNoBuiltIn:
            return "profile " + requested + " failed: shared file " + fileNameOf (status.requestedProfile)
                   + " unreadable, no built-in copy";

        case HRTFProfileProblem::SharedFileUnreadableUsedBuiltIn:
            return "shared file " + fileNameOf (status.requestedProfile) + " unreadable, used built-in";

        case HRTFProfileProblem::LoadFailed:
            return "profile " + requested + " failed: could not load (out of memory?)";

        case HRTFProfileProblem::None:
            break;
    }

    switch (status.state)
    {
        case HRTFLoadState::Idle:
            return "no profile requested";

        case HRTFLoadState::Loading:
            return "profile " + requested + " loading";

        case HRTFLoadState::Failed:
            return "profile " + requested + " failed";

        case HRTFLoadState::Ready:
        {
            const juce::String active (status.activeProfile);
            switch (status.source)
            {
                case HRTFProfileSource::Simple:       return "profile " + active + " ready (simple model)";
                case HRTFProfileSource::SharedFolder: return "profile " + active + " ready (shared folder)";
                case HRTFProfileSource::Embedded:     return "profile " + active + " ready (built-in)";
                case HRTFProfileSource::None:         break;
            }
            return "profile " + active + " ready";
        }
    }

    return "profile " + requested;
}

} // namespace spatialcore
