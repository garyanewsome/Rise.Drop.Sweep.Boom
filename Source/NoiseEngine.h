#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include "FastRandom.h"

/**
 * Stereo white/pink noise through a sweeping band-pass or high-pass.
 * Allocation-free; the cutoff follows the sweep in short chunks, so it is
 * smooth regardless of the host buffer size.
 */
class NoiseEngine {
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {
        currentSampleRate = spec.sampleRate;
        juce::dsp::ProcessSpec stereo = spec;
        stereo.numChannels = 2;
        filter.prepare(stereo);
        randomL.setSeed(0xC0FFEE01u);
        randomR.setSeed(0xBADC0DE5u);
        reset();
    }

    void reset() {
        filter.reset();
        pinkL.reset();
        pinkR.reset();
    }

    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                      double activePhaseStart, double activePhaseInc, float warpK, float volume,
                      int noiseType,   // 0 = White, 1 = Pink
                      int filterMode,  // 0 = Bandpass, 1 = Highpass
                      float cutoffStart, float cutoffEnd, float resonance)
    {
        if (currentSampleRate <= 0.0 || numSamples <= 0 || buffer.getNumChannels() < 2)
            return;

        filter.setType(filterMode == 1 ? juce::dsp::StateVariableTPTFilterType::highpass
                                       : juce::dsp::StateVariableTPTFilterType::bandpass);
        filter.setResonance(juce::jlimit(0.1f, 10.0f, resonance));

        float* left = buffer.getWritePointer(0) + startSample;
        float* right = buffer.getWritePointer(1) + startSample;
        const float maxCutoff = static_cast<float>(juce::jmin(20000.0, currentSampleRate * 0.49));

        constexpr int chunk = 32;
        for (int offset = 0; offset < numSamples; offset += chunk) {
            const int n = juce::jmin(chunk, numSamples - offset);

            // Cutoff at the middle of this chunk.
            const double phase = juce::jlimit(0.0, 1.0, activePhaseStart + activePhaseInc * (offset + n * 0.5));
            const float warped = static_cast<float>(std::pow(phase, static_cast<double>(warpK)));
            const float cutoff = juce::jlimit(20.0f, maxCutoff, cutoffStart + (cutoffEnd - cutoffStart) * warped);
            filter.setCutoffFrequency(cutoff);

            for (int i = 0; i < n; ++i) {
                float l, r;
                if (noiseType == 1) {
                    l = pinkL.next(randomL);
                    r = pinkR.next(randomR);
                } else {
                    l = randomL.nextBipolar();
                    r = randomR.nextBipolar();
                }
                left[offset + i] += filter.processSample(0, l * volume);
                right[offset + i] += filter.processSample(1, r * volume);
            }
        }
    }

private:
    struct PinkGenerator {
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        void reset() { b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f; }

        float next(FastRandom& rng) {
            const float white = rng.nextBipolar();
            b0 = 0.99886f * b0 + white * 0.0555179f;
            b1 = 0.99332f * b1 + white * 0.0750759f;
            b2 = 0.96900f * b2 + white * 0.1538520f;
            b3 = 0.86650f * b3 + white * 0.3104856f;
            b4 = 0.55000f * b4 + white * 0.5329522f;
            b5 = -0.7616f * b5 - white * 0.0168980f;
            const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
            b6 = white * 0.115926f;
            return pink * 0.11f;
        }
    };

    double currentSampleRate = 44100.0;
    PinkGenerator pinkL, pinkR;
    FastRandom randomL, randomR;
    juce::dsp::StateVariableTPTFilter<float> filter;
};
