# Rise.Drop.Sweep.Boom

A host-synced transition synthesizer for risers, downlifters, sweeps and impacts. Built with C++ and the
[JUCE](https://juce.com/) framework. Internal/target name: **RDSB**.

## What it does

Pick a length in bars, press play (or hit a key), and three engines sweep together then drop:

| Engine | Colour | What you get |
|---|---|---|
| **Tonal sweep** | violet | Multi-saw riser with unison, detune, stereo spread, tempo-synced pumping and an arpeggiator |
| **Noise** | cyan | White/pink noise through a sweeping band-pass or high-pass |
| **Boom** | orange | Noise punch plus a pitch-dropping sub rumble, fired exactly at the drop |
| **FX** | lime | Phaser and a 24 dB ladder filter that follow the sweep |
| **Space / shape** | pink / white | Reverb, sweep curve, drive, master |

* **Riser / Downlifter**, **Transport-synced** (locks to the DAW bar grid and loops) or **MIDI-triggered** (one-shot per key; the key transposes it).
* The drop is sample-accurate and the output is limited so stacking engines cannot clip the host.
* Hover any control for a plain-English definition; nine factory presets plus your own (`.rdsbpreset` in `~/Library/Application Support/RDSB/Presets`).
* The visualizer layers each engine's contribution in its own colour, with pump pulses, the boom tail and a live playhead.

## Architecture

```
DAW timeline (tempo, PPQ, time signature)
          |
   block plan: sweep segments + sample-accurate boom triggers
          |
   TonalSweeper (multi-saw) + NoiseEngine (SVF) + BoomEngine (sub/punch)
          |
   Phaser -> Ladder filter -> Drive -> Master -> Reverb -> Safety limiter
```

Code map: `PluginProcessor` (timing, planning, FX chain), `TonalSweeper`/`NoiseEngine`/`BoomEngine` (DSP),
`FastRandom` (real-time-safe noise), `PluginEditor` + `RDSBLookAndFeel` (UI), `Presets` (factory + user presets).

## Build, test, install

```bash
cmake -B cmake-build -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build                      # VST3, AU, Standalone (universal, macOS 14+)
cmake --build cmake-build --target RDSBTests   # headless engine tests
./cmake-build/RDSBTests_artefacts/Release/RDSBTests
./cmake-build/RDSBTests_artefacts/Release/RDSBTests --snapshot ui.png   # render the editor offscreen
installer/build_installer.sh                   # dist/Rise.Drop.Sweep.Boom-<ver>-macOS.dmg
```

The installer is unsigned by default (one-time "Open Anyway" on first run); Developer ID signing and
notarization switch on via environment variables, see the script header.

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
* Timing comes from the host playhead (tempo, PPQ position, time signature).
* Each block is planned up front: up to two sweep segments (before / after a loop boundary) plus sample-accurate boom triggers, so the drop lands on the exact sample whatever the buffer size.
* `TonalSweeper`, `NoiseEngine` and `BoomEngine` render into the planned segments; FX sweeps update in 64-sample chunks.

