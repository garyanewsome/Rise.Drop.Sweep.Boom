#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <cstdlib>

class NoiseEngine {
public:
    NoiseEngine() {
        // Seed random number generator
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
    }

    void prepare(const juce::dsp::ProcessSpec& spec) {
        currentSampleRate = spec.sampleRate;
        filter.prepare(spec);
        reset();
    }

    void reset() {
        filter.reset();
        pinkNoiseL.reset();
        pinkNoiseR.reset();
    }

    void processBlock(juce::AudioBuffer<float>& buffer,
                      float activePhase,
                      float warpK,
                      float volume,
                      int noiseType, // 0 = White, 1 = Pink
                      int filterMode, // 0 = Bandpass, 1 = Highpass
                      float cutoffStart,
                      float cutoffEnd,
                      float resonance)
    {
        if (currentSampleRate <= 0.0) return;

        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        if (numChannels < 2) return;

        // 1. Generate stereo-uncorrelated noise into a temporary buffer
        juce::AudioBuffer<float> noiseBuffer(numChannels, numSamples);
        float* leftNoise = noiseBuffer.getWritePointer(0);
        float* rightNoise = noiseBuffer.getWritePointer(1);

        if (noiseType == 1) { // Pink Noise
            for (int sample = 0; sample < numSamples; ++sample) {
                leftNoise[sample] = pinkNoiseL.nextSample() * volume;
                rightNoise[sample] = pinkNoiseR.nextSample() * volume;
            }
        } else { // White Noise
            for (int sample = 0; sample < numSamples; ++sample) {
                leftNoise[sample] = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * volume;
                rightNoise[sample] = (static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f) * volume;
            }
        }

        // 2. Configure the State Variable Filter
        if (filterMode == 1) {
            filter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
        } else {
            filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
        }

        // Map active phase with warp curve to determine filter cutoff
        float warpedPhase = std::pow(activePhase, warpK);
        float cutoff = cutoffStart + (cutoffEnd - cutoffStart) * warpedPhase;

        // Clamp cutoff to safe bounds
        cutoff = juce::jlimit(20.0f, static_cast<float>(std::min(20000.0, currentSampleRate * 0.49)), cutoff);
        filter.setCutoffFrequency(cutoff);
        filter.setResonance(juce::jlimit(0.1f, 10.0f, resonance));

        // 3. Filter the noise block in-place
        juce::dsp::AudioBlock<float> block(noiseBuffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        filter.process(context);

        // 4. Add the filtered noise to the main output buffer
        float* mainLeft = buffer.getWritePointer(0);
        float* mainRight = buffer.getWritePointer(1);
        for (int sample = 0; sample < numSamples; ++sample) {
            mainLeft[sample] += leftNoise[sample];
            mainRight[sample] += rightNoise[sample];
        }
    }

private:
    struct PinkNoiseGenerator {
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, b3 = 0.0f, b4 = 0.0f, b5 = 0.0f, b6 = 0.0f;

        void reset() {
            b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
        }

        inline float nextSample() {
            float white = static_cast<float>(std::rand()) / RAND_MAX * 2.0f - 1.0f;
            b0 = 0.99886f * b0 + white * 0.0555179f;
            b1 = 0.99332f * b1 + white * 0.0750759f;
            b2 = 0.96900f * b2 + white * 0.1538520f;
            b3 = 0.86650f * b3 + white * 0.3104856f;
            b4 = 0.55000f * b4 + white * 0.5329522f;
            b5 = -0.7616f * b5 - white * 0.0168980f;
            float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
            b6 = white * 0.115926f;
            return pink * 0.11f; // Scaled to ~1.0 RMS peak
        }
    };

    double currentSampleRate = 44100.0;
    PinkNoiseGenerator pinkNoiseL, pinkNoiseR;
    juce::dsp::StateVariableTPTFilter<float> filter;
};
