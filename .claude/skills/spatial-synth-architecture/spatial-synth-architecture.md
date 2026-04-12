---
name: spatial-synth-architecture
description: Patterns for mapping polyphonic synthesizer voices to SpatialCore spatial objects, including MPE-to-position routing, spatial voice allocation, and per-voice trajectory management
---

# Spatial Synthesizer Architecture

How to build a polyphonic synthesizer on SpatialCore where each voice is an independent spatial object.

## Core Challenge

SpatialCore supports up to 12 spatial objects. A polyphonic synth maps voices to these object slots. The challenge: voice allocation must consider both musical needs (which notes to play) AND spatial needs (where to place them).

## Voice-to-Object Mapping

### Direct Mapping (Recommended for SML)
Each active voice occupies one SpatialCore object slot (1:1 mapping).

```
Voice 0 → Object 0 → Position (az=0°, el=0°, dist=0.5)
Voice 1 → Object 1 → Position (az=120°, el=0°, dist=0.5)
Voice 2 → Object 2 → Position (az=240°, el=0°, dist=0.5)
...
Voice 11 → Object 11 → Position (az=330°, el=30°, dist=0.7)
```

**Max polyphony = 12** (limited by SpatialCore's 12 object slots).

### Advantages
- Simple, predictable
- Each voice has independent spatial position, trajectory, Doppler
- ADM-OSC controls individual voice positions
- Spatial map shows each voice as a distinct dot

### Voice Allocation Strategies

| Strategy | When to Use | Behavior |
|----------|-------------|----------|
| **Round-robin** | General purpose | Assign voices 0→11 sequentially, wrap |
| **Nearest-available** | Spatial awareness | Find the free slot whose default position is closest to the desired position |
| **Spread-maximizing** | Immersive patches | Allocate to maximize angular distance between active voices |
| **Zone-based** | Split/layer patches | Low notes → objects 0-5 (left hemisphere), high notes → objects 6-11 (right hemisphere) |

### Voice Stealing with Spatial Awareness

When all 12 slots are occupied and a new note arrives:
1. Find the voice to steal (oldest, quietest, or same-note)
2. Apply 5ms fade-out to the stolen voice (prevents click)
3. During fade-out, begin crossfading the spatial position to the new voice's target position
4. After fade completes, reassign the object slot to the new voice

**Critical:** Never teleport an object — always crossfade position changes over 5-20ms to prevent Doppler artifacts and HRTF discontinuities.

## MPE-to-Position Routing

### What is MPE?
MIDI Polyphonic Expression (MPE) provides per-note control via:
- **Pitch Bend** (per channel) — typically ±48 semitones
- **Pressure/Aftertouch** (per channel) — continuous 0.0-1.0
- **Slide/CC74** (per channel) — continuous 0.0-1.0

### Mapping MPE to Spatial Position

| MPE Source | Spatial Destination | Mapping | Why |
|-----------|-------------------|---------|-----|
| **Slide (CC74)** | Azimuth | 0.0 → -180°, 0.5 → 0°, 1.0 → +180° | Horizontal finger movement = horizontal spatial movement |
| **Pressure** | Elevation | 0.0 → 0°, 1.0 → +90° | Press harder = push sound upward |
| **Velocity** | Distance | 127 → 0.2 (close), 1 → 1.0 (far) | Harder hits = closer, more present |
| **Pitch Bend** | Pitch only (not spatial) | Standard pitch bend | Keep separate from spatial control |

### Implementation Pattern

```cpp
void handleMPENote(int channel, int note, int velocity, float slide, float pressure) {
    int voiceIndex = allocateVoice(note, velocity);

    // Set initial spatial position from MPE
    float azimuth = (slide - 0.5f) * 360.0f;  // -180° to +180°
    float elevation = pressure * 90.0f;         // 0° to +90°
    float distance = 1.0f - (velocity / 127.0f) * 0.8f;  // 0.2 to 1.0

    // Update SpatialCore object position
    setObjectPosition(voiceIndex, azimuth, elevation, distance);
}

void handleMPESlideUpdate(int channel, float slide) {
    int voiceIndex = getVoiceForChannel(channel);
    float azimuth = (slide - 0.5f) * 360.0f;

    // Smooth the position change (avoid Doppler artifacts)
    smoothObjectAzimuth(voiceIndex, azimuth, 10.0f);  // 10ms ramp
}
```

### Modulation Matrix Integration

MPE sources should be available in the modulation matrix alongside LFOs and envelopes:

```
Mod Slot 1: Slide → Azimuth (amount: 100%)
Mod Slot 2: Pressure → Elevation (amount: 75%)
Mod Slot 3: Velocity → Distance (amount: -80%)
Mod Slot 4: LFO 1 → Azimuth (amount: 30%)  // Subtle orbit on top of MPE
```

## Per-Voice Trajectories

### How Trajectories Work with Voices

Each voice can have its own trajectory shape and speed, set by the voice allocation or preset:

| Mode | Behavior |
|------|----------|
| **Static** | Voice stays at its assigned position. Best for pads. |
| **Per-voice orbit** | Each voice orbits independently (different phase offsets). Creates spatial richness. |
| **Chord-linked** | All voices in a chord share one trajectory but maintain angular offsets. Chord rotates as a unit. |
| **MPE-override** | MPE expression overrides trajectory. Finger movement = direct spatial control. |

### Phase Offset for Polyphonic Trajectories

When multiple voices use the same trajectory shape, offset their phases:

```cpp
float phaseOffset = (voiceIndex / (float)numActiveVoices) * 2.0f * M_PI;
float trajectoryPhase = basePhase + phaseOffset;
```

This ensures voices spread out around the trajectory path rather than clustering.

## Unison Mode — Spatial Detuning

Traditional unison stacks detuned copies in stereo. Spatial unison positions each copy at a different 3D location:

```
Unison 4 voices, note C4:
  Copy 0: -3 cents, azimuth 45°,  elevation 0°,  distance 0.4
  Copy 1: +3 cents, azimuth 135°, elevation 15°, distance 0.5
  Copy 2: -7 cents, azimuth 225°, elevation 0°,  distance 0.4
  Copy 3: +7 cents, azimuth 315°, elevation 15°, distance 0.5
```

**Result:** Super Saw-style fatness with full 3D spatial spread.

Each unison copy consumes one SpatialCore object slot. With 4-voice unison, max polyphony = 3 notes (12 slots / 4 copies).

## ADM-OSC Integration for Synth Voices

### Sending Voice Positions
SpatialCore's ADM-OSC Send broadcasts each active voice's position. External spatial audio workstations (SPAT Revolution, L-ISA Controller) see each synth voice as an independent audio object.

### Receiving Voice Positions
External systems can reposition synth voices in real-time via ADM-OSC. When a voice receives an OSC position update:
1. The OSC position takes priority (500ms override timeout)
2. The trajectory is paused (not stopped)
3. When OSC updates stop, the trajectory resumes from the current position

## Performance Considerations

| Concern | Solution |
|---------|----------|
| 12 per-source HRTF convolutions | Each PartitionedConvolver runs independently. Budget ~2ms total CPU at 44.1kHz. |
| Position smoothing | Use LinearSmoothedValue (10ms ramp) for all position changes to avoid Doppler pops |
| Voice allocation | O(1) allocation using fixed-size pool with free-list |
| Trajectory computation | Evaluate trajectory shapes at block rate (not sample rate) |
| Modulation matrix | Pre-sort sources by voice affinity, skip inactive voices |
