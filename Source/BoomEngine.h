#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdlib>

class BoomEngine {
public:
    BoomEngine() {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
    }

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
    }

    void trigger() {
        isSubActive = true;
        isShockwaveActive = true;
        subPhase = 0.0;
        shockwaveElapsedSamples = 0;
        subElapsedSamples = 0;
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

        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        if (numChannels < 2) return;

        float* leftChannel = buffer.getWritePointer(0);
        float* rightChannel = buffer.getWritePointer(1);

        double ts = 1.0 / currentSampleRate;

        for (int sample = 0; sample < numSamples; ++sample) {
            float outL = 0.0f;
            float outR = 0.0f;

            // 1. Process Noise Shockwave (LPF white noise with short linear decay)
            if (isShockwaveActive) {
                double elapsedSec = shockwaveElapsedSamples * ts;
                if (elapsedSec < noiseDecay) {
                    float env = 1.0f - static_cast<float>(elapsedSec / noiseDecay);
                    
                    // Stereo noise
                    float rawNoiseL = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * env;
                    float rawNoiseR = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * env;

                    // Pass through 24dB/octave LPF (two 12dB LPFs in series)
                    float filteredL = filter2L.processSample(0, filter1L.processSample(0, rawNoiseL));
                    float filteredR = filter2R.processSample(0, filter1R.processSample(0, rawNoiseR));

                    outL += filteredL;
                    outR += filteredR;

                    shockwaveElapsedSamples++;
                } else {
                    isShockwaveActive = false;
                }
            }

            // 2. Process Sub-bass Rumble (Sine wave with fast pitch sweep & exponential volume decay)
            if (isSubActive) {
                double elapsedSec = subElapsedSamples * ts;
                if (elapsedSec < subDecay) {
                    // Exponential pitch sweep: drops quickly in the first 75ms
                    const double pitchSweepDur = 0.075;
                    float progress = static_cast<float>(std::min(1.0, elapsedSec / pitchSweepDur));
                    float subFreq = subPitchEnd + (subPitchStart - subPitchEnd) * std::pow(1.0f - progress, 3.0f);

                    // Exponential amplitude decay
                    float ampEnv = std::exp(-static_cast<float>(elapsedSec) * (5.0f / subDecay));

                    // Generate sine wave sample
                    float subSample = std::sin(static_cast<float>(subPhase)) * ampEnv;

                    // Centered pan
                    outL += subSample * 0.707f;
                    outR += subSample * 0.707f;

                    // Advance sine phase
                    subPhase += 2.0 * juce::MathConstants<double>::pi * subFreq * ts;
                    if (subPhase >= 2.0 * juce::MathConstants<double>::pi) {
                        subPhase -= 2.0 * juce::MathConstants<double>::pi;
                    }

                    subElapsedSamples++;
                } else {
                    isSubActive = false;
                }
            }

            // Apply Boom Engine Volume and mix into the main buffer
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
};
