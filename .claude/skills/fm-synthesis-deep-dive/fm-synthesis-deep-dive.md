---
name: fm-synthesis-deep-dive
description: DX7-compatible FM synthesis implementation including 6-operator topologies, 32 algorithm definitions, feedback FM routing, carrier/modulator ratio tables, and phase modulation vs true FM
---

# FM Synthesis Deep Dive

Comprehensive reference for implementing FM (frequency modulation) synthesis in JUCE C++ audio plugins, with focus on DX7-compatible architectures.

## Phase Modulation vs True FM

The DX7 and most "FM" synthesizers actually use **Phase Modulation (PM)**, not true Frequency Modulation:

| | True FM | Phase Modulation (DX7) |
|---|---------|----------------------|
| **Formula** | `sin(2π × fc × t + I × sin(2π × fm × t))` | `sin(2π × fc × t + I × sin(2π × fm × t))` |
| **Difference** | Modulator affects instantaneous frequency | Modulator affects instantaneous phase |
| **Tuning** | Modulator ratio affects carrier pitch | Carrier pitch stays constant regardless of modulation depth |
| **Advantage** | — | Stable tuning at any modulation index |
| **Use this** | Never (for practical synths) | Always (DX7 standard) |

**Key insight:** PM produces identical spectra to FM but with stable pitch. Always implement PM.

## Operator Architecture

### Single Operator
```
Operator = Oscillator + Envelope + Level
  - Oscillator: sine wave (or other waveforms)
  - Envelope: 4-rate, 4-level (DX7 style) or ADSR
  - Level: 0-99 (DX7) or 0.0-1.0 (normalized)
```

### Operator Roles
- **Carrier:** Produces audible output (connected to mixer)
- **Modulator:** Modulates another operator's phase (not directly audible)
- **Feedback:** Operator modulates itself (creates sawtooth-like harmonics)

## DX7 32 Algorithms

The DX7 has 6 operators and 32 wiring configurations (algorithms). Each algorithm defines which operators are carriers, which are modulators, and how they connect.

### Notation
```
[6] → [5] → [4] → [3] → [2] → [1]  = serial chain
[3]   [6]
 ↓     ↓                              = parallel carriers
[2]   [5]
 ↓     ↓
[1]   [4]                             = two 3-op stacks
```

### Key Algorithms (most commonly used)

**Algorithm 1:** Classic 3-modulator chain
```
[6] → [5] → [4] → [3] → [2] → [1]*
```
One carrier (Op 1), five modulators in series. Extremely bright, metallic.

**Algorithm 5:** Two parallel 3-op chains
```
[6] → [5] → [4]*    [3] → [2] → [1]*
```
Two carriers (Op 1, Op 4). Good for bells, electric piano.

**Algorithm 7:** Three carriers with modulators
```
[6] → [5]*    [4] → [3]*    [2] → [1]*
```
Three 2-op pairs. Warm, organ-like.

**Algorithm 32:** All six carriers (additive)
```
[6]*  [5]*  [4]*  [3]*  [2]*  [1]*
```
Pure additive synthesis. Each operator is an independent sine partial.

### Implementation Pattern

```cpp
struct FMAlgorithm {
    struct Connection {
        int source;      // Operator index (0-5)
        int destination;  // Operator index (0-5), or -1 for output
        bool isFeedback;  // Self-modulation
    };
    int numCarriers;
    int carriers[6];           // Which operators output to mixer
    Connection connections[12]; // Max 12 connections per algorithm
    int numConnections;
};

// Example: Algorithm 1 (serial chain)
static const FMAlgorithm algo1 = {
    .numCarriers = 1,
    .carriers = {0},  // Op 1 is carrier
    .connections = {
        {5, 4, false}, // Op 6 → Op 5
        {4, 3, false}, // Op 5 → Op 4
        {3, 2, false}, // Op 4 → Op 3
        {2, 1, false}, // Op 3 → Op 2
        {1, 0, false}, // Op 2 → Op 1
        {5, 5, true},  // Op 6 feedback
    },
    .numConnections = 6
};
```

## Feedback FM

An operator modulating itself creates harmonically rich waveforms:

| Feedback Level | Resulting Waveform |
|---------------|-------------------|
| 0 | Pure sine |
| Low (0.1-0.3) | Rounded saw (warm) |
| Medium (0.4-0.6) | Full sawtooth |
| High (0.7-0.9) | Bright, harsh |
| Maximum (1.0) | Noise/chaos |

### Safe Feedback Implementation

```cpp
// Use one-sample delay to prevent infinite recursion
float feedbackSample = 0.0f;  // Previous output

float processFeedbackOperator(float phase, float feedbackAmount) {
    float modulatedPhase = phase + feedbackSample * feedbackAmount;
    float output = std::sin(modulatedPhase);
    feedbackSample = output;  // Store for next sample
    return output;
}
```

**Critical:** Always use a one-sample delay for feedback. Without it, the operator's output depends on itself in the same sample — infinite recursion.

## Carrier/Modulator Ratios

### Harmonic Ratios (integer multiples — musical)
| Ratio | Sound Character |
|-------|----------------|
| 1:1 | Fundamental reinforcement |
| 1:2 | Octave harmonics (bright) |
| 1:3 | 5th + octave (hollow, clarinet-like) |
| 1:4 | Two octaves (very bright) |
| 2:1 | Sub-octave modulation (bass emphasis) |
| 3:2 | 5th modulation (vocal quality) |

### Inharmonic Ratios (non-integer — metallic/bell-like)
| Ratio | Sound Character |
|-------|----------------|
| 1:1.414 | Bell, metallic |
| 1:2.76 | Tubular bell |
| 1:3.14 (π) | Chaotic, evolving |
| 1:7.0 | Very bright, electric |

### Detune for Warmth
Slight detuning of modulator ratio (e.g., 2.003 instead of 2.0) creates slow beating and movement.

## Modulation Index

The modulation index `I` controls brightness:
```
I = (Δf) / fm = modulator_level × modulator_frequency / fm
```

| Index | Effect |
|-------|--------|
| 0 | Pure sine (no modulation) |
| 0.1-0.5 | Subtle harmonics (warm) |
| 1.0 | Clear harmonics (bright) |
| 2.0-5.0 | Many harmonics (very bright) |
| 5.0+ | Extreme brightness, approaching noise |

### DX7 Level-to-Index Conversion
DX7 operator levels (0-99) map to modulation index via exponential curve:
```cpp
float dx7LevelToIndex(int level) {
    // DX7 uses a lookup table; this approximation is close
    return std::pow(2.0f, (level - 99.0f) / 8.0f);
}
```

## Per-Sample Processing (6-Operator)

```cpp
void processVoiceSample(float* operatorOutputs, const FMAlgorithm& algo) {
    // Process operators in REVERSE order (6 → 1)
    // Modulators must be computed before their targets
    for (int op = 5; op >= 0; --op) {
        float phase = operatorPhase[op];

        // Sum all modulation inputs to this operator
        float modulation = 0.0f;
        for (int c = 0; c < algo.numConnections; ++c) {
            if (algo.connections[c].destination == op) {
                int src = algo.connections[c].source;
                if (algo.connections[c].isFeedback)
                    modulation += feedbackSample[op] * feedbackAmount[op];
                else
                    modulation += operatorOutputs[src] * operatorLevel[src];
            }
        }

        // Phase modulation
        float modulatedPhase = phase + modulation;
        operatorOutputs[op] = std::sin(modulatedPhase) * envelope[op].getNextSample();

        // Store feedback
        feedbackSample[op] = operatorOutputs[op];

        // Advance phase
        operatorPhase[op] += phaseIncrement[op];
        if (operatorPhase[op] > juce::MathConstants<float>::twoPi)
            operatorPhase[op] -= juce::MathConstants<float>::twoPi;
    }

    // Sum carrier outputs
    float output = 0.0f;
    for (int c = 0; c < algo.numCarriers; ++c)
        output += operatorOutputs[algo.carriers[c]];

    return output;
}
```

## DX7-Style Envelopes (4-Rate, 4-Level)

```
Level: L1 ──→ L2 ──→ L3 ──→ L4
Rate:     R1     R2     R3     R4
        attack  decay sustain release

Rate = speed of transition (0=slow, 99=instant)
Level = target level (0=silent, 99=full)
```

### Conversion to Time
```cpp
float dx7RateToSeconds(int rate) {
    // Approximate: DX7 rate 99 ≈ 0ms, rate 0 ≈ 40s
    return 41.5f * std::exp(-0.07f * rate);
}
```

## Classic FM Patches (Starting Points)

### Electric Piano (DX7 Algorithm 5)
```
Op 1 (carrier): Ratio 1.0, Level 99, Env: fast attack, medium decay
Op 2 (mod):     Ratio 1.0, Level 85, Env: fast attack, faster decay
Op 3 (mod):     Ratio 14.0, Level 45, Env: instant attack, fast decay (tine)
Op 4 (carrier): Ratio 1.0, Level 90, Env: fast attack, slow decay
Op 5 (mod):     Ratio 1.0, Level 75, Env: medium attack, medium decay
Op 6 (mod):     Ratio 1.0, Level 40, Env: slow attack, very slow decay
```

### Bell (Algorithm 5, inharmonic ratios)
```
Op 1: Ratio 1.0,   Level 99
Op 2: Ratio 3.5,   Level 70, Fast decay
Op 3: Ratio 7.07,  Level 50, Fast decay
Op 4: Ratio 1.0,   Level 85
Op 5: Ratio 2.76,  Level 65, Medium decay
Op 6: Ratio 5.0,   Level 40, Medium decay
```

### Bass (Algorithm 1, minimal)
```
Op 1 (carrier): Ratio 1.0, Level 99
Op 2 (mod):     Ratio 1.0, Level 80, Velocity-sensitive envelope
Op 3-6:         Level 0 (unused)
Feedback on Op 2: 40% (adds saw-like harmonics)
```

## CPU Considerations

FM synthesis is very CPU-efficient compared to other methods:
- **Per voice:** 6 sin() calls + 6 envelope samples + routing = ~1x baseline
- **No anti-aliasing needed:** Sine oscillators produce no aliasing
- **No filter needed:** Spectral content controlled by modulation index
- **Exception:** Feedback operators at high levels can produce aliasing — consider soft-limiting feedback output

## Integration with SpatialCore

For OpenSpatialSynthesizer, each FM voice maps to one SpatialCore object. The modulation matrix can route:
- Modulation index → mapped from MPE pressure (play harder = brighter)
- Operator ratios → mapped from MPE slide (finger position = timbre)
- Algorithm selection → per-preset or even per-voice
- Feedback amount → mapped from aftertouch

This creates an instrument where physical gesture controls both timbre (FM) and spatial position simultaneously.
