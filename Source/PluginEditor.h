#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Custom LookAndFeel for premium flat knobs with glowing value tracks
class CustomLookAndFeel : public juce::LookAndFeel_V4 {
public:
    CustomLookAndFeel() {
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xFF2C323C));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF00D2FF));
        setColour(juce::Label::textColourId, juce::Colours::white);
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF21252D));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF00D2FF));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto outline = slider.findColour(juce::Slider::rotarySliderOutlineColourId);
        auto fill    = slider.findColour(juce::Slider::rotarySliderFillColourId);

        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        auto radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toX = bounds.getCentreX();
        auto toY = bounds.getCentreY();
        auto lineW = 3.5f;
        auto arcRadius = radius - lineW * 1.5f;

        // Background Track
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(toX, toY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(outline);
        g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value Arc
        if (slider.isEnabled()) {
            juce::Path valueArc;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
            valueArc.addCentredArc(toX, toY, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour(fill);
            g.strokePath(valueArc, juce::PathStrokeType(lineW + 1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            // Pointer Dot
            float dotX = toX + arcRadius * std::sin(angle);
            float dotY = toY - arcRadius * std::cos(angle);
            g.setColour(juce::Colours::white);
            g.fillEllipse(dotX - 2.0f, dotY - 2.0f, 4.0f, 4.0f);
        }
    }
};

class TransitionEngineAudioProcessorEditor : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    TransitionEngineAudioProcessorEditor(TransitionEngineAudioProcessor&);
    ~TransitionEngineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void drawVisualizer(juce::Graphics& g, juce::Rectangle<int> area);

    TransitionEngineAudioProcessor& audioProcessor;
    CustomLookAndFeel customLookAndFeel;

    // Header Controls
    juce::ComboBox modeCombo;
    juce::ComboBox syncCombo;
    juce::Slider durationSlider;
    juce::TextButton triggerButton;

    // Tonal Sweeper Sliders
    juce::Slider tonalVolSlider;
    juce::Slider tonalPitchStartSlider;
    juce::Slider tonalPitchEndSlider;
    juce::Slider tonalUnisonSlider;
    juce::Slider tonalDetuneSlider;
    juce::Slider tonalPanSpreadSlider;
    juce::Slider pumperDepthSlider;
    juce::Slider pumperRateStartSlider;
    juce::Slider pumperRateEndSlider;

    // Arpeggiator Controls
    juce::ToggleButton arpEnableButton;
    juce::ComboBox arpRateCombo;
    juce::ComboBox arpPatternCombo;

    // Noise Engine Sliders/Combos
    juce::Slider noiseVolSlider;
    juce::ComboBox noiseTypeCombo;
    juce::ComboBox noiseFilterModeCombo;
    juce::Slider noiseCutoffStartSlider;
    juce::Slider noiseCutoffEndSlider;
    juce::Slider noiseFilterQSlider;

    // Boom Engine Sliders/Combos
    juce::Slider boomVolSlider;
    juce::ComboBox boomTriggerCombo;
    juce::Slider boomNoiseDecaySlider;
    juce::Slider boomSubDecaySlider;
    juce::Slider boomSubPitchStartSlider;
    juce::Slider boomSubPitchEndSlider;

    // FX / Master Sliders
    juce::Slider saturationDriveSlider;
    juce::Slider reverbMixSlider;
    juce::Slider reverbSizeSlider;
    juce::Slider reverbWidthSlider;
    juce::Slider warpKSlider;
    juce::Slider masterVolSlider;
    juce::MidiKeyboardComponent keyboardComponent;

    // Phaser Controls
    juce::ToggleButton phaserEnableButton;
    juce::Slider phaserFeedbackSlider;
    juce::Slider phaserMixSlider;

    // Ladder Filter Controls
    juce::ToggleButton filterEnableButton;
    juce::ComboBox filterModeCombo;
    juce::Slider filterResonanceSlider;

    // Group Boxes
    juce::GroupComponent tonalGroup;
    juce::GroupComponent noiseGroup;
    juce::GroupComponent boomGroup;
    juce::GroupComponent masterGroup;
    juce::GroupComponent fxGroup;
    juce::GroupComponent arpGroup;

    // APVTS Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboBoxAttachment> modeAttach;
    std::unique_ptr<ComboBoxAttachment> syncAttach;
    std::unique_ptr<SliderAttachment> durationAttach;

    std::unique_ptr<SliderAttachment> tonalVolAttach;
    std::unique_ptr<SliderAttachment> tonalPitchStartAttach;
    std::unique_ptr<SliderAttachment> tonalPitchEndAttach;
    std::unique_ptr<SliderAttachment> tonalUnisonAttach;
    std::unique_ptr<SliderAttachment> tonalDetuneAttach;
    std::unique_ptr<SliderAttachment> tonalPanSpreadAttach;
    std::unique_ptr<SliderAttachment> pumperDepthAttach;
    std::unique_ptr<SliderAttachment> pumperRateStartAttach;
    std::unique_ptr<SliderAttachment> pumperRateEndAttach;

    // Arpeggiator Attachments
    std::unique_ptr<ButtonAttachment> arpEnableAttach;
    std::unique_ptr<ComboBoxAttachment> arpRateAttach;
    std::unique_ptr<ComboBoxAttachment> arpPatternAttach;

    std::unique_ptr<SliderAttachment> noiseVolAttach;
    std::unique_ptr<ComboBoxAttachment> noiseTypeAttach;
    std::unique_ptr<ComboBoxAttachment> noiseFilterModeAttach;
    std::unique_ptr<SliderAttachment> noiseCutoffStartAttach;
    std::unique_ptr<SliderAttachment> noiseCutoffEndAttach;
    std::unique_ptr<SliderAttachment> noiseFilterQAttach;

    std::unique_ptr<SliderAttachment> boomVolAttach;
    std::unique_ptr<ComboBoxAttachment> boomTriggerAttach;
    std::unique_ptr<SliderAttachment> boomNoiseDecayAttach;
    std::unique_ptr<SliderAttachment> boomSubDecayAttach;
    std::unique_ptr<SliderAttachment> boomSubPitchStartAttach;
    std::unique_ptr<SliderAttachment> boomSubPitchEndAttach;

    std::unique_ptr<SliderAttachment> saturationDriveAttach;
    std::unique_ptr<SliderAttachment> reverbMixAttach;
    std::unique_ptr<SliderAttachment> reverbSizeAttach;
    std::unique_ptr<SliderAttachment> reverbWidthAttach;
    std::unique_ptr<SliderAttachment> warpKAttach;
    std::unique_ptr<SliderAttachment> masterVolAttach;

    // Phaser Attachments
    std::unique_ptr<ButtonAttachment> phaserEnableAttach;
    std::unique_ptr<SliderAttachment> phaserFeedbackAttach;
    std::unique_ptr<SliderAttachment> phaserMixAttach;

    // Ladder Filter Attachments
    std::unique_ptr<ButtonAttachment> filterEnableAttach;
    std::unique_ptr<ComboBoxAttachment> filterModeAttach;
    std::unique_ptr<SliderAttachment> filterResonanceAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransitionEngineAudioProcessorEditor)
};
