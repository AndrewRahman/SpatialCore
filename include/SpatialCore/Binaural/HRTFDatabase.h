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
        valid SOFA file (e.g. an unresolved Git LFS pointer stub). */
    bool loadFromFile (const juce::File& sofaFile, float targetSampleRate);

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
