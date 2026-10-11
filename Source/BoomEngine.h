#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdlib>
#include "FastRandom.h"

class BoomEngine {
public:
    BoomEngine() = default;

    void prepare(const juce::dsp::ProcessSpec& spec) {
        currentSampleRate = spec.sampleRate;

        // Configure Low Pass Filters for Noise Shockwave (24dB/octave = 2x 12dB/octave in series)
        filter1L.prepare(spec);
        filter2L.prepare(spec);
        filter1R.prepare(spec);
        filter2R.prepare(spec);

        filter1L.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter2L.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter1R.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
        filter2R.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

        const float lpfCutoff = 200.0f; // Fixed LPF cutoff for transient punch
        filter1L.setCutoffFrequency(lpfCutoff);
        filter2L.setCutoffFrequency(lpfCutoff);
        filter1R.setCutoffFrequency(lpfCutoff);
        filter2R.setCutoffFrequency(lpfCutoff);

        const float resonance = 0.707f; // Butterworth-like resonance
        filter1L.setResonance(resonance);
        filter2L.setResonance(resonance);
        filter1R.setResonance(resonance);
        filter2R.setResonance(resonance);

        reset();
    }

    void reset() {
        filter1L.reset();
        filter2L.reset();
        filter1R.reset();
        filter2R.reset();

        isSubActive = false;
        isShockwaveActive = false;
        subPhase = 0.0;
        shockwaveElapsedSamples = 0;
        subElapsedSamples = 0;
        startDelay = 0;
    }

    // Fire the impact. delaySamples positions it inside the current block, so
    // the drop lands on the exact sample the transition ends / begins.
    void trigger(int delaySamples = 0) {
        isSubActive = true;
        isShockwaveActive = true;
        subPhase = 0.0;
        shockwaveElapsedSamples = 0;
        subElapsedSamples = 0;
        startDelay = juce::jmax(0, delaySamples);
        random.setSeed(0xB00B1E55u + static_cast<uint32_t>(triggerCount++));
    }

    void processBlock(juce::AudioBuffer<float>& buffer,
                      float volume,
                      float noiseDecay,      // e.g. 0.1s (100ms)
                      float subDecay,        // e.g. 3.0s
                      float subPitchStart,   // e.g. 200.0 Hz
                      float subPitchEnd)     // e.g. 35.0 Hz
    {
        if (currentSampleRate <= 0.0) return;
        if (!isSubActive && !isShockwaveActive) return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numChannels < 2) return;

        float* leftChannel = buffer.getWritePointer(0);
        float* rightChannel = buffer.getWritePointer(1);

        const double ts = 1.0 / currentSampleRate;
        const float endFloor = std::exp(-5.0f);

        for (int sample = 0; sample < numSamples; ++sample) {
            // A delayed trigger stays silent until its sample position arrives.
            if (startDelay > 0) {
                --startDelay;
                continue;
            }

            float outL = 0.0f;
            float outR = 0.0f;

            // 1. Noise shockwave: low-passed noise burst with a short linear decay
            if (isShockwaveActive) {
                const double elapsedSec = static_cast<double>(shockwaveElapsedSamples) * ts;
                if (elapsedSec < noiseDecay) {
                    const float env = 1.0f - static_cast<float>(elapsedSec / noiseDecay);
                    const float rawL = random.nextBipolar() * env;
                    const float rawR = random.nextBipolar() * env;

                    // 24 dB/octave low-pass = two 12 dB stages in series
                    outL += filter2L.processSample(0, filter1L.processSample(0, rawL));
                    outR += filter2R.processSample(0, filter1R.processSample(0, rawR));
                    shockwaveElapsedSamples++;
                } else {
                    isShockwaveActive = false;
                }
            }

            // 2. Sub rumble: sine with a fast pitch drop and an exponential tail
            if (isSubActive) {
                const double elapsedSec = static_cast<double>(subElapsedSamples) * ts;
                if (elapsedSec < subDecay) {
                    const double pitchSweepDur = 0.075;
                    const float progress = static_cast<float>(std::min(1.0, elapsedSec / pitchSweepDur));
                    const float subFreq = subPitchEnd + (subPitchStart - subPitchEnd) * std::pow(1.0f - progress, 3.0f);

                    // Exponential tail, normalised so it reaches exactly zero (no click when it ends).
                    const float raw = std::exp(-static_cast<float>(elapsedSec) * (5.0f / subDecay));
                    const float ampEnv = juce::jmax(0.0f, (raw - endFloor) / (1.0f - endFloor));

                    const float subSample = std::sin(static_cast<float>(subPhase)) * ampEnv;
                    outL += subSample * 0.707f;
                    outR += subSample * 0.707f;

                    subPhase += 2.0 * juce::MathConstants<double>::pi * subFreq * ts;
                    if (subPhase >= 2.0 * juce::MathConstants<double>::pi)
                        subPhase -= 2.0 * juce::MathConstants<double>::pi;
                    subElapsedSamples++;
                } else {
                    isSubActive = false;
                }
            }

            leftChannel[sample] += outL * volume;
            rightChannel[sample] += outR * volume;
        }
    }

    bool isActive() const {
        return isSubActive || isShockwaveActive;
    }

private:
    double currentSampleRate = 44100.0;

    // Filter states for stereo LPF white noise burst
    juce::dsp::StateVariableTPTFilter<float> filter1L, filter2L;
    juce::dsp::StateVariableTPTFilter<float> filter1R, filter2R;

    // Sub-bass oscillator state
    double subPhase = 0.0;
    bool isSubActive = false;
    bool isShockwaveActive = false;

    int64_t shockwaveElapsedSamples = 0;
    int64_t subElapsedSamples = 0;
    int startDelay = 0;
    uint32_t triggerCount = 0;
    FastRandom random;
};
