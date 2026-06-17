# TransitionEngine VST

A lightweight, high-performance, host-synced transition synthesizer designed for creating dynamic risers, downlifters, sweeps, and explosive impacts (booms) directly within any modern DAW. Built with C++ using the JUCE framework.

## Project Architecture

`TransitionEngine` focuses entirely on time-based modulation curves synced directly to the DAW host timeline. Rather than relying on traditional ADSR architectures, it utilizes a normalized $0.0 \to 1.0$ master phase ramp calculated relative to the target bar length and project tempo.

```
                             ┌───────────────┐
                             │  DAW Timeline │
                             └───────┬───────┘
                                     │ (BPM, Position)
                                     ▼
                             ┌───────────────┐
                             │ HostRampGen   │
                             └───────┬───────┘
                                     │
                    ┌────────────────┼────────────────┐
                    │ Normalized     │ Normalized     │ Normalized
                    │ Ramp (0->1)    │ Ramp (0->1)    │ Trigger (1.0)
                    ▼                ▼                ▼
             ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
             │ TonalSweeper │ │ NoiseEngine  │ │  BoomEngine  │
             │ (Multi-Saw)  │ │ (Band/High)  │ │ (Sub/Smack)  │
             └──────┬───────┘ └──────┬───────┘ └──────┬───────┘
                    │                │                │
                    └────────────────┼────────────────┘
                                     │
                                     ▼
                             ┌───────────────┐
                             │ Waveshaper    │
                             │ (Saturator)   │
                             └───────┬───────┘
                                     │
                                     ▼
                             ┌───────────────┐
                             │ Spatial Reverb│
                             └───────┬───────┘
                                     │
                                     ▼
                                [Audio Out]
```

---

## Core Audio Engines

### 1. Tonal Sweeper (The Rise/Sweep)
* **Oscillator Type:** Multi-sawtooth engine with adjustable stereo unison detune.
* **Modulation:** Pitch tracks the master phase ramp using an exponential scaling curve to induce dramatic tension immediately before the drop boundary.
* **Pumper Module:** Internal LFO modulating master amplitude, accelerating in rate (quarter notes $\to$ 8th $\to$ 16th $\to$ 32nd) linked directly to the progress of the master ramp.

### 2. Noise Engine (The Air/Whoosh)
* **Generator:** Pure white and pink noise algorithms.
* **Filter:** State Variable Filter (SVF) switchable between 12dB/octave Band-Pass and High-Pass configurations.
* **Modulation:** Cutoff frequency sweeps automatically from $200\text{ Hz}$ up to $15\text{ kHz}$ along the active timeline.

### 3. Boom Engine (The Impact)
* **Transient Shockwave:** Zero-attack white noise burst passed through a fixed steep 24dB/octave Low-Pass Filter ($150\text{ Hz} - 300\text{ Hz}$) with a swift linear decay ($100\text{ ms}$).
* **Sustained Rumble:** Monophonic sine wave oscillator governed by an aggressive exponential pitch envelope dropping from $\sim200\text{ Hz}$ down to a deep sub-bass register ($30\text{ Hz} - 45\text{ Hz}$) within $75\text{ ms}$, paired with a long, trailing amplitude decay ($2.0 - 4.0\text{ s}$).
* **Triggering Logic:** Selectable between explicit MIDI mapping or auto-detonation immediately upon the master phase ramp reaching terminal capacity (`1.0`).

---

## Technical Specifications & Mathematical Models

### Host-Synced Phase Math
The master phase velocity $\omega_{\text{phase}}$ per sample is determined by tracking the sample rate $f_s$, the host tempo $B$, and the user-selected duration in musical bars $L$:

$$\Delta \phi = \frac{B \cdot \text{BeatsPerBar}}{60 \cdot f_s \cdot L}$$

The instantaneous phase position $\phi_n$ increments linearly until bounded by execution length:

$$\phi_n = \phi_{n-1} + \Delta \phi, \quad \phi \in [0.0, 1.0]$$

### Modulation Warp Curves
To prevent flat, linear sweeps, parameter mapping applies exponential scaling:

$$P_{\text{inst}} = P_{\text{start}} + (P_{\text{end}} - P_{\text{start}}) \cdot \phi_n^k$$

*Where $k > 1.0$ dictates the curve intensity, concentrating structural sonic shifts at the tail end of the transition window.*

### Harmonic Aggression (Waveshaper)
To maximize mid-range presence and translate sub-bass energy across smaller audio playback systems, the combined summing bus passes through a symmetrical soft-clipper distortion model:

$$f(x) = \tanh(\alpha \cdot x)$$

*Where $\alpha$ represents the drive scalar coefficient.*

---

## Implementation Outline (JUCE Architecture)

### 1. Audio Processing Block (`PluginProcessor.cpp`)
* Extract timing parameters via `juce::AudioPlayHead::CurrentPositionInfo`.
* Implement the sample-accurate accumulator loop for tracking progress against host transport position.
* Interleave parallel block calculations for the `TonalSweeper`, `NoiseEngine`, and `BoomEngine` components.

### 2. User Interface View (`PluginEditor.cpp`)
* Custom 2D vector graphic window rendering the real-time geometric representation of the modulation warp curve.
* Interactive sliders mapping to the curve coefficient $k$, unison detune width, and the global target bar length.
* Macro controls toggling inversion routines to instantly transform an active Riser configuration into an automated Downlifter asset.

## Compilation Requirements
* **C++ Standard:** C++17 or higher
* **Framework:** JUCE 7.x+ (Audio-Plugin Template)
* **Target Architectures:** VST3, AU, Standalone Application
