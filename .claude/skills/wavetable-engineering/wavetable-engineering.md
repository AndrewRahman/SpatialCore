---
name: wavetable-engineering
description: Wavetable synthesis implementation including mipmap generation for anti-aliased playback, cubic Hermite interpolation, .wav single-cycle loading, wavetable morphing, and user wavetable import
---

# Wavetable Engineering

Complete reference for implementing anti-aliased wavetable synthesis in JUCE C++ audio plugins.

## Core Concept

A wavetable is a collection of single-cycle waveforms stored in a lookup table. To play a note:
1. Step through the table at a rate determined by the frequency
2. Interpolate between samples for sub-sample accuracy
3. Use mipmaps to prevent aliasing at high frequencies

## Single-Cycle Waveform Storage

### Table Size
Standard: **2048 samples** per single-cycle waveform.

| Size | Quality | CPU | Use Case |
|------|---------|-----|----------|
| 256 | Low | Minimal | Retro/lo-fi |
| 1024 | Good | Low | General purpose |
| 2048 | Excellent | Medium | Production quality |
| 4096 | Overkill | Higher | Only for analysis |

### Data Layout

```cpp
struct Wavetable {
    static constexpr int TABLE_SIZE = 2048;
    static constexpr int NUM_MIPMAPS = 11;  // log2(2048) = 11 levels

    // Mipmap levels: [0] = full table (2048), [1] = half (1024), ... [10] = 2 samples
    float tables[NUM_MIPMAPS][TABLE_SIZE + 1];  // +1 for wraparound guard sample
    int tableSizes[NUM_MIPMAPS];                // Actual size at each level
};
```

The **+1 guard sample** duplicates the first sample at the end, enabling simple interpolation at the table boundary without modulo:
```cpp
tables[level][TABLE_SIZE] = tables[level][0];  // Guard sample
```

## Mipmap Generation (Anti-Aliasing)

### Why Mipmaps?

Playing a wavetable at high frequencies (e.g., C8 = 4186 Hz at 44.1kHz sample rate) means stepping through the table very quickly. Harmonics above Nyquist fold back as aliasing.

**Solution:** Pre-filter the wavetable at multiple resolutions. At high frequencies, use a version with fewer harmonics.

### Generation Algorithm

```cpp
void generateMipmaps(Wavetable& wt) {
    // Level 0: full-resolution table (already loaded)
    wt.tableSizes[0] = Wavetable::TABLE_SIZE;

    // Forward FFT of level 0
    juce::dsp::FFT fft(11);  // log2(2048) = 11
    std::complex<float> spectrum[Wavetable::TABLE_SIZE];
    fft.performRealOnlyForwardTransform(wt.tables[0], spectrum);

    // Generate each mipmap level by truncating harmonics
    for (int level = 1; level < Wavetable::NUM_MIPMAPS; ++level) {
        int size = Wavetable::TABLE_SIZE >> level;  // 1024, 512, 256, ...
        wt.tableSizes[level] = size;

        // Copy spectrum, zero out harmonics above Nyquist for this level
        std::complex<float> filtered[Wavetable::TABLE_SIZE];
        std::copy(spectrum, spectrum + Wavetable::TABLE_SIZE, filtered);

        int maxHarmonic = size / 2;
        for (int h = maxHarmonic; h < Wavetable::TABLE_SIZE / 2; ++h) {
            filtered[h] = {0, 0};
            filtered[Wavetable::TABLE_SIZE - h] = {0, 0};
        }

        // Inverse FFT back to time domain
        fft.performRealOnlyInverseTransform(filtered, wt.tables[level]);

        // Add guard sample
        wt.tables[level][size] = wt.tables[level][0];
    }
}
```

### Selecting the Right Mipmap Level

```cpp
int selectMipmapLevel(float phaseIncrement) {
    // phaseIncrement = frequency / sampleRate * TABLE_SIZE
    // Level 0 covers up to TABLE_SIZE/2 harmonics
    // Each level halves the harmonic count

    float harmonicsNeeded = 0.5f / phaseIncrement * Wavetable::TABLE_SIZE;
    int level = 0;
    int availableHarmonics = Wavetable::TABLE_SIZE / 2;

    while (level < Wavetable::NUM_MIPMAPS - 1 && availableHarmonics > harmonicsNeeded * 2) {
        ++level;
        availableHarmonics /= 2;
    }
    return level;
}
```

## Interpolation Methods

### Linear Interpolation (Minimum Quality)
```cpp
float readLinear(const float* table, float phase, int tableSize) {
    int index0 = (int)phase;
    float frac = phase - index0;
    return table[index0] + frac * (table[index0 + 1] - table[index0]);
}
```

### Cubic Hermite / Catmull-Rom (Recommended)
```cpp
float readCubicHermite(const float* table, float phase, int tableSize) {
    int i1 = (int)phase;
    float frac = phase - i1;
    int i0 = (i1 - 1 + tableSize) % tableSize;
    int i2 = (i1 + 1) % tableSize;
    int i3 = (i1 + 2) % tableSize;

    float y0 = table[i0], y1 = table[i1], y2 = table[i2], y3 = table[i3];

    // Catmull-Rom coefficients
    float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c = -0.5f * y0 + 0.5f * y2;
    float d = y1;

    return ((a * frac + b) * frac + c) * frac + d;
}
```

### Quality Comparison

| Method | CPU | Quality | Alias Rejection |
|--------|-----|---------|-----------------|
| Nearest | ~0 | Terrible | None |
| Linear | 1x | Acceptable | -60dB |
| Cubic Hermite | 2x | Good | -90dB |
| Lagrange 5th | 3x | Excellent | -110dB |
| Sinc (windowed) | 10x | Perfect | -140dB |

**Recommendation:** Cubic Hermite + mipmaps. Best quality/CPU tradeoff.

## Loading .wav Single-Cycle Files

### File Format
Single-cycle wavetables are standard .wav files containing exactly one cycle (or multiple cycles for multi-frame wavetables).

```cpp
bool loadWavetableFromFile(const juce::File& file, Wavetable& wt) {
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    auto reader = formatManager.createReaderFor(file);
    if (!reader) return false;

    int numSamples = (int)reader->lengthInSamples;

    // Read audio data
    juce::AudioBuffer<float> buffer(1, numSamples);
    reader->read(&buffer, 0, numSamples, 0, true, false);

    // Resample to TABLE_SIZE if needed
    if (numSamples != Wavetable::TABLE_SIZE) {
        resampleToTableSize(buffer.getReadPointer(0), numSamples,
                           wt.tables[0], Wavetable::TABLE_SIZE);
    } else {
        std::copy(buffer.getReadPointer(0),
                  buffer.getReadPointer(0) + numSamples,
                  wt.tables[0]);
    }

    // Add guard sample
    wt.tables[0][Wavetable::TABLE_SIZE] = wt.tables[0][0];

    // Generate mipmaps
    generateMipmaps(wt);

    delete reader;
    return true;
}
```

### Multi-Frame Wavetables
Some .wav files contain multiple single-cycle frames concatenated:

```cpp
int framesInFile = numSamples / expectedFrameSize;
// Each frame is one wavetable position for morphing
```

Common frame sizes: 256, 512, 1024, 2048 samples.

## Wavetable Morphing

### Crossfade Between Frames

```cpp
struct MorphableWavetable {
    static constexpr int MAX_FRAMES = 256;
    Wavetable frames[MAX_FRAMES];
    int numFrames;

    float readMorphed(float phase, float morphPosition, int mipmapLevel) {
        // morphPosition: 0.0 = first frame, 1.0 = last frame
        float framePos = morphPosition * (numFrames - 1);
        int frame0 = (int)framePos;
        int frame1 = std::min(frame0 + 1, numFrames - 1);
        float frameFrac = framePos - frame0;

        float s0 = readCubicHermite(frames[frame0].tables[mipmapLevel],
                                     phase, frames[frame0].tableSizes[mipmapLevel]);
        float s1 = readCubicHermite(frames[frame1].tables[mipmapLevel],
                                     phase, frames[frame1].tableSizes[mipmapLevel]);

        return s0 + frameFrac * (s1 - s0);  // Linear crossfade between frames
    }
};
```

### Spectral Morphing (Higher Quality)

Instead of crossfading time-domain waveforms, interpolate in the frequency domain:

1. FFT both frames
2. Interpolate magnitude and phase spectra separately
3. Inverse FFT the interpolated spectrum

This avoids the amplitude dip that occurs with simple crossfading of out-of-phase waveforms.

## Standard Waveform Generation

Generate classic waveforms as wavetables (with proper band-limiting via mipmaps):

```cpp
void generateSaw(float* table, int size) {
    for (int i = 0; i < size; ++i)
        table[i] = 2.0f * i / size - 1.0f;
}

void generateSquare(float* table, int size) {
    for (int i = 0; i < size; ++i)
        table[i] = (i < size / 2) ? 1.0f : -1.0f;
}

void generateTriangle(float* table, int size) {
    for (int i = 0; i < size; ++i) {
        float phase = (float)i / size;
        table[i] = (phase < 0.5f)
            ? 4.0f * phase - 1.0f
            : 3.0f - 4.0f * phase;
    }
}

void generatePulse(float* table, int size, float pulseWidth) {
    int threshold = (int)(pulseWidth * size);
    for (int i = 0; i < size; ++i)
        table[i] = (i < threshold) ? 1.0f : -1.0f;
}
```

**Important:** These naive waveforms will alias without mipmaps. Always run `generateMipmaps()` after creating the base waveform.

## Per-Sample Playback

```cpp
float processWavetableSample(Wavetable& wt, float& phase, float frequency,
                              float sampleRate) {
    float phaseIncrement = frequency / sampleRate * Wavetable::TABLE_SIZE;
    int level = selectMipmapLevel(phaseIncrement);

    float output = readCubicHermite(wt.tables[level], phase, wt.tableSizes[level]);

    // Advance phase (wrap at table boundary)
    phase += phaseIncrement;
    while (phase >= wt.tableSizes[level])
        phase -= wt.tableSizes[level];

    return output;
}
```

## Integration with SpatialCore

For OpenSpatialSynthesizer, wavetable morphing creates evolving timbres while SpatialCore positions each voice in 3D space. Modulation matrix can route:
- **Morph position → LFO** — Sweeping through wavetable frames over time
- **Morph position → MPE slide** — Finger movement controls timbre
- **Morph position → velocity** — Harder hits select brighter frames
- **Spatial distance → morph** — Farther objects use softer/darker wavetable frames (natural acoustic behavior)
