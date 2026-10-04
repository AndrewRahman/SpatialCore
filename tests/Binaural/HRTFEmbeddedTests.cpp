#include <catch2/catch_test_macros.hpp>
#include "BinauralMetrics.h"

#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <SpatialCore/Binaural/BinauralRenderer.h>
#include <SpatialCore/Binaural/HRTFProfile.h>
#include <SpatialCore/Binaural/HRTFProfileResolver.h>

#include <cstdint>

#if JUCE_MAC || JUCE_LINUX
 #include <sys/stat.h>
#endif

using namespace spatialcore;
using namespace spatialcore::test;

// ============================================================================
// HRTF packaging tests (DATA-01).
//
// [hrtf-embed]   the source-tree SOFA files are real HDF5, the compiled-in copies equal them,
//                and loadFromBinaryData loads each by profile number.
// [hrtf-resolve] the resolution chain: shared folder (D-08, D-10, D-11, D-18), embedded copy
//                (D-09), reported error (D-06). Every test runs in both embedding modes
//                (SPATIALCORE_EMBED_ALL_HRTF ON and OFF); where the modes differ the test
//                says so.
// ============================================================================

TEST_CASE ("HRTF files: every source-tree SOFA file is real HDF5, not a Git LFS pointer",
           "[hrtf-embed][signature]")
{
    // An unresolved Git LFS pointer is a ~130 byte text stub; every HRTF test after it would
    // measure nothing. The HDF5 file signature is 89 48 44 46 0D 0A 1A 0A. Mirrors the LFS
    // guard in .github/workflows/ci.yml.
    constexpr uint8_t kSignature[8] = { 0x89, 0x48, 0x44, 0x46, 0x0D, 0x0A, 0x1A, 0x0A };

    for (int profile = 1; profile <= 5; ++profile)
    {
        const juce::File file = getSofaFile (testProfileFile (profile));
        INFO (file.getFullPathName());
        REQUIRE (file.existsAsFile());
        CHECK (file.getSize() > 1000000);

        juce::FileInputStream stream (file);
        REQUIRE (stream.openedOk());

        uint8_t head[8] = {};
        REQUIRE (stream.read (head, 8) == 8);
        for (int i = 0; i < 8; ++i)
        {
            INFO ("signature byte " << i);
            CHECK (head[i] == kSignature[i]);
        }
    }
}

namespace
{
    /** IR length of each built-in profile at 48 kHz, index = profile (0 unused). */
    constexpr int kExpectedIRLength[6] = { 0, 256, 218, 279, 128, 558 };

    /** True when profile p is compiled into this build. */
    constexpr bool profileIsEmbedded (int p)
    {
#if SPATIALCORE_EMBEDS_ALL_HRTF
        return p >= 1 && p <= 5;
#else
        return p == kAlwaysEmbeddedHRTFProfile;
#endif
    }
}

TEST_CASE ("HRTF embed: embedded bytes equal the source file and load",
           "[hrtf-embed][bytes]")
{
    for (int profile = 1; profile <= 5; ++profile)
    {
        INFO ("profile " << profile);
        int size = 0;
        const char* data = HRTFDatabase::getEmbeddedProfileData (profile, size);

        if (! profileIsEmbedded (profile))
        {
            // OFF build: profiles 1-4 are simply absent, reported without any #if in the lookup.
            CHECK (data == nullptr);
            CHECK (size == 0);
            continue;
        }

        REQUIRE (data != nullptr);

        const juce::File file = getSofaFile (testProfileFile (profile));
        juce::MemoryBlock fileBytes;
        REQUIRE (file.loadFileAsData (fileBytes));

        CHECK (static_cast<size_t> (size) == fileBytes.getSize());
        CHECK (fnv1aHash (data, static_cast<size_t> (size)) == fnv1aHash (fileBytes.getData(), fileBytes.getSize()));

        HRTFDatabase db;
        REQUIRE (db.loadFromMemory (data, size, 48000.0f));
        CHECK (db.getIRLength() == kExpectedIRLength[profile]);
    }
}

TEST_CASE ("HRTF embed: the profile table matches the files and the D-07 numbering",
           "[hrtf-embed][table]")
{
    CHECK (kNumHRTFProfileSlots == 6);
    CHECK (kHRTFProfiles[0].fileName == nullptr);
    CHECK (juce::String (kHRTFProfiles[0].displayName) == "Simple (Low CPU)");

    // D-07: display names and numbering are persisted by consumers and never change.
    const char* const expectedNames[6] = { "Simple (Low CPU)", "Immersive", "Natural", "Precise", "Spatial", "Studio Reference" };
    for (int p = 0; p < kNumHRTFProfileSlots; ++p)
        CHECK (juce::String (kHRTFProfiles[p].displayName) == expectedNames[p]);

    for (int p = 1; p <= 5; ++p)
    {
        INFO ("profile " << p);
        const juce::String fileName (kHRTFProfiles[p].fileName);
        CHECK (fileName == juce::String (testProfileFile (p)));
        CHECK (juce::String (kHRTFProfiles[p].resourceName) == fileName.replaceCharacter ('.', '_'));
        CHECK (getSofaFile (kHRTFProfiles[p].fileName).existsAsFile());
    }

    CHECK (isValidHRTFProfileIndex (0));
    CHECK (isValidHRTFProfileIndex (5));
    CHECK_FALSE (isValidHRTFProfileIndex (-1));
    CHECK_FALSE (isValidHRTFProfileIndex (6));
}

TEST_CASE ("HRTF embed: loadFromBinaryData loads each embedded profile by number",
           "[hrtf-embed][load]")
{
    HRTFDatabase db;

    for (int profile = 1; profile <= 5; ++profile)
    {
        INFO ("profile " << profile);
        const bool ok = db.loadFromBinaryData (profile, 48000.0f);
        CHECK (ok == profileIsEmbedded (profile));

        if (profileIsEmbedded (profile))
        {
            CHECK (db.isLoaded());
            CHECK (db.getIRLength() == kExpectedIRLength[profile]);
        }
        else
        {
            CHECK_FALSE (db.isLoaded());
        }
    }

    // Anything that is not a file-backed profile fails and leaves the database unloaded,
    // even when it held a profile a moment ago.
    for (const int bad : { 0, -1, 6, 100 })
    {
        INFO ("index " << bad);
        REQUIRE (db.loadFromBinaryData (kAlwaysEmbeddedHRTFProfile, 48000.0f));
        CHECK_FALSE (db.loadFromBinaryData (bad, 48000.0f));
        CHECK_FALSE (db.isLoaded());
    }
}

// ============================================================================
// Resolution chain ([hrtf-resolve])
// ============================================================================
namespace
{
    /** A unique folder under the system temp directory, deleted with the object. */
    struct TempFolder
    {
        juce::File dir;

        explicit TempFolder (const juce::String& tag, bool create = true)
            : dir (juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("sc-hrtf-" + tag + "-"
                                      + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64())))
        {
            if (create)
                dir.createDirectory();
        }

        ~TempFolder() { dir.deleteRecursively(); }
    };

    /** The profile the shared-file tests replace. Profile 3 (HUTUBS, IR 279) when all five are
        embedded; KEMAR (profile 5, IR 558) in an OFF build, where it is the only profile that
        has an embedded copy to fall back to. */
    constexpr int kProbeProfile =
#if SPATIALCORE_EMBEDS_ALL_HRTF
        3;
#else
        kAlwaysEmbeddedHRTFProfile;
#endif
    // The probe must have a built-in copy to fall back to in either embedding mode, so the
    // fallback assertions below always run (an `if (profileIsEmbedded (...))` around them could
    // silently assert nothing). The OFF-build "no built-in copy" outcome is asserted by the
    // [kemar-only] case.
    static_assert (profileIsEmbedded (kProbeProfile), "the probe profile must be embedded in every build");

    /** The profile whose file stands in as the "custom" content dropped into the folder. */
    constexpr int kDonorProfile = 2;   // CIPIC, IR 218

    juce::String probeFileName() { return kHRTFProfiles[kProbeProfile].fileName; }

    /** Copy the donor profile's real file into `folder` under `asName`. */
    void dropDonorInto (const juce::File& folder, const juce::String& asName)
    {
        REQUIRE (getSofaFile (testProfileFile (kDonorProfile)).copyFileTo (folder.getChildFile (asName)));
    }

    /** Write `bytes` into `folder` as the probe profile's file. */
    void dropBytesAsProbe (const juce::File& folder, const juce::MemoryBlock& bytes)
    {
        // replaceWithData() deletes the file for a zero-length payload, so create it explicitly.
        const juce::File target = folder.getChildFile (probeFileName());
        if (bytes.getSize() == 0)
            REQUIRE (target.create().wasOk());
        else
            REQUIRE (target.replaceWithData (bytes.getData(), bytes.getSize()));
        REQUIRE (target.existsAsFile());
        CHECK (target.getSize() == static_cast<juce::int64> (bytes.getSize()));
    }

    juce::MemoryBlock realFileBytes (int profile)
    {
        juce::MemoryBlock bytes;
        REQUIRE (getSofaFile (testProfileFile (profile)).loadFileAsData (bytes));
        return bytes;
    }

    /** Renders a unit impulse through `renderer` at 90 degrees left and reports whether the
        result is finite and audibly non-silent. */
    bool rendersNonSilentImpulse (BinauralRenderer& renderer, int profile)
    {
        renderer.setProfile (profile);
        renderer.updateSourceHRIR (0, juce::MathConstants<float>::halfPi, 0.0f);

        constexpr int kBlock = 512;
        constexpr int kBlocks = 8;
        std::vector<float> input (static_cast<size_t> (kBlock * kBlocks), 0.0f);
        input[0] = 1.0f;

        bool enabled[MAX_SOURCES] = {};
        enabled[0] = true;
        std::vector<float> outL (kBlock), outR (kBlock);
        float peak = 0.0f;
        bool finite = true;

        for (int b = 0; b < kBlocks; ++b)
        {
            const float* bufs[MAX_SOURCES];
            for (auto& ptr : bufs)
                ptr = input.data() + b * kBlock;

            renderer.renderSourceBuffers (bufs, enabled, MAX_SOURCES, kBlock, outL.data(), outR.data());
            for (int i = 0; i < kBlock; ++i)
            {
                finite = finite && std::isfinite (outL[(size_t) i]) && std::isfinite (outR[(size_t) i]);
                peak = std::max ({ peak, std::abs (outL[(size_t) i]), std::abs (outR[(size_t) i]) });
            }
        }
        return finite && peak > 1.0e-4f;
    }

    /** Restores the working directory when it goes out of scope. */
    struct ScopedWorkingDirectory
    {
        juce::File previous = juce::File::getCurrentWorkingDirectory();
        explicit ScopedWorkingDirectory (const juce::File& dir) { dir.setAsCurrentWorkingDirectory(); }
        ~ScopedWorkingDirectory() { previous.setAsCurrentWorkingDirectory(); }
    };
}

TEST_CASE ("HRTF resolve: with no shared folder every embedded profile resolves silently",
           "[hrtf-resolve][embedded]")
{
    const TempFolder missing ("missing", false);   // never created
    REQUIRE_FALSE (missing.dir.exists());

    for (int profile = 1; profile <= 5; ++profile)
    {
        INFO ("profile " << profile);
        BinauralRenderer renderer;
        renderer.prepare (48000.0, 512);

        const HRTFResolveResult r = resolveHRTFProfile (profile, renderer.hrtfDatabase, 48000.0f, missing.dir);

        if (profileIsEmbedded (profile))
        {
            CHECK (r.loaded);
            CHECK (r.source == HRTFProfileSource::Embedded);
            CHECK (r.problem == HRTFProfileProblem::None);
            CHECK (renderer.hrtfDatabase.getIRLength() == kExpectedIRLength[profile]);
            CHECK (rendersNonSilentImpulse (renderer, profile));
        }
        else
        {
            // OFF build: nothing compiled in and nothing in the folder.
            CHECK_FALSE (r.loaded);
            CHECK (r.source == HRTFProfileSource::None);
            CHECK (r.problem == HRTFProfileProblem::NotFound);
            CHECK_FALSE (renderer.hrtfDatabase.isLoaded());
        }
    }
}

TEST_CASE ("HRTF resolve: a same-name shared file replaces the profile and nothing else does",
           "[hrtf-resolve][shared]")
{
    const TempFolder folder ("shared");
    dropDonorInto (folder.dir, probeFileName());          // CIPIC bytes under the probe's name
    dropDonorInto (folder.dir, "custom.sofa");            // a name that is not a built-in file
    dropDonorInto (folder.dir, "extra_profile.sofa");

    HRTFDatabase db;
    const HRTFResolveResult r = resolveHRTFProfile (kProbeProfile, db, 48000.0f, folder.dir);
    CHECK (r.loaded);
    CHECK (r.source == HRTFProfileSource::SharedFolder);
    CHECK (r.problem == HRTFProfileProblem::None);
    CHECK (db.getIRLength() == kExpectedIRLength[kDonorProfile]);   // the CIPIC IR, not the built-in one
    CHECK (db.getIRLength() != kExpectedIRLength[kProbeProfile]);

    // The other profiles are untouched: the extra files change nothing for any of them.
    for (int profile = 1; profile <= 5; ++profile)
    {
        if (profile == kProbeProfile)
            continue;

        INFO ("profile " << profile);
        HRTFDatabase other;
        const HRTFResolveResult o = resolveHRTFProfile (profile, other, 48000.0f, folder.dir);

        if (profileIsEmbedded (profile))
        {
            CHECK (o.loaded);
            CHECK (o.source == HRTFProfileSource::Embedded);
            CHECK (o.problem == HRTFProfileProblem::None);
            CHECK (other.getIRLength() == kExpectedIRLength[profile]);
        }
        else
        {
            CHECK_FALSE (o.loaded);
            CHECK (o.problem == HRTFProfileProblem::NotFound);
        }
    }
}

TEST_CASE ("HRTF resolve: a missing or empty shared folder falls back silently",
           "[hrtf-resolve][missing]")
{
    HRTFDatabase db;
    const auto checkEmbedded = [&] (const HRTFResolveResult& r)
    {
        CHECK (r.loaded);
        CHECK (r.source == HRTFProfileSource::Embedded);
        CHECK (r.problem == HRTFProfileProblem::None);
        CHECK (db.getIRLength() == kExpectedIRLength[kProbeProfile]);
    };

    SECTION ("a folder that does not exist")
    {
        const TempFolder missing ("nofolder", false);
        checkEmbedded (resolveHRTFProfile (kProbeProfile, db, 48000.0f, missing.dir));
        CHECK_FALSE (missing.dir.exists());   // SpatialCore only reads; it never creates the folder
    }

    SECTION ("a folder that exists and is empty")
    {
        const TempFolder empty ("empty");
        checkEmbedded (resolveHRTFProfile (kProbeProfile, db, 48000.0f, empty.dir));
        CHECK (empty.dir.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0);
    }

    SECTION ("a path that is a file, not a folder")
    {
        const TempFolder holder ("notadir");
        const juce::File asFile = holder.dir.getChildFile ("HRTF");
        REQUIRE (asFile.replaceWithText ("not a folder"));
        checkEmbedded (resolveHRTFProfile (kProbeProfile, db, 48000.0f, asFile));
    }

    SECTION ("an empty juce::File never resolves against the working directory")
    {
        const TempFolder cwdFolder ("cwd");
        dropDonorInto (cwdFolder.dir, probeFileName());   // a same-name decoy in the cwd
        const ScopedWorkingDirectory cwd (cwdFolder.dir);

        checkEmbedded (resolveHRTFProfile (kProbeProfile, db, 48000.0f, juce::File()));
    }
}

TEST_CASE ("HRTF resolve: an unusable same-name shared file falls back to the embedded copy and says so",
           "[hrtf-resolve][broken]")
{
    const TempFolder folder ("broken");
    const juce::MemoryBlock real = realFileBytes (kProbeProfile);
    juce::int64 cap = kMaxSharedHRTFFileBytes;
    juce::MemoryBlock content;

    SECTION ("Git LFS pointer text")
    {
        const juce::String stub = "version https://git-lfs.github.com/spec/v1\n"
                                  "oid sha256:0000000000000000000000000000000000000000000000000000000000000000\n"
                                  "size 1658802\n";
        content.append (stub.toRawUTF8(), stub.getNumBytesAsUTF8());
    }
    SECTION ("zero bytes")
    {
        // content stays empty
    }
    SECTION ("the first 100 KB of the real file")
    {
        content.append (real.getData(), 100 * 1024);
    }
    SECTION ("200 KB of seeded random bytes")
    {
        juce::Random rng (12345);
        for (int i = 0; i < 200 * 1024; ++i)
        {
            const char byte = static_cast<char> (rng.nextInt (256));
            content.append (&byte, 1);
        }
    }
    SECTION ("a real file above the size cap")
    {
        content = real;
        cap = 1000;
    }

    dropBytesAsProbe (folder.dir, content);

    HRTFDatabase db;
    const HRTFResolveResult r = resolveHRTFProfile (kProbeProfile, db, 48000.0f, folder.dir, cap);

    CHECK (r.loaded);
    CHECK (r.source == HRTFProfileSource::Embedded);
    CHECK (r.problem == HRTFProfileProblem::SharedFileUnreadableUsedBuiltIn);
    CHECK (db.getIRLength() == kExpectedIRLength[kProbeProfile]);
}

TEST_CASE ("HRTF resolve: the folder is checked on every call, nothing is cached (D-11)",
           "[hrtf-resolve][d11]")
{
    const TempFolder folder ("d11");
    HRTFDatabase db;

    const HRTFResolveResult before = resolveHRTFProfile (kProbeProfile, db, 48000.0f, folder.dir);
    CHECK (before.source == HRTFProfileSource::Embedded);
    CHECK (db.getIRLength() == kExpectedIRLength[kProbeProfile]);

    dropDonorInto (folder.dir, probeFileName());

    const HRTFResolveResult after = resolveHRTFProfile (kProbeProfile, db, 48000.0f, folder.dir);
    CHECK (after.source == HRTFProfileSource::SharedFolder);
    CHECK (db.getIRLength() == kExpectedIRLength[kDonorProfile]);

    REQUIRE (folder.dir.getChildFile (probeFileName()).deleteFile());

    const HRTFResolveResult removed = resolveHRTFProfile (kProbeProfile, db, 48000.0f, folder.dir);
    CHECK (removed.source == HRTFProfileSource::Embedded);
    CHECK (db.getIRLength() == kExpectedIRLength[kProbeProfile]);
}

TEST_CASE ("HRTF resolve: same-name matching follows the filesystem's own case rules (D-18)",
           "[hrtf-resolve][case]")
{
    const TempFolder folder ("case");

    // Detect the volume's behaviour the way the resolver meets it: create one name, ask for another.
    REQUIRE (folder.dir.getChildFile ("CaseProbe.tmp").replaceWithText ("x"));
    const bool caseInsensitive = folder.dir.getChildFile ("caseprobe.tmp").existsAsFile();
    REQUIRE (folder.dir.getChildFile ("CaseProbe.tmp").deleteFile());

    const juce::String upper = probeFileName().upToLastOccurrenceOf (".", false, false).toUpperCase() + ".sofa";
    REQUIRE (upper != probeFileName());
    dropDonorInto (folder.dir, upper);

    HRTFDatabase db;
    const HRTFResolveResult r = resolveHRTFProfile (kProbeProfile, db, 48000.0f, folder.dir);
    CHECK (r.loaded);
    INFO ("volume is " << (caseInsensitive ? "case-insensitive" : "case-sensitive"));
    CHECK (r.source == (caseInsensitive ? HRTFProfileSource::SharedFolder : HRTFProfileSource::Embedded));
}

TEST_CASE ("HRTF resolve: an invalid index is an error and Simple touches no file",
           "[hrtf-resolve][invalid]")
{
    const TempFolder folder ("invalid");
    HRTFDatabase db;

    for (const int bad : { -1, 6, 9, 100 })
    {
        INFO ("index " << bad);
        REQUIRE (db.loadFromBinaryData (kAlwaysEmbeddedHRTFProfile, 48000.0f));   // something loaded first

        const HRTFResolveResult r = resolveHRTFProfile (bad, db, 48000.0f, folder.dir);
        CHECK_FALSE (r.loaded);
        CHECK (r.source == HRTFProfileSource::None);
        CHECK (r.problem == HRTFProfileProblem::InvalidIndex);
        CHECK_FALSE (db.isLoaded());
    }

    REQUIRE (db.loadFromBinaryData (kAlwaysEmbeddedHRTFProfile, 48000.0f));
    const HRTFResolveResult simple = resolveHRTFProfile (kHRTFProfileSimple, db, 48000.0f, folder.dir);
    CHECK (simple.loaded);
    CHECK (simple.source == HRTFProfileSource::Simple);
    CHECK (simple.problem == HRTFProfileProblem::None);
    CHECK_FALSE (db.isLoaded());
}

TEST_CASE ("HRTF resolve: status text comes from the profile table and numbers only",
           "[hrtf-resolve][status-text]")
{
    HRTFProfileStatus s;

    s.requestedProfile = s.activeProfile = 3;
    s.state = HRTFLoadState::Failed;
    s.source = HRTFProfileSource::None;
    s.problem = HRTFProfileProblem::NotFound;
    CHECK (describeHRTFProfileStatus (s).contains ("profile 3 failed: file missing"));

    s.state = HRTFLoadState::Ready;
    s.source = HRTFProfileSource::Embedded;
    s.problem = HRTFProfileProblem::SharedFileUnreadableUsedBuiltIn;
    CHECK (describeHRTFProfileStatus (s).contains ("shared file hutubs_pp2.sofa unreadable, used built-in"));

    s.requestedProfile = s.activeProfile = 9;
    s.state = HRTFLoadState::Failed;
    s.source = HRTFProfileSource::None;
    s.problem = HRTFProfileProblem::InvalidIndex;
    CHECK (describeHRTFProfileStatus (s).contains ("profile 9 failed: no such profile"));

    // Every state x problem x source combination yields a non-empty line, for any index.
    for (const int profile : { -3, 0, 1, 3, 5, 6, 40 })
        for (const auto state : { HRTFLoadState::Idle, HRTFLoadState::Loading, HRTFLoadState::Ready, HRTFLoadState::Failed })
            for (const auto source : { HRTFProfileSource::None, HRTFProfileSource::Simple, HRTFProfileSource::SharedFolder, HRTFProfileSource::Embedded })
                for (const auto problem : { HRTFProfileProblem::None, HRTFProfileProblem::InvalidIndex, HRTFProfileProblem::NotFound,
                                            HRTFProfileProblem::SharedFileUnreadableUsedBuiltIn, HRTFProfileProblem::SharedFileUnreadableNoBuiltIn,
                                            HRTFProfileProblem::LoadFailed })
                {
                    s.requestedProfile = s.activeProfile = profile;
                    s.state = state;
                    s.source = source;
                    s.problem = problem;
                    CHECK (describeHRTFProfileStatus (s).isNotEmpty());
                }
}

TEST_CASE ("HRTF resolve: the shared folder is the system folder (D-10)",
           "[hrtf-resolve][folder]")
{
#if JUCE_MAC
    CHECK (getSharedHRTFFolder().getFullPathName() == "/Library/Application Support/Spatial Media Lab/HRTF");
#elif JUCE_WINDOWS
    CHECK (getSharedHRTFFolder().getFullPathName().endsWith ("Spatial Media Lab\\HRTF"));
#else
    CHECK (getSharedHRTFFolder() == juce::File());
#endif
}

#if ! SPATIALCORE_EMBEDS_ALL_HRTF
TEST_CASE ("HRTF resolve: in a KEMAR-only build profiles 1-4 come from the shared folder or not at all",
           "[hrtf-resolve][kemar-only]")
{
    const TempFolder folder ("kemar-only");
    HRTFDatabase db;

    const HRTFResolveResult none = resolveHRTFProfile (1, db, 48000.0f, folder.dir);
    CHECK_FALSE (none.loaded);
    CHECK (none.problem == HRTFProfileProblem::NotFound);
    CHECK_FALSE (db.isLoaded());

    REQUIRE (getSofaFile (testProfileFile (1)).copyFileTo (folder.dir.getChildFile (kHRTFProfiles[1].fileName)));
    const HRTFResolveResult shared = resolveHRTFProfile (1, db, 48000.0f, folder.dir);
    CHECK (shared.loaded);
    CHECK (shared.source == HRTFProfileSource::SharedFolder);
    CHECK (db.getIRLength() == kExpectedIRLength[1]);

    // A broken shared file with no built-in copy is reported, not hidden.
    REQUIRE (folder.dir.getChildFile (kHRTFProfiles[1].fileName).replaceWithText ("not a sofa file"));
    const HRTFResolveResult broken = resolveHRTFProfile (1, db, 48000.0f, folder.dir);
    CHECK_FALSE (broken.loaded);
    CHECK (broken.problem == HRTFProfileProblem::SharedFileUnreadableNoBuiltIn);
    CHECK_FALSE (db.isLoaded());
}
#endif

// ============================================================================
// Decoded-size bound (T-03-09, review WR-01). The byte cap on a shared file does not bound the
// filter length: libmysofa scales it by targetSampleRate / fileSampleRate. A database whose
// decoded IR is longer than HRTFDatabase::kMaxIRLength would make BinauralRenderer::setProfile
// size 24 convolvers into the gigabytes, so loadFromBytes refuses it. A real file asked for an
// absurd rate is the cheapest way to produce one without hand-building an HDF5 file.
// ============================================================================
TEST_CASE ("HRTF database: a decoded IR longer than the bound is refused and leaves the database unloaded",
           "[hrtf-resolve][ir-bound]")
{
    const juce::File file = getSofaFile (testProfileFile (3));   // hutubs_pp2: 440 positions, the smallest decoded size
    REQUIRE (file.existsAsFile());

    HRTFDatabase db;

    SECTION ("an in-range rate loads")
    {
        REQUIRE (db.loadFromFile (file, 192000.0f));
        CHECK (db.getIRLength() >= 1);
        CHECK (db.getIRLength() <= HRTFDatabase::kMaxIRLength);
    }

    SECTION ("a rate that scales the IR past the bound is refused")
    {
        // 44.1 kHz source at 3 MHz: about 64x, so the IR would be well over 16384 samples.
        CHECK_FALSE (db.loadFromFile (file, 3000000.0f));
        CHECK_FALSE (db.isLoaded());
        CHECK (db.getIRLength() == 0);
        CHECK (db.getNumPositions() == 0);
    }
}

// ============================================================================
// The size cap is enforced while reading, from one open handle, and only a regular file is read
// (T-03-09, review WR-03). A stat-then-read check let a FIFO, a device node or a symlink to one
// bypass it: their size reads as 0, and the read then blocks or never ends.
// ============================================================================
TEST_CASE ("HRTF database: loadFromFile enforces the byte cap while reading",
           "[hrtf-resolve][read-cap]")
{
    const juce::File file = getSofaFile (testProfileFile (kProbeProfile));
    REQUIRE (file.existsAsFile());

    HRTFDatabase db;
    CHECK_FALSE (db.loadFromFile (file, 48000.0f, 1000));                 // larger than the cap
    CHECK_FALSE (db.isLoaded());
    CHECK (db.loadFromFile (file, 48000.0f, file.getSize()));             // exactly at the cap
    CHECK (db.isLoaded());
    CHECK (db.loadFromFile (file, 48000.0f));                             // no cap given

    // A failed read leaves the database fully empty, not half-loaded (IN-04).
    REQUIRE (db.loadFromFile (file, 48000.0f));
    REQUIRE (db.getIRLength() > 0);
    CHECK_FALSE (db.loadFromFile (file.getSiblingFile ("does-not-exist.sofa"), 48000.0f, 1000));
    CHECK_FALSE (db.isLoaded());
    CHECK (db.getIRLength() == 0);
    CHECK (db.getNumPositions() == 0);
}

#if JUCE_MAC || JUCE_LINUX
TEST_CASE ("HRTF resolve: a FIFO or a symlink to a device behind the profile name is refused, not read",
           "[hrtf-resolve][read-cap][special-file]")
{
    const TempFolder folder ("special");
    const juce::File probe = folder.dir.getChildFile (kHRTFProfiles[kProbeProfile].fileName);

    SECTION ("a FIFO")
    {
        // Opening a FIFO for reading blocks until a writer appears, so an unguarded read hangs here.
        REQUIRE (::mkfifo (probe.getFullPathName().toRawUTF8(), 0600) == 0);
    }
    SECTION ("a symlink to /dev/zero")
    {
        // Reads forever, and its size reads as 0, so a size check passes it.
        REQUIRE (juce::File ("/dev/zero").createSymbolicLink (probe, true));
    }

    HRTFDatabase db;
    const HRTFResolveResult r = resolveHRTFProfile (kProbeProfile, db, 48000.0f, folder.dir);

    CHECK (r.loaded);
    CHECK (r.source == HRTFProfileSource::Embedded);
    CHECK (r.problem == HRTFProfileProblem::SharedFileUnreadableUsedBuiltIn);
}
#endif
