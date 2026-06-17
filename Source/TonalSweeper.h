#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

class TonalSweeper {
public:
    TonalSweeper() {
        // Seed random number generator
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
    }

    void prepare(double sampleRate) {
        currentSampleRate = sampleRate;
        for (auto& voice : voices) {
            voice.phase = static_cast<double>(std::rand()) / RAND_MAX;
        }
    }

    void processBlock(juce::AudioBuffer<float>& buffer,
                      double activePhaseStart,
                      double activePhaseIncrement,
                      float warpK,
                      float volume,
                      float pitchStart,
                      float pitchEnd,
                      int unisonVoices,
                      float detune,
                      float panSpread,
                      float pumperDepth,
                      float pumperRateStart,
                      float pumperRateEnd,
                      float numBars,
                      bool arpEnable,
                      int arpRate,      // 0 = 1/8, 1 = 1/16, 2 = 1/32
                      int arpPattern)   // 0 = Up, 1 = Down, 2 = Up-Down, 3 = Octaves
    {
        if (currentSampleRate <= 0.0) return;

        // Rebuild voices if count, detune, or panSpread changed
        if (unisonVoices != lastUnisonVoices || 
            std::abs(detune - lastDetune) > 0.0001f || 
            std::abs(panSpread - lastPanSpread) > 0.0001f) {
            rebuildVoices(unisonVoices, detune, panSpread);
        }

        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        if (numChannels < 2) return; // Expect stereo

        float* leftChannel = buffer.getWritePointer(0);
        float* rightChannel = buffer.getWritePointer(1);

        // Process sample loop
        for (int sample = 0; sample < numSamples; ++sample) {
            // Calculate active phase for this sample
            double activePhase = activePhaseStart + sample * activePhaseIncrement;
            activePhase = juce::jlimit(0.0, 1.0, activePhase);

            double warpedPhase = std::pow(activePhase, warpK);

            // Arpeggiator note offset
            int arpOffset = 0;
            if (arpEnable)
            {
                double beatsPerBar = 4.0;
                double currentBeat = activePhase * numBars * beatsPerBar;
                
                // Steps per beat: 0 = 1/8 (2 steps), 1 = 1/16 (4 steps), 2 = 1/32 (8 steps)
                double stepsPerBeat = (arpRate == 0) ? 2.0 : ((arpRate == 1) ? 4.0 : 8.0);
                int stepIndex = static_cast<int>(currentBeat * stepsPerBeat);
                
                if (arpPattern == 0) // Up
                {
                    int pattern[] = { 0, 4, 7, 12 };
                    arpOffset = pattern[stepIndex % 4];
                }
                else if (arpPattern == 1) // Down
                {
                    int pattern[] = { 12, 7, 4, 0 };
                    arpOffset = pattern[stepIndex % 4];
                }
                else if (arpPattern == 2) // Up-Down
                {
                    int pattern[] = { 0, 4, 7, 12, 7, 4 };
                    arpOffset = pattern[stepIndex % 6];
                }
                else // Octaves
                {
                    int pattern[] = { 0, 12, 24, 12 };
                    arpOffset = pattern[stepIndex % 4];
                }
            }

            // Convert target pitch from MIDI note to Hz (adding the arpeggiator offset)
            float currentMidiNote = pitchStart + (pitchEnd - pitchStart) * warpedPhase + arpOffset;
            float centerFreq = midiNoteToHz(currentMidiNote);

            // Calculate amplitude pump factor for this sample
            double pumperTheta = 2.0 * juce::MathConstants<double>::pi * numBars * activePhase * 
                                 (pumperRateStart + (pumperRateEnd - pumperRateStart) * 0.5 * activePhase);
            float pumpVolumeFactor = 1.0f - pumperDepth * static_cast<float>(std::pow(std::cos(pumperTheta * 0.5), 2.0));
            float targetVolume = volume * (1.0f / std::sqrt(static_cast<float>(voices.size()))) * pumpVolumeFactor;

            float sumL = 0.0f;
            float sumR = 0.0f;

            for (auto& voice : voices) {
                // Calculate frequency for this unison voice
                double voiceFreq = centerFreq * voice.detuneFactor;
                double dt = voiceFreq / currentSampleRate;

                // Generate PolyBLEP sawtooth
                float oscVal = sawPolyBlep(voice.phase, dt);

                sumL += oscVal * voice.panL;
                sumR += oscVal * voice.panR;

                // Advance phase
                voice.phase += dt;
                if (voice.phase >= 1.0) {
                    voice.phase -= 1.0;
                }
            }

            // Sum and apply volume
            leftChannel[sample] += sumL * targetVolume;
            rightChannel[sample] += sumR * targetVolume;
        }
    }

private:
    struct UnisonVoice {
        double phase = 0.0;
        double detuneFactor = 1.0;
        float panL = 0.707f;
        float panR = 0.707f;
    };

    double currentSampleRate = 44100.0;
    std::vector<UnisonVoice> voices;

    int lastUnisonVoices = -1;
    float lastDetune = -1.0f;
    float lastPanSpread = -1.0f;

    void rebuildVoices(int numVoices, float detune, float panSpread) {
        lastUnisonVoices = numVoices;
        lastDetune = detune;
        lastPanSpread = panSpread;

        voices.clear();

        if (numVoices <= 1) {
            UnisonVoice v;
            v.detuneFactor = 1.0;
            v.panL = 0.707f;
            v.panR = 0.707f;
            v.phase = static_cast<double>(std::rand()) / RAND_MAX;
            voices.push_back(v);
            return;
        }

        for (int i = 0; i < numVoices; ++i) {
            UnisonVoice v;
            float frac = static_cast<float>(i) / static_cast<float>(numVoices - 1); // 0.0 to 1.0
            float detuneVal = -1.0f + 2.0f * frac; // -1.0f to 1.0f
            
            // Logarithmic/linear spread detuning
            v.detuneFactor = 1.0 + (detuneVal * detune);

            // Equal power panning
            float panVal = detuneVal * panSpread; // -panSpread to panSpread
            float angle = (panVal + 1.0f) * 0.25f * static_cast<float>(juce::MathConstants<double>::pi);
            v.panL = std::cos(angle);
            v.panR = std::sin(angle);

            // Keep/randomize phase
            v.phase = static_cast<double>(std::rand()) / RAND_MAX;
            voices.push_back(v);
        }
    }

    inline float midiNoteToHz(float midiNote) {
        return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
    }

    // PolyBLEP anti-aliased saw wave generator
    inline float polyBlep(double t, double dt) {
        if (t < dt) {
            double t_div_dt = t / dt;
            return static_cast<float>(t_div_dt + t_div_dt - t_div_dt * t_div_dt - 1.0);
        } else if (t > 1.0 - dt) {
            double t_div_dt = (t - 1.0) / dt;
            return static_cast<float>(t_div_dt + t_div_dt + t_div_dt * t_div_dt + 1.0);
        }
        return 0.0f;
    }

    inline float sawPolyBlep(double phase, double dt) {
        float val = static_cast<float>(2.0 * phase - 1.0);
        return val - polyBlep(phase, dt);
    }
};
