---
name: reverb-algorithms
description: Use when implementing reverb in JUCE C++ plugins — algorithmic (FDN, Dattorro plate, Schroeder), convolution, physical plate/spring modeling, or shimmer reverb. NOT for spatial panning, synthesis, or chorus/flanger.
---

# Reverberation Algorithms

## Which Reverb Algorithm?

| Algorithm | Best For | CPU | Quality |
|-----------|----------|-----|---------|
| **Dattorro Plate** | General-purpose, studio reverb | Low | High |
| **FDN 8-16ch** | Rooms, halls, flexible tuning | Medium | High |
| **Convolution** | Exact space reproduction | High | Highest |
| **Freeverb** | Simple/quick implementation | Very Low | Medium |
| **Shimmer** | Ambient, ethereal textures | High | Stylistic |
| **Spring model** | Guitar amp, vintage character | Medium | Stylistic |

**For JUCE built-in:** Use `juce::dsp::Convolution` for convolution reverb — handles partitioned FFT automatically.

## FDN Reverb Skeleton

Feedback Delay Network — the most flexible algorithmic approach.

```cpp
class FDNReverb {
public:
    static constexpr int N = 8;  // 8 delay lines

    void prepare(float sampleRate) {
        // Coprime delay lengths for density
        const int delays[] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };
        for (int i = 0; i < N; ++i) {
            int len = static_cast<int>(delays[i] * sampleRate / 44100.0f);
            delayLines[i].resize(len, 0.0f);
            writePos[i] = 0;
        }
        sr = sampleRate;
    }

    void setParams(float decaySeconds, float damping) {
        for (int i = 0; i < N; ++i) {
            int M = static_cast<int>(delayLines[i].size());
            feedbackGain[i] = std::pow(10.0f, -3.0f * M / (sr * decaySeconds));
        }
        dampCoeff = damping;  // 0-1, higher = more HF damping
    }

    std::pair<float, float> processSample(float inputL, float inputR) noexcept {
        float input = (inputL + inputR) * 0.5f;
        float outputs[N];

        // Read from delay lines
        for (int i = 0; i < N; ++i)
            outputs[i] = delayLines[i][writePos[i]];

        // Householder feedback matrix: A = I - (2/N)*ones
        float sum = 0.0f;
        for (int i = 0; i < N; ++i) sum += outputs[i];
        float householder = (2.0f / N) * sum;

        // Write back with feedback + damping
        for (int i = 0; i < N; ++i) {
            float fb = outputs[i] * feedbackGain[i];
            float mixed = fb - householder + outputs[i];
            // One-pole damping filter
            dampState[i] += dampCoeff * (mixed - dampState[i]);
            delayLines[i][writePos[i]] = input + dampState[i];
            writePos[i] = (writePos[i] + 1) % delayLines[i].size();
        }

        // Stereo output from alternating taps
        float outL = outputs[0] + outputs[2] + outputs[4] + outputs[6];
        float outR = outputs[1] + outputs[3] + outputs[5] + outputs[7];
        return { outL * 0.25f, outR * 0.25f };
    }

    void reset() {
        for (int i = 0; i < N; ++i) {
            std::fill(delayLines[i].begin(), delayLines[i].end(), 0.0f);
            dampState[i] = 0.0f;
        }
    }

private:
    std::vector<float> delayLines[N];
    int writePos[N] = {};
    float feedbackGain[N] = {};
    float dampState[N] = {};
    float dampCoeff = 0.3f;
    float sr = 44100.0f;
};
```

## Dattorro Plate Quick Reference

From "Effect Design Part 1" (JAES 45(9), 1997). Best quality-to-CPU ratio.

```
Input → BW_LPF → 4 input diffusion allpass → Tank

Tank (figure-8 cross-coupled):
  Loop 1: AP_mod(672) → Delay(4453) → Damp_LPF → ×decay → AP(1800) → Delay(3720) → Loop 2
  Loop 2: AP_mod(908) → Delay(4217) → Damp_LPF → ×decay → AP(2656) → Delay(3163) → Loop 1

All delay values at 29761 Hz — scale by fs/29761.
Stereo output: 14 taps from both loops, some subtracted for decorrelation.
```

Key params: `decay` (0-0.999), `damping` (0-1), modulated allpass ~1Hz ±8 samples.

## Common Mistakes

| Mistake | Fix |
|---|---|
| Feedback gain >= 1.0 — infinite buildup | Keep `decay` < 0.999; always verify `feedbackGain[i] < 1.0` |
| No modulation — metallic/ringing artifacts | Add slow LFO (0.5-2Hz, 1-8 sample depth) on delay reads, different rate per line |
| No damping filter — unrealistic bright tail | Add one-pole LP in each feedback path; real rooms absorb HF faster |
| Delay lengths with common factors — resonant modes | Use mutually prime delay lengths; scale from 44.1kHz reference |
| Convolution IR not sample-rate matched | Resample IR to match project rate, or use `juce::dsp::Convolution` which handles it |
| Mixing wet/dry incorrectly — phase issues | Use equal-power crossfade: `dry * cos(mix*pi/2) + wet * sin(mix*pi/2)` |

---

See **[REFERENCE.md](REFERENCE.md)** for full theory: Schroeder/Moorer equations, Hadamard/Householder matrices, Dattorro complete topology, convolution partitioning (uniform/non-uniform/zero-latency), physical plate FDTD, spring dispersive modeling, shimmer architecture, Freeverb delay values, Velvet Noise, Scattering Delay Networks, IR library licensing, famous hardware characteristics (EMT/Lexicon/Bricasti), CPU comparison tables, and academic references.
