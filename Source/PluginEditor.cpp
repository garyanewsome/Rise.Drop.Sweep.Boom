#include "PluginProcessor.h"
#include "PluginEditor.h"

TransitionEngineAudioProcessorEditor::TransitionEngineAudioProcessorEditor(TransitionEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      keyboardComponent(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    // Helper to initialize rotary sliders
    auto initKnob = [this](juce::Slider& s, const juce::String& suffix) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 15);
        s.setTextValueSuffix(suffix);
        s.setLookAndFeel(&customLookAndFeel);
        addAndMakeVisible(s);
    };

    // Helper to initialize ComboBoxes
    auto initCombo = [this](juce::ComboBox& c, const juce::StringArray& items) {
        c.setLookAndFeel(&customLookAndFeel);
        c.addItemList(items, 1);
        addAndMakeVisible(c);
    };

    // Helper to initialize ToggleButtons
    auto initToggle = [this](juce::ToggleButton& b, const juce::String& text) {
        b.setButtonText(text);
        b.setLookAndFeel(&customLookAndFeel);
        addAndMakeVisible(b);
    };

    // Global / Header Controls
    initCombo(modeCombo, juce::StringArray("Riser", "Downlifter"));
    modeAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "mode", modeCombo);

    initCombo(syncCombo, juce::StringArray("Transport Synced", "MIDI Triggered"));
    syncAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "sync_mode", syncCombo);

    // Bar length (Linear horizontal slider in header)
    durationSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    durationSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 15);
    durationSlider.setLookAndFeel(&customLookAndFeel);
    addAndMakeVisible(durationSlider);
    durationAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "duration_bars", durationSlider);

    triggerButton.setButtonText("TRIGGER");
    triggerButton.setLookAndFeel(&customLookAndFeel);
    triggerButton.onClick = [this] { audioProcessor.triggerTransition(); };
    addAndMakeVisible(triggerButton);

    // Tonal Sweeper Sliders
    initKnob(tonalVolSlider, "");
    tonalVolAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "tonal_volume", tonalVolSlider);

    initKnob(tonalPitchStartSlider, "");
    tonalPitchStartAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "tonal_pitch_start", tonalPitchStartSlider);

    initKnob(tonalPitchEndSlider, "");
    tonalPitchEndAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "tonal_pitch_end", tonalPitchEndSlider);

    initKnob(tonalUnisonSlider, "");
    tonalUnisonAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "tonal_unison", tonalUnisonSlider);

    initKnob(tonalDetuneSlider, "");
    tonalDetuneAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "tonal_detune", tonalDetuneSlider);

    initKnob(tonalPanSpreadSlider, "");
    tonalPanSpreadAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "tonal_pan_spread", tonalPanSpreadSlider);

    initKnob(pumperDepthSlider, "");
    pumperDepthAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pumper_depth", pumperDepthSlider);

    initKnob(pumperRateStartSlider, " Hz");
    pumperRateStartAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pumper_rate_start", pumperRateStartSlider);

    initKnob(pumperRateEndSlider, " Hz");
    pumperRateEndAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pumper_rate_end", pumperRateEndSlider);

    // Noise Engine
    initKnob(noiseVolSlider, "");
    noiseVolAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "noise_volume", noiseVolSlider);

    initCombo(noiseTypeCombo, juce::StringArray("White Noise", "Pink Noise"));
    noiseTypeAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "noise_type", noiseTypeCombo);

    initCombo(noiseFilterModeCombo, juce::StringArray("Bandpass", "Highpass"));
    noiseFilterModeAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "noise_filter_mode", noiseFilterModeCombo);

    initKnob(noiseCutoffStartSlider, " Hz");
    noiseCutoffStartAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "noise_cutoff_start", noiseCutoffStartSlider);

    initKnob(noiseCutoffEndSlider, " Hz");
    noiseCutoffEndAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "noise_cutoff_end", noiseCutoffEndSlider);

    initKnob(noiseFilterQSlider, "");
    noiseFilterQAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "noise_filter_q", noiseFilterQSlider);

    // Boom Engine
    initKnob(boomVolSlider, "");
    boomVolAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "boom_volume", boomVolSlider);

    initCombo(boomTriggerCombo, juce::StringArray("Auto (Drop)", "MIDI Key"));
    boomTriggerAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "boom_trigger_mode", boomTriggerCombo);

    initKnob(boomNoiseDecaySlider, "s");
    boomNoiseDecayAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "boom_noise_decay", boomNoiseDecaySlider);

    initKnob(boomSubDecaySlider, "s");
    boomSubDecayAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "boom_sub_decay", boomSubDecaySlider);

    initKnob(boomSubPitchStartSlider, " Hz");
    boomSubPitchStartAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "boom_sub_pitch_start", boomSubPitchStartSlider);

    initKnob(boomSubPitchEndSlider, " Hz");
    boomSubPitchEndAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "boom_sub_pitch_end", boomSubPitchEndSlider);

    // FX / Master
    initKnob(saturationDriveSlider, "x");
    saturationDriveAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "saturation_drive", saturationDriveSlider);

    initKnob(reverbMixSlider, "");
    reverbMixAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "reverb_mix", reverbMixSlider);

    initKnob(reverbSizeSlider, "");
    reverbSizeAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "reverb_size", reverbSizeSlider);

    initKnob(reverbWidthSlider, "");
    reverbWidthAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "reverb_width", reverbWidthSlider);

    initKnob(warpKSlider, "");
    warpKAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "warp_k", warpKSlider);

    initKnob(masterVolSlider, " dB");
    masterVolAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "master_volume", masterVolSlider);

    // Group Boxes
    tonalGroup.setText("TONAL SWEEPER");
    addAndMakeVisible(tonalGroup);

    noiseGroup.setText("NOISE ENGINE");
    addAndMakeVisible(noiseGroup);

    boomGroup.setText("BOOM ENGINE");
    addAndMakeVisible(boomGroup);

    masterGroup.setText("MASTER & FX");
    addAndMakeVisible(masterGroup);

    // Phaser Controls
    initToggle(phaserEnableButton, "Phaser Enable");
    phaserEnableAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "phaser_enable", phaserEnableButton);

    initKnob(phaserFeedbackSlider, "");
    phaserFeedbackAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "phaser_feedback", phaserFeedbackSlider);

    initKnob(phaserMixSlider, "");
    phaserMixAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "phaser_mix", phaserMixSlider);

    // Ladder Filter Controls
    initToggle(filterEnableButton, "Filter Enable");
    filterEnableAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "filter_enable", filterEnableButton);

    initCombo(filterModeCombo, juce::StringArray("Lowpass", "Highpass"));
    filterModeAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "filter_mode", filterModeCombo);

    initKnob(filterResonanceSlider, "");
    filterResonanceAttach = std::make_unique<SliderAttachment>(audioProcessor.apvts, "filter_resonance", filterResonanceSlider);

    fxGroup.setText("FX ENGINES");
    addAndMakeVisible(fxGroup);

    // Arpeggiator Controls & Layout
    initToggle(arpEnableButton, "Enable Arpeggiator");
    arpEnableAttach = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "arp_enable", arpEnableButton);

    initCombo(arpRateCombo, juce::StringArray("1/8", "1/16", "1/32"));
    arpRateAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "arp_rate", arpRateCombo);

    initCombo(arpPatternCombo, juce::StringArray("Up", "Down", "Up-Down", "Octaves"));
    arpPatternAttach = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, "arp_pattern", arpPatternCombo);

    arpGroup.setText("ARPEGGIATOR");
    addAndMakeVisible(arpGroup);

    keyboardComponent.setLookAndFeel(&customLookAndFeel);
    addAndMakeVisible(keyboardComponent);

    setSize(1150, 725);

    // Start UI updates (60 FPS / 16ms refresh for smooth playhead)
    startTimerHz(60);
}

TransitionEngineAudioProcessorEditor::~TransitionEngineAudioProcessorEditor()
{
    // Clean up look and feel pointers
    durationSlider.setLookAndFeel(nullptr);
    triggerButton.setLookAndFeel(nullptr);

    tonalVolSlider.setLookAndFeel(nullptr);
    tonalPitchStartSlider.setLookAndFeel(nullptr);
    tonalPitchEndSlider.setLookAndFeel(nullptr);
    tonalUnisonSlider.setLookAndFeel(nullptr);
    tonalDetuneSlider.setLookAndFeel(nullptr);
    tonalPanSpreadSlider.setLookAndFeel(nullptr);
    pumperDepthSlider.setLookAndFeel(nullptr);
    pumperRateStartSlider.setLookAndFeel(nullptr);
    pumperRateEndSlider.setLookAndFeel(nullptr);

    noiseVolSlider.setLookAndFeel(nullptr);
    noiseTypeCombo.setLookAndFeel(nullptr);
    noiseFilterModeCombo.setLookAndFeel(nullptr);
    noiseCutoffStartSlider.setLookAndFeel(nullptr);
    noiseCutoffEndSlider.setLookAndFeel(nullptr);
    noiseFilterQSlider.setLookAndFeel(nullptr);

    boomVolSlider.setLookAndFeel(nullptr);
    boomTriggerCombo.setLookAndFeel(nullptr);
    boomNoiseDecaySlider.setLookAndFeel(nullptr);
    boomSubDecaySlider.setLookAndFeel(nullptr);
    boomSubPitchStartSlider.setLookAndFeel(nullptr);
    boomSubPitchEndSlider.setLookAndFeel(nullptr);

    saturationDriveSlider.setLookAndFeel(nullptr);
    reverbMixSlider.setLookAndFeel(nullptr);
    reverbSizeSlider.setLookAndFeel(nullptr);
    reverbWidthSlider.setLookAndFeel(nullptr);
    warpKSlider.setLookAndFeel(nullptr);
    masterVolSlider.setLookAndFeel(nullptr);
    keyboardComponent.setLookAndFeel(nullptr);

    phaserFeedbackSlider.setLookAndFeel(nullptr);
    phaserMixSlider.setLookAndFeel(nullptr);
    filterResonanceSlider.setLookAndFeel(nullptr);
    phaserEnableButton.setLookAndFeel(nullptr);
    filterEnableButton.setLookAndFeel(nullptr);
    filterModeCombo.setLookAndFeel(nullptr);

    arpEnableButton.setLookAndFeel(nullptr);
    arpRateCombo.setLookAndFeel(nullptr);
    arpPatternCombo.setLookAndFeel(nullptr);

    modeCombo.setLookAndFeel(nullptr);
    syncCombo.setLookAndFeel(nullptr);
}

void TransitionEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Sleek premium dark slate background
    g.fillAll(juce::Colour(0xFF101216));

    // Dynamic color theme dependent on Mode
    auto modeVal = audioProcessor.apvts.getRawParameterValue("mode")->load();
    juce::Colour accentColour = (modeVal == 1.0f) 
        ? juce::Colour(0xFFFF007A)  // Neon Magenta for Downlifters
        : juce::Colour(0xFF00D2FF); // Neon Cyan for Risers

    customLookAndFeel.setColour(juce::Slider::rotarySliderFillColourId, accentColour);
    customLookAndFeel.setColour(juce::TextButton::buttonOnColourId, accentColour);
    customLookAndFeel.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, accentColour.withAlpha(0.5f));

    // Title text
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font("Outfit", 20.0f, juce::Font::bold));
    g.drawText("RISE.DROP.SWEEP.BOOM", 20, 15, 300, 30, juce::Justification::left);

    // Header labels
    g.setColour(juce::Colours::grey);
    g.setFont(juce::Font("Inter", 11.0f, juce::Font::plain));
    g.drawText("BARS:", 945, 23, 40, 15, juce::Justification::right);

    // Draw the real-time Warp Visualizer
    juce::Rectangle<int> visArea(20, 80, 1110, 180);
    drawVisualizer(g, visArea);

    // Label coordinates inside Groups
    g.setFont(10.0f);
    g.setColour(juce::Colours::lightgrey);

    // 1. Tonal Sweeper Labels
    int tx = tonalGroup.getX();
    int ty = tonalGroup.getY();
    g.drawText("VOLUME", tx + 15, ty + 85, 60, 12, juce::Justification::centred);
    g.drawText("START P", tx + 75, ty + 85, 55, 12, juce::Justification::centred);
    g.drawText("END P", tx + 145, ty + 85, 55, 12, juce::Justification::centred);
    
    g.drawText("VOICES", tx + 15, ty + 160, 55, 12, juce::Justification::centred);
    g.drawText("DETUNE", tx + 75, ty + 160, 55, 12, juce::Justification::centred);
    g.drawText("WIDTH", tx + 135, ty + 160, 55, 12, juce::Justification::centred);

    g.drawText("PUMP D", tx + 15, ty + 240, 55, 12, juce::Justification::centred);
    g.drawText("RATE S", tx + 75, ty + 240, 55, 12, juce::Justification::centred);
    g.drawText("RATE E", tx + 135, ty + 240, 55, 12, juce::Justification::centred);

    // 2. Noise Engine Labels
    int nx = noiseGroup.getX();
    int ny = noiseGroup.getY();
    g.drawText("VOLUME", nx + 15, ny + 85, 60, 12, juce::Justification::centred);
    g.drawText("TYPE", nx + 85, ny + 15, 110, 12, juce::Justification::left);
    g.drawText("FILTER", nx + 85, ny + 50, 110, 12, juce::Justification::left);

    g.drawText("CUTOFF S", nx + 15, ty + 200, 55, 12, juce::Justification::centred);
    g.drawText("CUTOFF E", nx + 75, ty + 200, 55, 12, juce::Justification::centred);
    g.drawText("FILTER Q", nx + 135, ty + 200, 55, 12, juce::Justification::centred);

    // 3. Boom Engine Labels
    int bx = boomGroup.getX();
    int by = boomGroup.getY();
    g.drawText("VOLUME", bx + 15, by + 85, 60, 12, juce::Justification::centred);
    g.drawText("TRIGGER", bx + 85, by + 15, 110, 12, juce::Justification::left);

    g.drawText("PUNCH DEC", bx + 15, ty + 175, 55, 12, juce::Justification::centred);
    g.drawText("RUMBLE DEC", bx + 75, ty + 175, 55, 12, juce::Justification::centred);
    g.drawText("SUB START", bx + 15, ty + 255, 55, 12, juce::Justification::centred);
    g.drawText("SUB END", bx + 75, ty + 255, 55, 12, juce::Justification::centred);

    // 4. Master / FX Labels
    int mx = masterGroup.getX();
    int my = masterGroup.getY();
    g.drawText("DRIVE", mx + 15, my + 90, 55, 12, juce::Justification::centred);
    g.drawText("REV MIX", mx + 75, my + 90, 55, 12, juce::Justification::centred);
    g.drawText("REV SIZE", mx + 135, my + 90, 55, 12, juce::Justification::centred);
    g.drawText("REV WIDTH", mx + 75, my + 165, 55, 12, juce::Justification::centred);

    g.drawText("WARP (k)", mx + 15, my + 270, 80, 12, juce::Justification::centred);
    g.drawText("MASTER", mx + 115, my + 270, 80, 12, juce::Justification::centred);

    // 5. FX Engines Labels
    int fx = fxGroup.getX();
    int fy = fxGroup.getY();
    g.drawText("MIX", fx + 40, fy + 120, 55, 12, juce::Justification::centred);
    g.drawText("FEEDBACK", fx + 115, fy + 120, 55, 12, juce::Justification::centred);
    g.drawText("RESONANCE", fx + 77, fy + 267, 55, 12, juce::Justification::centred);

    // 6. Arpeggiator Labels
    int ax = arpGroup.getX();
    int ay = arpGroup.getY();
    g.setFont(11.0f);
    g.setColour(juce::Colours::lightgrey);
    g.drawText("RATE:", ax + 380, ay + 25, 60, 22, juce::Justification::right | juce::Justification::verticallyCentred);
    g.drawText("PATTERN:", ax + 720, ay + 25, 80, 22, juce::Justification::right | juce::Justification::verticallyCentred);
}

void TransitionEngineAudioProcessorEditor::resized()
{
    // Header Bounds
    modeCombo.setBounds(280, 18, 120, 24);
    syncCombo.setBounds(415, 18, 130, 24);
    durationSlider.setBounds(990, 18, 140, 24);
    triggerButton.setBounds(560, 18, 90, 24);

    // Bottom Groups
    tonalGroup.setBounds(20, 280, 210, 300);
    noiseGroup.setBounds(243, 280, 210, 300);
    boomGroup.setBounds(466, 280, 210, 300);
    masterGroup.setBounds(689, 280, 210, 300);
    fxGroup.setBounds(912, 280, 210, 300);

    // 1. Tonal Sweeper Layout
    int tx = tonalGroup.getX();
    int ty = tonalGroup.getY();
    tonalVolSlider.setBounds(tx + 15, ty + 25, 60, 60);
    tonalPitchStartSlider.setBounds(tx + 75, ty + 25, 55, 60);
    tonalPitchEndSlider.setBounds(tx + 145, ty + 25, 55, 60);

    tonalUnisonSlider.setBounds(tx + 15, ty + 100, 55, 60);
    tonalDetuneSlider.setBounds(tx + 75, ty + 100, 55, 60);
    tonalPanSpreadSlider.setBounds(tx + 135, ty + 100, 55, 60);

    pumperDepthSlider.setBounds(tx + 15, ty + 180, 55, 60);
    pumperRateStartSlider.setBounds(tx + 75, ty + 180, 55, 60);
    pumperRateEndSlider.setBounds(tx + 135, ty + 180, 55, 60);

    // 2. Noise Engine Layout
    int nx = noiseGroup.getX();
    int ny = noiseGroup.getY();
    noiseVolSlider.setBounds(nx + 15, ny + 25, 60, 60);
    noiseTypeCombo.setBounds(nx + 85, ny + 27, 110, 22);
    noiseFilterModeCombo.setBounds(nx + 85, ny + 62, 110, 22);

    noiseCutoffStartSlider.setBounds(nx + 15, ny + 140, 55, 60);
    noiseCutoffEndSlider.setBounds(nx + 75, ny + 140, 55, 60);
    noiseFilterQSlider.setBounds(nx + 135, ny + 140, 55, 60);

    // 3. Boom Engine Layout
    int bx = boomGroup.getX();
    int by = boomGroup.getY();
    boomVolSlider.setBounds(bx + 15, by + 25, 60, 60);
    boomTriggerCombo.setBounds(bx + 85, by + 27, 110, 22);

    boomNoiseDecaySlider.setBounds(bx + 15, by + 115, 55, 60);
    boomSubDecaySlider.setBounds(bx + 75, by + 115, 55, 60);
    boomSubPitchStartSlider.setBounds(bx + 15, by + 195, 55, 60);
    boomSubPitchEndSlider.setBounds(bx + 75, by + 195, 55, 60);

    // 4. Master & FX Layout
    int mx = masterGroup.getX();
    int my = masterGroup.getY();
    saturationDriveSlider.setBounds(mx + 15, my + 30, 55, 60);
    reverbMixSlider.setBounds(mx + 75, my + 30, 55, 60);
    reverbSizeSlider.setBounds(mx + 135, my + 30, 55, 60);
    reverbWidthSlider.setBounds(mx + 75, my + 105, 55, 60);

    warpKSlider.setBounds(mx + 15, my + 185, 80, 85);
    masterVolSlider.setBounds(mx + 115, my + 185, 80, 85);

    // 5. FX Engines Layout
    int fx = fxGroup.getX();
    int fy = fxGroup.getY();
    phaserEnableButton.setBounds(fx + 15, fy + 27, 180, 22);
    phaserMixSlider.setBounds(fx + 40, fy + 60, 55, 60);
    phaserFeedbackSlider.setBounds(fx + 115, fy + 60, 55, 60);

    filterEnableButton.setBounds(fx + 15, fy + 137, 180, 22);
    filterModeCombo.setBounds(fx + 15, fy + 172, 180, 22);
    filterResonanceSlider.setBounds(fx + 77, fy + 207, 55, 60);

    // 6. Arpeggiator Group Layout
    arpGroup.setBounds(20, 590, 1110, 60);
    int ax = arpGroup.getX();
    int ay = arpGroup.getY();
    arpEnableButton.setBounds(ax + 50, ay + 25, 180, 22);
    arpRateCombo.setBounds(ax + 450, ay + 25, 150, 22);
    arpPatternCombo.setBounds(ax + 810, ay + 25, 180, 22);

    // On-Screen Keyboard layout
    keyboardComponent.setBounds(20, 660, 1110, 50);
}

void TransitionEngineAudioProcessorEditor::timerCallback()
{
    // Repaint to update the real-time visualizer playhead position
    repaint();
}

void TransitionEngineAudioProcessorEditor::drawVisualizer(juce::Graphics& g, juce::Rectangle<int> area)
{
    auto modeVal = audioProcessor.apvts.getRawParameterValue("mode")->load();
    bool isDownlifter = (modeVal == 1.0f);
    
    juce::Colour accentColour = isDownlifter 
        ? juce::Colour(0xFFFF007A)  // Neon Magenta
        : juce::Colour(0xFF00D2FF); // Neon Blue

    // Draw visualizer background
    g.setColour(juce::Colour(0xFF13171E));
    g.fillRect(area);

    // Draw subtle glowing frame border
    g.setColour(accentColour.withAlpha(0.2f));
    g.drawRect(area, 2.0f);

    // Draw grid
    g.setColour(juce::Colour(0xFF2C323C).withAlpha(0.3f));
    int gridCount = 8;
    for (int i = 1; i < gridCount; ++i)
    {
        float ratio = static_cast<float>(i) / gridCount;
        g.drawVerticalLine(area.getX() + ratio * area.getWidth(), area.getY(), area.getBottom());
        g.drawHorizontalLine(area.getY() + ratio * area.getHeight(), area.getX(), area.getRight());
    }

    // Get parameters
    float k = audioProcessor.apvts.getRawParameterValue("warp_k")->load();
    float phase = audioProcessor.getMasterPhase();

    // Plot warp curve path
    juce::Path p;
    for (int x = 0; x <= area.getWidth(); ++x)
    {
        float normX = static_cast<float>(x) / area.getWidth();
        // Calculate curve mapping
        float activePhase = isDownlifter ? (1.0f - normX) : normX;
        float warped = std::pow(activePhase, k);
        float y = area.getBottom() - warped * area.getHeight();

        if (x == 0)
            p.startNewSubPath(area.getX() + x, y);
        else
            p.lineTo(area.getX() + x, y);
    }

    // Glow effect - draw thick translucent curve underneath
    g.setColour(accentColour.withAlpha(0.15f));
    g.strokePath(p, juce::PathStrokeType(6.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

    // Sharp curve line
    g.setColour(accentColour);
    g.strokePath(p, juce::PathStrokeType(2.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

    // Fill underneath the curve with gradient
    juce::Path fillPath = p;
    fillPath.lineTo(area.getRight(), area.getBottom());
    fillPath.lineTo(area.getX(), area.getBottom());
    fillPath.closeSubPath();

    juce::ColourGradient grad(accentColour.withAlpha(0.18f), area.getX(), area.getY(),
                              accentColour.withAlpha(0.0f), area.getX(), area.getBottom(), false);
    g.setGradientFill(grad);
    g.fillPath(fillPath);

    // Draw real-time playhead if transition is playing or if standing by
    float playheadX = area.getX() + phase * area.getWidth();
    
    // Draw vertical playhead line
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawVerticalLine(static_cast<int>(playheadX), area.getY(), area.getBottom());

    // Draw glowing playhead dot on the curve
    float activePlayheadPhase = isDownlifter ? (1.0f - phase) : phase;
    float playheadWarped = std::pow(activePlayheadPhase, k);
    float playheadY = area.getBottom() - playheadWarped * area.getHeight();

    // Glow circle
    g.setColour(accentColour.withAlpha(0.4f));
    g.fillEllipse(playheadX - 6.0f, playheadY - 6.0f, 12.0f, 12.0f);

    // Inner white point
    g.setColour(juce::Colours::white);
    g.fillEllipse(playheadX - 3.0f, playheadY - 3.0f, 6.0f, 6.0f);

    // Display Text parameters on the visualizer overlay
    g.setColour(juce::Colours::grey.withAlpha(0.7f));
    g.setFont(juce::Font("Inter", 10.0f, juce::Font::bold));
    g.drawText("BPM: " + juce::String(audioProcessor.isPlaying() ? "SYNCED" : "MANUAL"), area.getX() + 10, area.getY() + 10, 100, 15, juce::Justification::left);
    g.drawText("TRIGGER KEY: " + juce::MidiMessage::getMidiNoteName(audioProcessor.getTriggerMidiNote(), true, true, 3) + " (" + juce::String(audioProcessor.getTriggerMidiNote()) + ")", area.getX() + 120, area.getY() + 10, 200, 15, juce::Justification::left);
    g.drawText("WARP COEFF: " + juce::String(k, 2), area.getX() + 10, area.getBottom() - 20, 150, 15, juce::Justification::left);
    g.drawText("PHASE: " + juce::String(phase * 100.0f, 1) + "%", area.getRight() - 110, area.getBottom() - 20, 100, 15, juce::Justification::right);
}
