#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include "FastRandom.h"

/**
 * Multi-saw riser/downlifter voice stack. Everything is fixed-size and
 * allocation-free, so parameter changes are safe on the audio thread and never
 * restart the oscillator phases (no clicks when unison/detune/width move).
 */
class TonalSweeper {
public:
    static constexpr int maxVoices = 7;

    void prepare(double sampleRate) {
        currentSampleRate = sampleRate;
        random.setSeed(0x1234ABCDu);
        for (auto& v : voices)
            v.phase = static_cast<double>(random.nextUnit());
        configured = false;
        hadConfig = false;
        resetPumper();
    }

    // Restart the pump LFO (called at the start of every transition / loop).
    void resetPumper() { pumpPhase = 0.0; }

    /**
     * Adds the sweeper into buffer[startSample, startSample + numSamples).
     *
     * @param activePhaseStart   sweep position (0..1) at the first sample
     * @param activePhaseInc     change of sweep position per sample (negative for downlifters)
     * @param pumpStart/pumpEnd  pump rate in pumps per beat at sweep start/end
     * @param beatsPerSample     host tempo in beats per sample
     * @param transitionBeats    length of the transition in beats (drives the arp clock)
     * @param reversed           true for downlifters, so the arp still moves forward in time
     */
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples,
                      double activePhaseStart, double activePhaseInc, float warpK, float volume,
                      float pitchStart, float pitchEnd, int unisonVoices, float detune, float panSpread,
                      float pumperDepth, float pumpStart, float pumpEnd,
                      double beatsPerSample, double transitionBeats, bool reversed,
                      bool arpEnable, int arpRate, int arpPattern)
    {
        if (currentSampleRate <= 0.0 || numSamples <= 0 || buffer.getNumChannels() < 2)
            return;

        configure(juce::jlimit(1, maxVoices, unisonVoices), detune, panSpread);

        float* left = buffer.getWritePointer(0) + startSample;
        float* right = buffer.getWritePointer(1) + startSample;

        const float voiceNorm = 1.0f / std::sqrt(static_cast<float>(numActive));
        const double nyquistLimit = currentSampleRate * 0.45;
        const double smoothing = 1.0 - std::exp(-1.0 / (0.01 * currentSampleRate));
        const double stepsPerBeat = arpRate == 0 ? 2.0 : (arpRate == 1 ? 4.0 : 8.0);

        for (int s = 0; s < numSamples; ++s) {
            const double phase = juce::jlimit(0.0, 1.0, activePhaseStart + activePhaseInc * s);
            const double warped = std::pow(phase, static_cast<double>(warpK));

            // Arpeggiator: steps advance with elapsed time through the transition.
            int arpOffset = 0;
            if (arpEnable) {
                const double elapsed = reversed ? 1.0 - phase : phase;
                const int step = static_cast<int>(elapsed * transitionBeats * stepsPerBeat);
                static constexpr int up[] = {0, 4, 7, 12};
                static constexpr int down[] = {12, 7, 4, 0};
                static constexpr int updown[] = {0, 4, 7, 12, 7, 4};
                static constexpr int octaves[] = {0, 12, 24, 12};
                switch (arpPattern) {
                    case 0:  arpOffset = up[step % 4]; break;
                    case 1:  arpOffset = down[step % 4]; break;
                    case 2:  arpOffset = updown[step % 6]; break;
                    default: arpOffset = octaves[step % 4]; break;
                }
            }

            const double midi = static_cast<double>(pitchStart)
                                + static_cast<double>(pitchEnd - pitchStart) * warped + arpOffset;
            const double centerFreq = 440.0 * std::pow(2.0, (midi - 69.0) / 12.0);

            // Tempo-synced pump: rate (pumps per beat) glides start -> end across the sweep.
            const double pumpRate = pumpStart + (pumpEnd - pumpStart) * phase;
            pumpPhase += pumpRate * beatsPerSample;
            if (pumpPhase >= 1.0) pumpPhase -= std::floor(pumpPhase);
            const float pumpGain = 1.0f - pumperDepth * static_cast<float>(
                0.5 + 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * pumpPhase));

            const float gain = volume * voiceNorm * pumpGain;
            float sumL = 0.0f, sumR = 0.0f;

            for (int i = 0; i < numActive; ++i) {
                auto& v = voices[static_cast<size_t>(i)];
                // Glide detune and pan toward their targets (about 10 ms), so
                // turning a knob mid-sweep never steps the signal.
                v.detuneFactor += (v.targetDetune - v.detuneFactor) * smoothing;
                v.panL += (v.targetPanL - v.panL) * static_cast<float>(smoothing);
                v.panR += (v.targetPanR - v.panR) * static_cast<float>(smoothing);

                // Keep every voice below Nyquist (arp and trigger key can push it over).
                const double f = juce::jmin(centerFreq * v.detuneFactor, nyquistLimit);
                const double dt = f / currentSampleRate;

                const float osc = sawPolyBlep(v.phase, dt);
                sumL += osc * v.panL;
                sumR += osc * v.panR;

                v.phase += dt;
                if (v.phase >= 1.0) v.phase -= 1.0;
            }

            left[s] += sumL * gain;
            right[s] += sumR * gain;
        }
    }

private:
    struct UnisonVoice {
        double phase = 0.0;
        double detuneFactor = 1.0;      // smoothed toward targetDetune
        float panL = 0.7071f;           // smoothed toward targetPanL/R
        float panR = 0.7071f;
        double targetDetune = 1.0;
        float targetPanL = 0.7071f;
        float targetPanR = 0.7071f;
    };

    // Recomputes detune/pan in place. Phases are kept, so nothing clicks; voices
    // that are newly switched on start at a random phase.
    void configure(int numVoices, float detune, float panSpread) {
        if (configured && numVoices == numActive && std::abs(detune - lastDetune) < 1.0e-5f
            && std::abs(panSpread - lastPan) < 1.0e-5f)
            return;

        const int previous = configured ? numActive : 0;
        numActive = numVoices;
        lastDetune = detune;
        lastPan = panSpread;
        configured = true;

        for (int i = 0; i < numVoices; ++i) {
            auto& v = voices[static_cast<size_t>(i)];
            const bool fresh = i >= previous;
            if (fresh)
                v.phase = static_cast<double>(random.nextUnit());

            if (numVoices == 1) {
                v.targetDetune = 1.0;
                v.targetPanL = v.targetPanR = 0.7071f;
            } else {
                const float spread = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(numVoices - 1);
                v.targetDetune = 1.0 + static_cast<double>(spread * detune);
                const float angle = (spread * panSpread + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
                v.targetPanL = std::cos(angle);
                v.targetPanR = std::sin(angle);
            }
            // The voice is fully there from the start the first time; afterwards it glides.
            if (!hadConfig) {
                v.detuneFactor = v.targetDetune;
                v.panL = v.targetPanL;
                v.panR = v.targetPanR;
            }
        }
        hadConfig = true;
    }

    static float polyBlep(double t, double dt) {
        if (t < dt) {
            const double x = t / dt;
            return static_cast<float>(x + x - x * x - 1.0);
        }
        if (t > 1.0 - dt) {
            const double x = (t - 1.0) / dt;
            return static_cast<float>(x * x + x + x + 1.0);
        }
        return 0.0f;
    }

    static float sawPolyBlep(double phase, double dt) {
        return static_cast<float>(2.0 * phase - 1.0) - polyBlep(phase, dt);
    }

    double currentSampleRate = 44100.0;
    std::array<UnisonVoice, maxVoices> voices {};
    int numActive = 1;
    bool configured = false;
    bool hadConfig = false;
    float lastDetune = 0.0f, lastPan = 0.0f;
    double pumpPhase = 0.0;
    FastRandom random;
};
