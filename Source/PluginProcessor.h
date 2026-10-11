#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "TonalSweeper.h"
#include "NoiseEngine.h"
#include "BoomEngine.h"

class RDSBAudioProcessor : public juce::AudioProcessor {
public:
    RDSBAudioProcessor();
    ~RDSBAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Real-time accessors for the Editor
    float getMasterPhase() const { return masterPhase; }
    bool isPlaying() const { return isTransitionPlaying; }
    int getTriggerMidiNote() const { return triggerMidiNote; }
    void triggerTransition();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSP components
    TonalSweeper tonalSweeper;
    NoiseEngine noiseEngine;
    BoomEngine boomEngine;
    juce::Reverb reverb;
    juce::dsp::Phaser<float> phaser;
    juce::dsp::LadderFilter<float> ladderFilter;

    // Phase Accumulation variables
    float masterPhase = 0.0f;
    bool isTransitionPlaying = false;
    bool wasHostPlaying = false;
    long long lastLoopIndex = 0;     // transport loop counter, for exact drop detection
    bool haveLoopIndex = false;
    bool boomOnNextBlock = false;   // a drop that fell exactly on a block boundary
    double currentSampleRate = 44100.0;
    int triggerMidiNote = 36; // Default to C2 (36)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RDSBAudioProcessor)
};
