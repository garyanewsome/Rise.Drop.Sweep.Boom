#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "RDSBLookAndFeel.h"

/**
 * Editor for Rise.Drop.Sweep.Boom: a layered transition visualizer (one colour
 * per engine), sectioned control panels, hover definitions and a preset browser.
 */
class RDSBAudioProcessorEditor : public juce::AudioProcessorEditor,
                                 private juce::Timer
{
public:
    RDSBAudioProcessorEditor(RDSBAudioProcessor&);
    ~RDSBAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Knob {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct Choice {
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };
    struct Toggle {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };
    struct Section {
        juce::String title;
        juce::Colour accent;
        std::vector<Toggle*> toggles;
        std::vector<Choice*> choices;
        std::vector<Knob*> knobs;
        int perRow = 3;
        float weight = 1.0f;
        juce::Rectangle<float> bounds;
    };

    void timerCallback() override;
    void drawVisualizer(juce::Graphics& g, juce::Rectangle<int> area);

    void addKnob(Knob&, const juce::String& paramId, const juce::String& title, const juce::String& tip,
                 juce::Colour accent);
    void addChoice(Choice&, const juce::String& paramId, const juce::StringArray& items,
                   const juce::String& tip, juce::Colour accent);
    void addToggle(Toggle&, const juce::String& paramId, const juce::String& title,
                   const juce::String& tip, juce::Colour accent);

    void refreshPresetList(const juce::String& selectName = {});
    void stepPreset(int delta);
    void promptSavePreset();
    float param(const char* id) const;

    RDSBAudioProcessor& audioProcessor;
    RDSBLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 350 };

    // Header
    Choice modeCombo, syncCombo;
    juce::TextButton triggerButton;
    Knob barsKnob;

    // Tonal
    Knob tonalVol, pitchStart, pitchEnd, unison, detune, width, pumpDepth, pumpStart, pumpEnd;
    Toggle arpEnable;
    Choice arpRate, arpPattern;
    // Noise
    Knob noiseVol, cutStart, cutEnd, noiseQ;
    Choice noiseType, noiseFilterMode;
    // Boom
    Knob boomVol, punchDecay, rumbleDecay, subStart, subEnd;
    Choice boomTrigger, boomPosition;
    // FX
    Toggle phaserEnable, filterEnable;
    Knob phaserMix, phaserFeedback, filterRes;
    Choice filterMode;
    // Space / global
    Knob revMix, revSize, revWidth, warp, drive, master;

    std::vector<Section> sections;
    std::vector<std::vector<size_t>> rows;   // section indices per row

    juce::MidiKeyboardComponent keyboard;

    juce::ComboBox presetBox;
    juce::TextButton prevPresetButton, nextPresetButton, savePresetButton;
    juce::Array<juce::File> userPresetFiles;

    juce::Rectangle<int> visualizerArea;
    float uiScale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RDSBAudioProcessorEditor)
};
