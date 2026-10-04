#include <SpatialCore/Binaural/HRTFDatabase.h>
#include <SpatialCore/Binaural/SharedFFTCache.h>

extern "C" {
#include "mysofa.h"
}

#include <limits>

#if ! JUCE_WINDOWS
 #include <sys/stat.h>
#endif

namespace spatialcore
{

//==============================================================================
// HRTFDatabase implementation -- SOFA loading + HRIR lookup
//==============================================================================
HRTFDatabase::HRTFDatabase() {}

HRTFDatabase::~HRTFDatabase()
{
    unload();
}

bool HRTFDatabase::loadFromMemory (const void* data, int dataSize, float targetSampleRate)
{
    return loadFromBytes (data, dataSize, targetSampleRate);
}

bool HRTFDatabase::loadFromBinaryData (int profileIndex, float targetSampleRate)
{
    unload();

    int size = 0;
    const char* data = getEmbeddedProfileData (profileIndex, size);

    if (data == nullptr || size <= 0)
        return false;

    return loadFromBytes (data, size, targetSampleRate);
}

namespace
{
    /** True for a regular file (following symlinks). A FIFO, device node or socket is not: reading
        one can block forever or never reach EOF, and its reported size is meaningless. */
    bool isRegularFile (const juce::File& file)
    {
#if JUCE_WINDOWS
        return file.existsAsFile();
#else
        struct stat st {};
        return ::stat (file.getFullPathName().toRawUTF8(), &st) == 0 && S_ISREG (st.st_mode);
#endif
    }
}

bool HRTFDatabase::loadFromFile (const juce::File& sofaFile, float targetSampleRate, juce::int64 maxBytes)
{
    // Read the LFS-tracked raw .sofa bytes from disk. A checkout that returned
    // LFS pointer text instead of real bytes (T-08-05b) fails mysofa_open_data's
    // parse below (or the checksum assertion in the per-profile tests), never
    // silently succeeds with garbage HRIR data.
    //
    // The byte cap is enforced while reading, from one open handle, so a file that lies about
    // its size, grows after a size check, or is not a regular file cannot bypass it (T-03-09).
    //
    // Unload first, as loadFromBytes and loadFromBinaryData do: a read failure then leaves the
    // database fully empty, not holding the previous handle behind loaded = false (IN-04).
    unload();

    constexpr juce::int64 kMaxInt = std::numeric_limits<int>::max();
    const juce::int64 cap = (maxBytes > 0 && maxBytes < kMaxInt) ? maxBytes : kMaxInt;

    juce::MemoryBlock fileBytes;
    bool readOk = false;

    if (isRegularFile (sofaFile))
    {
        juce::FileInputStream in (sofaFile);

        if (in.openedOk() && in.getTotalLength() <= cap)
        {
            in.readIntoMemoryBlock (fileBytes, static_cast<ssize_t> (cap) + 1);
            readOk = static_cast<juce::int64> (fileBytes.getSize()) <= cap;
        }
    }

    if (! readOk)
    {
        DBG ("HRTFDatabase: Failed to read SOFA file: " + sofaFile.getFullPathName());
        loaded = false;
        return false;
    }

    return loadFromBytes (fileBytes.getData(), static_cast<int> (fileBytes.getSize()), targetSampleRate);
}

bool HRTFDatabase::loadFromBytes (const void* data, int dataSize, float targetSampleRate)
{
    unload();

    int filterLength = 0;
    int err = 0;

    easyHandle = mysofa_open_data (static_cast<const char*> (data),
                                   static_cast<long> (dataSize),
                                   targetSampleRate,
                                   &filterLength,
                                   &err);

    if (easyHandle == nullptr || err != MYSOFA_OK)
    {
        DBG ("HRTFDatabase: Failed to load SOFA data, error code: " + juce::String (err));
        easyHandle = nullptr;
        loaded = false;
        return false;
    }

    // T-03-09: bound the decoded size before anything sizes buffers from it. The byte cap on
    // a shared file does not bound the filter length (libmysofa has no ceiling on N and
    // scales it by targetSampleRate / fileSampleRate), so an out-of-range database is
    // refused here, which the resolver reports as an unreadable file.
    if (filterLength < 1 || filterLength > kMaxIRLength
        || easyHandle->hrtf == nullptr
        || easyHandle->hrtf->M < 1 || easyHandle->hrtf->M > static_cast<unsigned> (kMaxPositions))
    {
        DBG ("HRTFDatabase: SOFA data out of bounds (IR length " + juce::String (filterLength) + ")");
        unload();
        return false;
    }

    // Note: mysofa_open_data() already normalizes via mysofa_loudness(), which
    // scales all HRIRs so the frontal HRIR has consistent energy (factor = sqrt(2/E)).
    // This provides cross-profile normalization at the source level.

    irLength = filterLength;
    numPositions = static_cast<int> (easyHandle->hrtf->M);
    loaded = true;

    DBG ("HRTFDatabase: Loaded " + juce::String (numPositions) + " positions, "
         + "IR length = " + juce::String (irLength) + " samples");

    return true;
}

void HRTFDatabase::getInterpolatedHRIR (float azimuthRad, float elevationRad,
                                         float* irL, float* irR,
                                         float& delayL, float& delayR) const
{
    if (! loaded || easyHandle == nullptr)
        return;

    // Convert from our convention (radians) to Cartesian for libmysofa
    // libmysofa uses Cartesian coordinates internally
    // Our convention: az=0 front, positive=left; el=0 ear level, positive=up
    // Convert to Cartesian: x=front, y=left, z=up
    float x = std::cos (elevationRad) * std::cos (azimuthRad);
    float y = std::cos (elevationRad) * std::sin (azimuthRad);
    float z = std::sin (elevationRad);

    mysofa_getfilter_float (easyHandle, x, y, z,
                            irL, irR,
                            &delayL, &delayR);
}

void HRTFDatabase::getAlignedHRIR (float azimuthRad, float elevationRad,
                                    float* irL, float* irR,
                                    float& delayL, float& delayR) const
{
    // First get the standard interpolated HRIR with embedded ITD
    getInterpolatedHRIR (azimuthRad, elevationRad, irL, irR, delayL, delayR);

    if (! loaded || irLength <= 0)
        return;

    // Remove ITD by shifting each HRIR backward by its delay (integer part).
    // The fractional part remains in delayL/delayR for the caller to apply
    // as a separate fractional-sample delay.
    //
    // This produces time-aligned HRIRs with coherent phase structure,
    // enabling smooth crossfading between neighboring positions without
    // comb-filtering from ITD misalignment.

    int shiftL = static_cast<int> (delayL);
    int shiftR = static_cast<int> (delayR);

    // v1.0.11 (issue Spatial-Media-Lab/OpenSpatialDelay#89): If SOFA reports zero delay for both channels,
    // the ITD is baked into the HRIR waveform (confirmed for MIT KEMAR,
    // likely CIPIC/HUTUBS/Bernschuetz). Detect onset from the waveform
    // itself so the dual-slot crossfade blends time-aligned HRIRs.
    // SADIE II KU100 reports non-zero delays and bypasses this fallback.
    // NOTE: Check raw float delays, not integer-truncated -- SADIE reports
    // fractional delays (e.g., 0.3/0.7) that truncate to int 0 but are valid.
    if (delayL < 0.001f && delayR < 0.001f)
    {
        int onsetL = detectOnset (irL, irLength, 0.1f);
        int onsetR = detectOnset (irR, irLength, 0.1f);
        int minOnset = std::min (onsetL, onsetR);
        shiftL = onsetL - minOnset;
        shiftR = onsetR - minOnset;
        // Report detected ITD for the ITD delay line in renderSourceBuffers()
        delayL = static_cast<float> (onsetL);
        delayR = static_cast<float> (onsetR);
    }

    // Shift left channel: move samples backward by shiftL
    if (shiftL > 0 && shiftL < irLength)
    {
        for (int i = 0; i < irLength - shiftL; ++i)
            irL[i] = irL[i + shiftL];
        for (int i = irLength - shiftL; i < irLength; ++i)
            irL[i] = 0.0f;
    }

    // Shift right channel: move samples backward by shiftR
    if (shiftR > 0 && shiftR < irLength)
    {
        for (int i = 0; i < irLength - shiftR; ++i)
            irR[i] = irR[i + shiftR];
        for (int i = irLength - shiftR; i < irLength; ++i)
            irR[i] = 0.0f;
    }
}

void HRTFDatabase::unload()
{
    if (easyHandle != nullptr)
    {
        mysofa_close (easyHandle);
        easyHandle = nullptr;
    }
    loaded = false;
    irLength = 0;
    numPositions = 0;
}

//==============================================================================
// v1.0.4: Minimum-phase HRIR conversion via cepstral decomposition (issue Spatial-Media-Lab/OpenSpatialDelay#47).
// Converts a raw HRIR to its minimum-phase equivalent in-place.
// Preserves magnitude spectrum but removes excess phase, enabling smooth
// time-domain EMA interpolation between adjacent HRIRs without comb filtering.
//==============================================================================
void HRTFDatabase::convertToMinPhase (float* ir, int irLength, int fftOrder, float* workBuf)
{
    if (irLength <= 0 || fftOrder <= 0) return;

    const int N = 1 << fftOrder;  // FFT size (must be >= 2 * irLength)
    auto fftPtr = getSharedFFTCache().getOrCreate (fftOrder);

    // workBuf layout: N Complex<float> = N * 2 floats
    auto* cBuf = reinterpret_cast<std::complex<float>*> (workBuf);

    // Step 1: Copy IR into complex buffer, zero-pad
    for (int i = 0; i < N; ++i)
        cBuf[i] = (i < irLength) ? std::complex<float> (ir[i], 0.0f) : std::complex<float> (0.0f, 0.0f);

    // Step 2: Forward FFT
    fftPtr->perform (cBuf, cBuf, false);

    // Step 3: Compute log-magnitude (real cepstrum input)
    for (int k = 0; k < N; ++k)
    {
        float mag = std::abs (cBuf[k]);
        float logMag = std::log (std::max (mag, 1e-20f));  // epsilon for -ffast-math safety
        cBuf[k] = std::complex<float> (logMag, 0.0f);
    }

    // Step 4: IFFT to get real cepstrum
    fftPtr->perform (cBuf, cBuf, true);

    // Step 5: Apply minimum-phase window to cepstrum
    // c_mp[0] unchanged, c_mp[1..N/2-1] doubled, c_mp[N/2] unchanged, c_mp[N/2+1..N-1] zeroed
    int halfN = N / 2;
    for (int n = 1; n < halfN; ++n)
        cBuf[n] *= 2.0f;
    for (int n = halfN + 1; n < N; ++n)
        cBuf[n] = std::complex<float> (0.0f, 0.0f);

    // Step 6: Forward FFT
    fftPtr->perform (cBuf, cBuf, false);

    // Step 7: Exponentiate to get minimum-phase spectrum
    for (int k = 0; k < N; ++k)
    {
        float re = cBuf[k].real();
        float im = cBuf[k].imag();
        // Clamp to prevent exp() overflow under -ffast-math
        re = std::max (-80.0f, std::min (80.0f, re));
        float mag = std::exp (re);
        cBuf[k] = std::complex<float> (mag * std::cos (im), mag * std::sin (im));
    }

    // Step 8: IFFT to get minimum-phase IR
    fftPtr->perform (cBuf, cBuf, true);

    // Step 9: Copy first irLength samples back (real part only)
    for (int i = 0; i < irLength; ++i)
        ir[i] = cBuf[i].real();
}

int HRTFDatabase::detectOnset (const float* ir, int length, float thresholdFraction)
{
    if (ir == nullptr || length <= 0)
        return 0;

    // Find peak absolute value
    float peak = 0.0f;
    for (int i = 0; i < length; ++i)
    {
        float absVal = std::abs (ir[i]);
        if (absVal > peak)
            peak = absVal;
    }

    if (peak < 1e-20f)
        return 0;  // Silent IR

    // Scan forward for first sample exceeding threshold * peak
    float thresh = thresholdFraction * peak;
    for (int i = 0; i < length; ++i)
    {
        if (std::abs (ir[i]) >= thresh)
        {
            // Clamp: if onset is past halfway, IR has no clear leading edge
            if (i > length / 2)
                return 0;
            return i;
        }
    }

    return 0;
}

void HRTFDatabase::correctLowFrequency (float* ir, int irLength, int fftOrder,
                                         float sampleRate, float* workBuf,
                                         float lfCutoffHz, float hfCutoffHz)
{
    if (ir == nullptr || irLength <= 0 || fftOrder <= 0)
        return;

    const int N = 1 << fftOrder;
    juce::dsp::FFT fft (fftOrder);

    auto* cBuf = reinterpret_cast<std::complex<float>*> (workBuf);

    // Step 1: Copy IR into complex buffer, zero-pad
    for (int i = 0; i < N; ++i)
        cBuf[i] = (i < irLength) ? std::complex<float> (ir[i], 0.0f)
                                 : std::complex<float> (0.0f, 0.0f);

    // Step 2: Forward FFT
    fft.perform (cBuf, cBuf, false);

    // Step 3: Identify frequency bin ranges
    float binHz = sampleRate / static_cast<float> (N);
    int lfBin = static_cast<int> (lfCutoffHz / binHz);
    int hfBin = static_cast<int> (hfCutoffHz / binHz);

    if (lfBin < 1) lfBin = 1;
    if (hfBin <= lfBin) hfBin = lfBin + 1;
    if (hfBin > N / 2) hfBin = N / 2;

    // Step 4: Compute mean magnitude in the reference range (100-300 Hz)
    float sumMag = 0.0f;
    int magCount = 0;
    for (int k = lfBin; k < hfBin; ++k)
    {
        sumMag += std::abs (cBuf[k]);
        ++magCount;
    }
    float meanMag = (magCount > 0) ? sumMag / static_cast<float> (magCount) : 0.0f;

    // Step 5: Magnitude-only correction below lfCutoff.
    // Scale each bin's magnitude up to meanMag while preserving its original phase.
    // This avoids the phase discontinuities between adjacent positions that caused
    // glitches when full phase extrapolation was used (same issue as min-phase, Spatial-Media-Lab/OpenSpatialDelay#47).
    for (int k = 0; k < lfBin; ++k)
    {
        float currentMag = std::abs (cBuf[k]);
        if (currentMag < 1e-20f)
        {
            // Bin has essentially zero energy -- set to meanMag with zero phase
            cBuf[k] = std::complex<float> (meanMag, 0.0f);
        }
        else
        {
            // Scale magnitude up to meanMag, preserve original phase
            float scale = meanMag / currentMag;
            cBuf[k] *= scale;
        }

        // Mirror to negative frequencies (conjugate symmetry for real output)
        if (k > 0 && (N - k) < N)
            cBuf[N - k] = std::conj (cBuf[k]);
    }

    // Step 6: Inverse FFT
    fft.perform (cBuf, cBuf, true);

    // Step 7: Copy back to IR (real part only, original length)
    for (int i = 0; i < irLength; ++i)
        ir[i] = cBuf[i].real();
}

} // namespace spatialcore
