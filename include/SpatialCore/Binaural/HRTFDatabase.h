#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>

// Forward declaration -- libmysofa is a private dependency
struct MYSOFA_EASY;

namespace spatialcore
{

//==============================================================================
// HRTF Database -- loads SOFA files and provides HRIR lookup
// Wraps libmysofa for SOFA parsing and nearest-neighbor interpolation
//
// Thread contract (RTSF-03, D-09):
//   - ONE database is used by ONE thread at a time. The lookups are const, but
//     mysofa_getfilter_float writes a scratch buffer inside the libmysofa handle
//     (easy->fir), so two threads looking up on one database race even though
//     neither "writes" it as far as the C++ interface shows.
//   - loadFromMemory / loadFromFile / loadFromBinaryData / unload allocate and
//     parse. They run on the message thread or the engine's loader thread, never
//     on a database the audio thread is using. Exclusive ownership is the
//     contract; no lock is taken anywhere (the audio path must not wait).
//   - RenderEngine marks the database of every renderer the audio thread renders
//     from (setInUseByAudioThread). A mutation of a marked database still runs,
//     exactly as before, but is counted by getMutationWhileInUseCount() and
//     asserts in Debug, so a misuse is visible without breaking a consumer.
//   - Every mutator also bumps a mutation serial (getMutationSerial()), marked or not. The
//     engine compares it across a legacy swap to catch a load into the crossfade source in the
//     window before the next block (countMutationBeforeFade()).
//   - Not copyable: it owns a raw libmysofa handle and the atomics below.
//==============================================================================
class HRTFDatabase
{
public:
    HRTFDatabase();
    ~HRTFDatabase();

    /** Load a SOFA file from memory (BinaryData). Resamples to targetSampleRate. */
    bool loadFromMemory (const void* data, int dataSize, float targetSampleRate);

    /** Load a SOFA file from disk (Git-LFS-tracked raw .sofa file). Resamples to
        targetSampleRate. Reads the file's bytes and hands them to the same
        libmysofa parsing body used by loadFromMemory — the SOFA-parsing logic
        itself stays byte-identical; only the byte source differs (D-09,
        minimum-diff). Returns false if the file cannot be read or is not a
        valid SOFA file (e.g. an unresolved Git LFS pointer stub).

        Only a regular file is read (a FIFO, a device node or a symlink to one is refused, since
        reading it can block forever or never reach EOF). With maxBytes > 0 the cap is enforced
        while reading, from one open handle: a file longer than maxBytes, or one that grows past
        it between a size check and the read, is refused without being buffered (T-03-09).
        maxBytes <= 0 means no cap beyond what fits in an int. */
    bool loadFromFile (const juce::File& sofaFile, float targetSampleRate, juce::int64 maxBytes = 0);

    /** The compiled-in SOFA bytes for profile 1..5 (see HRTFProfile.h), or nullptr with
        sizeInBytes = 0 when that profile is not embedded (a build with
        SPATIALCORE_EMBED_ALL_HRTF=OFF embeds profile 5 only) or the index is out of range.
        Never allocates; the pointer is valid for the life of the program. */
    static const char* getEmbeddedProfileData (int profileIndex, int& sizeInBytes);

    /** Load the compiled-in copy of profile 1..5 through the same parse body as
        loadFromMemory. Returns false and leaves the database unloaded when the profile is
        not embedded or the index is invalid. Allocates and parses (up to tens of MB):
        never call from the audio thread. */
    bool loadFromBinaryData (int profileIndex, float targetSampleRate);

    /** Get interpolated HRIR pair for a direction (our convention: radians).
        Writes irLength samples to irL and irR buffers (must be pre-allocated).
        const, but it writes the libmysofa handle's scratch buffer: one database is used by
        one thread at a time (see the class comment). */
    void getInterpolatedHRIR (float azimuthRad, float elevationRad,
                              float* irL, float* irR,
                              float& delayL, float& delayR) const;

    /** Get ITD-free interpolated HRIR pair for a direction.
        The returned HRIRs have ITD removed (time-aligned onsets). The ITD values
        are returned separately in delayL/delayR (in samples, fractional).
        This produces phase-coherent HRIRs that can be smoothly crossfaded
        without comb-filtering artifacts from ITD misalignment.
        const, but it writes the libmysofa handle's scratch buffer: one database is used by
        one thread at a time (see the class comment). */
    void getAlignedHRIR (float azimuthRad, float elevationRad,
                         float* irL, float* irR,
                         float& delayL, float& delayR) const;

    /** Sanity bounds on a decoded SOFA database, applied to every load path. The byte cap on a
        shared file bounds the file, NOT what libmysofa allocates from it: mysofa_resample
        mallocs ceil (N * target / declaredRate) * R * M floats with no check, so a small file
        declaring a low (or zero) sample rate would drive an allocation of any size. loadFromBytes
        therefore parses the file first (no resampling) and refuses it through
        declaredShapeWithinBounds() before mysofa_open_data runs, then re-checks the decoded
        result. BinauralRenderer::setProfile sizes 24 convolvers from the IR length, which is the
        other reason for the cap (T-03-09). A database outside these bounds is rejected like an
        unreadable file. KEMAR (profile 5) has the longest shipped IR: 512 samples at its native
        44.1 kHz, 558 at 48 kHz and about 2.2k at 192 kHz. Bernschuetz has the most positions,
        16020. kMaxDecodedSamples bounds the resampled float count (R * M * newN), 512 MiB of
        floats; the largest shipped decoded size at 192 kHz is about 18 million (SADIE II, 8802
        positions, 1024 samples), and Bernschuetz is about 16 million. */
    static constexpr int kMaxIRLength   = 16384;
    static constexpr int kMaxPositions  = 65536;
    static constexpr double kMaxDecodedSamples = 134217728.0;   // 2^27 floats

    /** True when a file declaring this sample rate and shape, resampled to targetSampleRate,
        stays within the bounds above: declared and target rate in [8000, 768000] Hz, R == 2,
        1 <= M <= kMaxPositions, N >= 1, resampled length ceil (N * target / declared) <=
        kMaxIRLength, and R * M * that length <= kMaxDecodedSamples. Pure arithmetic, no
        allocation, in double so it cannot wrap. Public so the bound can be tested without
        building an HDF5 file. */
    static bool declaredShapeWithinBounds (double declaredRate, unsigned R, unsigned M, unsigned N,
                                           float targetSampleRate);

    int  getIRLength() const { return irLength; }
    int  getNumPositions() const { return numPositions; }
    bool isLoaded() const { return loaded; }

    /** Unload current profile and free resources. Allocation-free but not audio-thread safe:
        message or loader thread, on a database the audio thread is not using. */
    void unload();

    /** RenderEngine's mark (RTSF-03, D-09): true while the audio thread renders from this
        database (the active renderer's, or the source of a running renderer crossfade, as of
        the last rendered block). One relaxed store; any thread. */
    void setInUseByAudioThread (bool inUse) noexcept;

    /** The current mark. Relaxed load; any thread. */
    bool isInUseByAudioThread() const noexcept;

    /** How many mutating calls (a load, unload, or BinauralRenderer::setProfile) ran while
        the in-use mark was set. One per call, even though loadFromBinaryData closes the handle
        twice internally; the destructor never counts. Lock-free, any thread. */
    int getMutationWhileInUseCount() const noexcept;

    /** Called once at the start of every public mutator and by BinauralRenderer::setProfile.
        It always bumps the mutation serial. When the in-use mark is set it also increments the
        counter, asserts in Debug and returns true; the mutation then proceeds exactly as before
        (D-09: detect, never block). */
    bool noteMutationIfInUse() noexcept;

    /** How many mutating calls have started on this database, marked or not (wraps; compare for
        inequality only). Relaxed load; any thread. RenderEngine snapshots it at a legacy swap. */
    unsigned getMutationSerial() const noexcept;

    /** RenderEngine only (RTSF-03): adds one to getMutationWhileInUseCount() for a mutation that
        the in-use mark could not see because it ran between swapActiveRenderer() and the first
        block that crossfades from this database. One relaxed add; no assert, no logging, so the
        audio thread may call it. */
    void countMutationBeforeFade() noexcept;

    /** Convert a raw HRIR to minimum-phase in-place using cepstral decomposition.
        Preserves magnitude spectrum but removes excess phase, so time-domain
        interpolation between adjacent HRIRs produces smooth spectral transitions
        without comb filtering (issue Spatial-Media-Lab/OpenSpatialDelay#47). workBuf must be >= fftSize * 2 floats. */
    static void convertToMinPhase (float* ir, int irLength, int fftOrder, float* workBuf);

    /** Detect onset sample index of an IR using threshold of peak amplitude.
        Returns the index of the first sample exceeding thresholdFraction * peakAbs.
        Used to compute ITD when SOFA delay values are zero (ITD baked into waveform).
        Returns 0 if no clear onset found or if onset > irLength/2. */
    static int detectOnset (const float* ir, int length, float thresholdFraction = 0.1f);

    /** Apply low-frequency correction to an HRIR in-place (Xie 2009 method).
        Below lfCutoffHz: magnitude is set to the mean of the lfCutoffHz-to-hfCutoffHz
        range, and phase is linearly extrapolated from that range. This restores
        physically plausible bass response for datasets with weak LF content (e.g.,
        MIT KEMAR). workBuf must be >= fftSize * 2 floats. */
    static void correctLowFrequency (float* ir, int irLength, int fftOrder,
                                      float sampleRate, float* workBuf,
                                      float lfCutoffHz = 100.0f,
                                      float hfCutoffHz = 300.0f);

private:
    /** Shared SOFA-parsing body for loadFromMemory/loadFromFile — byte-identical
        libmysofa call sequence regardless of byte source (D-09, minimum-diff). */
    bool loadFromBytes (const void* data, int dataSize, float targetSampleRate);

    /** Closes the libmysofa handle and clears the loaded state, without counting. Used by the
        destructor and by every internal path that must not count (the public mutator already
        did, once). */
    void closeHandle() noexcept;

    // RTSF-03 (D-09): relaxed bookkeeping. A non-lock-free atomic would put a lock on the path.
    std::atomic<bool> inUseByAudioThread_ { false };
    std::atomic<int> mutationWhileInUseCount_ { 0 };
    std::atomic<unsigned> mutationSerial_ { 0 };
    static_assert (std::atomic<bool>::is_always_lock_free && std::atomic<int>::is_always_lock_free
                       && std::atomic<unsigned>::is_always_lock_free,
                   "HRTFDatabase bookkeeping requires lock-free std::atomic<bool>, <int> and <unsigned>");

    MYSOFA_EASY* easyHandle = nullptr;
    int irLength     = 0;
    int numPositions = 0;
    bool loaded      = false;
};

} // namespace spatialcore
