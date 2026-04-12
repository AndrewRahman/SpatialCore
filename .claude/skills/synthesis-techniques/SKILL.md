---
name: synthesis-techniques
description: Use when implementing audio synthesis in JUCE C++ plugins — oscillators, filters, envelopes, voice management, or specific methods like FM, subtractive, wavetable, granular, or physical modeling. NOT for spatial audio, reverb, or time-based effects.
---

# Audio Synthesis Techniques

## Which Synthesis Method?

| Method | Best For | CPU | Polyphony |
|--------|----------|-----|-----------|
| **Subtractive** | Classic analog sounds, pads, basses | Low | Easy |
| **FM** | Bells, electric piano, metallic tones | Very Low | Easy |
| **Wavetable** | Modern EDM, evolving textures | Low | Easy |
| **Granular** | Textures, time-stretch, ambience | High | Hard |
| **Physical Model** | Realistic instruments (strings, wind) | High | Hard |
| **Additive** | Organ, evolving spectra, resynthesis | Very High | Hard |

## Band-Limited Saw Oscillator (PolyBLEP)

The most common starting point. PolyBLEP eliminates aliasing with minimal CPU.

```cpp
class PolyBLEPOsc {
public:
    void setFrequency(float hz, float sampleRate) {
        dt = hz / sampleRate;
    }

    float nextSample() noexcept {
        float saw = 2.0f * phase - 1.0f;          // Naive saw
        saw -= polyblep(phase, dt);                 // Anti-alias
        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;
        return saw;
    }

    void reset() { phase = 0.0f; }

private:
    float phase = 0.0f, dt = 0.0f;

    static float polyblep(float t, float dt) {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
};
```

**Square wave:** Generate two saws an octave apart, subtract. Or: `square = saw - polyblep(phase, dt) + polyblep(fmod(phase+0.5, 1.0), dt)`.

## Zero-Delay Feedback Filter (SVF)

Modern standard. Simultaneous LP/HP/BP outputs, no one-sample delay artifact.

```cpp
class ZDFFilter {
public:
    void setParams(float cutoffHz, float Q, float sampleRate) {
        float w = juce::MathConstants<float>::pi * cutoffHz / sampleRate;
        g = std::tan(w);
        k = 1.0f / Q;
    }

    struct Output { float lp, bp, hp; };

    Output processSample(float input) noexcept {
        float hp = (input - (k + g) * s1 - s2) / (1.0f + k * g + g * g);
        float bp = g * hp + s1;
        float lp = g * bp + s2;
        s1 = g * hp + bp;   // State update
        s2 = g * bp + lp;
        return { lp, bp, hp };
    }

    void reset() { s1 = s2 = 0.0f; }

private:
    float g = 0.0f, k = 0.0f;
    float s1 = 0.0f, s2 = 0.0f;
};
```

## ADSR Envelope

Exponential curves for natural-sounding attack/release.

```cpp
class ADSR {
public:
    enum Stage { Idle, Attack, Decay, Sustain, Release };

    void setParams(float a, float d, float s, float r, float sampleRate) {
        attackRate  = (a > 0.001f) ? 1.0f / (a * sampleRate) : 1.0f;
        decayRate   = (d > 0.001f) ? 1.0f / (d * sampleRate) : 1.0f;
        sustainLevel = s;
        releaseRate = (r > 0.001f) ? 1.0f / (r * sampleRate) : 1.0f;
    }

    void noteOn()  { stage = Attack; }
    void noteOff() { if (stage != Idle) stage = Release; }

    float nextSample() noexcept {
        switch (stage) {
            case Attack:
                level += attackRate;
                if (level >= 1.0f) { level = 1.0f; stage = Decay; }
                break;
            case Decay:
                level -= decayRate * (level - sustainLevel + 0.0001f);
                if (level <= sustainLevel + 0.001f) { level = sustainLevel; stage = Sustain; }
                break;
            case Sustain: break;
            case Release:
                level -= releaseRate * (level + 0.0001f);
                if (level <= 0.001f) { level = 0.0f; stage = Idle; }
                break;
            case Idle: break;
        }
        return level;
    }

    bool isActive() const { return stage != Idle; }

private:
    Stage stage = Idle;
    float level = 0.0f;
    float attackRate = 0.0f, decayRate = 0.0f;
    float sustainLevel = 0.5f, releaseRate = 0.0f;
};
```

## Voice Stealing (Soft)

Apply 1-5ms fade-out before reassigning stolen voice. Prevents clicks.

```cpp
// Allocation strategy: same-note priority + release-phase first + oldest steal
Voice* findVoiceToSteal(int noteNumber) {
    // 1. Same note already playing?
    for (auto& v : voices) if (v.note == noteNumber) return &v;
    // 2. Inactive voice?
    for (auto& v : voices) if (!v.isActive()) return &v;
    // 3. Voice in release phase?
    for (auto& v : voices) if (v.isReleasing()) return &v;
    // 4. Oldest voice
    return &*std::min_element(voices.begin(), voices.end(),
        [](auto& a, auto& b) { return a.startTime < b.startTime; });
}
```

## Common Mistakes

| Mistake | Fix |
|---|---|
| No anti-aliasing on oscillators — harsh digital artifacts | Use PolyBLEP minimum; wavetable mipmap for best quality |
| Denormals causing CPU spikes in filters | `juce::ScopedNoDenormals` at top of `processBlock()` |
| Click on voice steal | Apply 1-5ms fade-out envelope before reassigning |
| Linear frequency scaling on filter cutoff | Use logarithmic/exponential mapping for perceptual uniformity |
| Hard sync without handling discontinuity | Apply PolyBLEP correction at sync reset point |
| Not scaling filter coefficients for sample rate | Recalculate in `prepareToPlay()` and on rate changes |

---

See **[REFERENCE.md](REFERENCE.md)** for full theory: FM equations/Bessel functions, all 10 synthesis methods with math, Moog ladder topology, filter comparisons (Sallen-Key, TB-303, MS-20), CPU cost tables, voice allocation strategies, unison/Super Saw detuning, and academic references.
