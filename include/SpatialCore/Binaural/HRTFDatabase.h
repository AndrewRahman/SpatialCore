#pragma once

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

// Forward declaration -- libmysofa is a private dependency
struct MYSOFA_EASY;

namespace spatialcore
{

//==============================================================================
// HRTF Database -- loads SOFA files and provides HRIR lookup
// Wraps libmysofa for SOFA parsing and nearest-neighbor interpolation
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
        Writes irLength samples to irL and irR buffers (must be pre-allocated). */
    void getInterpolatedHRIR (float azimuthRad, float elevationRad,
                              float* irL, float* irR,
                              float& delayL, float& delayR) const;

    /** Get ITD-free interpolated HRIR pair for a direction.
        The returned HRIRs have ITD removed (time-aligned onsets). The ITD values
        are returned separately in delayL/delayR (in samples, fractional).
        This produces phase-coherent HRIRs that can be smoothly crossfaded
        without comb-filtering artifacts from ITD misalignment. */
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
        unreadable file. The largest shipped profile is 558 samples at its native rate and 16020
        positions; at 192 kHz the longest shipped IR is about 2.4k samples.
        kMaxDecodedSamples bounds the resampled float count (R * M * newN), 512 MiB of floats;
        the largest shipped profile at 192 kHz is under 80 million. */
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

    /** Unload current profile and free resources. */
    void unload();

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

    MYSOFA_EASY* easyHandle = nullptr;
    int irLength     = 0;
    int numPositions = 0;
    bool loaded      = false;
};

} // namespace spatialcore
