#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace {
constexpr int kBaseWidth = 1280;
constexpr int kBaseHeight = 700;
constexpr int kHeaderHeight = 66;
constexpr int kVisualizerHeight = 190;
constexpr int kRow1Height = 214;
constexpr int kRow2Height = 198;

using LAF = RDSBLookAndFeel;

double leadingNumber(const juce::String& t) { return t.retainCharacters("+-.0123456789").getDoubleValue(); }
juce::String percent(double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; }
double percentFrom(const juce::String& t) { return leadingNumber(t) / 100.0; }
juce::String hz(double v) {
    return v < 1000.0 ? juce::String(juce::roundToInt(v)) + " Hz" : juce::String(v / 1000.0, 1) + " kHz";
}
double hzFrom(const juce::String& t) {
    const double n = leadingNumber(t);
    return t.containsIgnoreCase("k") ? n * 1000.0 : n;
}
juce::String noteName(double midi) { return juce::MidiMessage::getMidiNoteName(juce::roundToInt(midi), true, true, 4); }
juce::String seconds(double v) { return juce::String(v, 2) + " s"; }
} // namespace

//==============================================================================
RDSBAudioProcessorEditor::RDSBAudioProcessorEditor(RDSBAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      keyboard(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel(&lookAndFeel);

    const auto violet = LAF::violet, cyan = LAF::cyan, orange = LAF::orange;
    const auto lime = LAF::lime, pink = LAF::pink, white = LAF::white;

    // ---- Header
    addChoice(modeCombo, "mode", juce::StringArray("RISER", "DOWNLIFTER"),
              "Riser builds up to the drop. Downlifter drops first, then falls away.", white);
    addChoice(syncCombo, "sync_mode", juce::StringArray("TRANSPORT", "MIDI"),
              "TRANSPORT locks the sweep to the DAW's bar grid and loops while the DAW plays "
              "(when it is stopped, a key or TRIGGER previews it once). MIDI plays it once "
              "from each note you play.", white);
    triggerButton.setButtonText("TRIGGER");
    triggerButton.setTooltip("Start a transition now (MIDI mode). Same as playing a key.");
    triggerButton.setColour(juce::TextButton::textColourOffId, white);
    triggerButton.onClick = [this] { audioProcessor.triggerTransition(); };
    addAndMakeVisible(triggerButton);

    addKnob(barsKnob, "duration_bars", "BARS",
            "Length of the transition in bars. The sweep takes exactly this long, then drops.", white);
    barsKnob.slider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " bars"; };

    // ---- Tonal sweeper
    addKnob(tonalVol, "tonal_volume", "LEVEL", "How loud the pitched saw riser is.", violet);
    tonalVol.slider.textFromValueFunction = percent; tonalVol.slider.valueFromTextFunction = percentFrom;
    addKnob(pitchStart, "tonal_pitch_start", "START",
            "Pitch the riser begins on. The note you play transposes it.", violet);
    pitchStart.slider.textFromValueFunction = noteName;
    addKnob(pitchEnd, "tonal_pitch_end", "END", "Pitch the riser reaches at the drop.", violet);
    pitchEnd.slider.textFromValueFunction = noteName;
    addKnob(unison, "tonal_unison", "VOICES",
            "Number of stacked, slightly detuned saws. More is wider and thicker.", violet);
    addKnob(detune, "tonal_detune", "DETUNE", "How far apart the stacked saws are tuned.", violet);
    detune.slider.textFromValueFunction = percent; detune.slider.valueFromTextFunction = percentFrom;
    addKnob(width, "tonal_pan_spread", "SPREAD", "How widely the stacked saws spread across the stereo field.", violet);
    width.slider.textFromValueFunction = percent; width.slider.valueFromTextFunction = percentFrom;
    addKnob(pumpDepth, "pumper_depth", "PUMP",
            "Sidechain-style pumping: how deeply the riser ducks on each pulse. Zero is off.", violet);
    pumpDepth.slider.textFromValueFunction = percent; pumpDepth.slider.valueFromTextFunction = percentFrom;
    addKnob(pumpStart, "pumper_rate_start", "FROM",
            "Pump speed at the start of the sweep, in pulses per beat (1 = quarter notes, 4 = 16ths).", violet);
    pumpStart.slider.textFromValueFunction = [](double v) { return juce::String(v, 2) + "/beat"; };
    addKnob(pumpEnd, "pumper_rate_end", "TO",
            "Pump speed at the end of the sweep. Higher than FROM makes the pulses speed up.", violet);
    pumpEnd.slider.textFromValueFunction = [](double v) { return juce::String(v, 2) + "/beat"; };

    addToggle(arpEnable, "arp_enable", "ARP",
              "Plays the riser as a rising arpeggio (root, third, fifth, octave) instead of a smooth glide.", violet);
    addChoice(arpRate, "arp_rate", juce::StringArray("1/8", "1/16", "1/32"),
              "How fast the arpeggio steps.", violet);
    addChoice(arpPattern, "arp_pattern", juce::StringArray("UP", "DOWN", "UP-DOWN", "OCTAVES"),
              "The order the arpeggio plays its notes in.", violet);

    // ---- Noise
    addKnob(noiseVol, "noise_volume", "LEVEL", "How loud the noise whoosh is.", cyan);
    noiseVol.slider.textFromValueFunction = percent; noiseVol.slider.valueFromTextFunction = percentFrom;
    addChoice(noiseType, "noise_type", juce::StringArray("WHITE", "PINK"),
              "White is bright and hissy. Pink is softer and fuller.", cyan);
    addChoice(noiseFilterMode, "noise_filter_mode", juce::StringArray("BAND-PASS", "HIGH-PASS"),
              "Band-pass is a focused whoosh. High-pass opens into airy hiss.", cyan);
    addKnob(cutStart, "noise_cutoff_start", "FROM", "Where the noise filter starts its sweep.", cyan);
    cutStart.slider.textFromValueFunction = hz; cutStart.slider.valueFromTextFunction = hzFrom;
    addKnob(cutEnd, "noise_cutoff_end", "TO", "Where the noise filter ends up at the drop.", cyan);
    cutEnd.slider.textFromValueFunction = hz; cutEnd.slider.valueFromTextFunction = hzFrom;
    addKnob(noiseQ, "noise_filter_q", "FOCUS", "How narrow and whistling the noise filter is.", cyan);
    noiseQ.slider.textFromValueFunction = [](double v) { return juce::String(v, 1); };

    // ---- Boom
    addKnob(boomVol, "boom_volume", "LEVEL", "How loud the impact is.", orange);
    boomVol.slider.textFromValueFunction = percent; boomVol.slider.valueFromTextFunction = percentFrom;
    addChoice(boomTrigger, "boom_trigger_mode", juce::StringArray("AUTO DROP", "MIDI KEY"),
              "AUTO fires the impact at the drop. MIDI KEY fires it only when you play a key.", orange);
    addChoice(boomPosition, "boom_position", juce::StringArray("AT DROP", "AT START", "AT END", "START + END"),
              "Where the impact lands. AT DROP is the classic: end of a riser, start of a downlifter. "
              "AT END lands a downlifter with a boom. START + END gives both.", orange);
    addKnob(punchDecay, "boom_noise_decay", "PUNCH", "How long the sharp noise hit at the front of the impact lasts.", orange);
    punchDecay.slider.textFromValueFunction = seconds;
    addKnob(rumbleDecay, "boom_sub_decay", "RUMBLE", "How long the deep sub tail rings out.", orange);
    rumbleDecay.slider.textFromValueFunction = seconds;
    addKnob(subStart, "boom_sub_pitch_start", "DROP FROM", "Pitch the sub starts on before it falls.", orange);
    subStart.slider.textFromValueFunction = hz; subStart.slider.valueFromTextFunction = hzFrom;
    addKnob(subEnd, "boom_sub_pitch_end", "DROP TO", "The low note the sub settles on.", orange);
    subEnd.slider.textFromValueFunction = hz; subEnd.slider.valueFromTextFunction = hzFrom;

    // ---- FX
    addToggle(phaserEnable, "phaser_enable", "PHASER", "Adds a swirling phaser that follows the sweep.", lime);
    addKnob(phaserMix, "phaser_mix", "MIX", "How much phaser is blended in.", lime);
    phaserMix.slider.textFromValueFunction = percent; phaserMix.slider.valueFromTextFunction = percentFrom;
    addKnob(phaserFeedback, "phaser_feedback", "DEPTH", "How intense and resonant the phaser is.", lime);
    phaserFeedback.slider.textFromValueFunction = percent; phaserFeedback.slider.valueFromTextFunction = percentFrom;
    addToggle(filterEnable, "filter_enable", "FILTER", "Sweeps a filter over the whole mix with the transition.", lime);
    addChoice(filterMode, "filter_mode", juce::StringArray("LOW-PASS", "HIGH-PASS"),
              "Low-pass opens up as it rises. High-pass clears out the low end as it rises.", lime);
    addKnob(filterRes, "filter_resonance", "RESO", "How much the filter resonates around the cutoff.", lime);
    filterRes.slider.textFromValueFunction = percent; filterRes.slider.valueFromTextFunction = percentFrom;

    // ---- Space / global
    addKnob(revMix, "reverb_mix", "REVERB", "How much reverb is blended in. Great for a long tail on the drop.", pink);
    revMix.slider.textFromValueFunction = percent; revMix.slider.valueFromTextFunction = percentFrom;
    addKnob(revSize, "reverb_size", "SIZE", "How big the reverb room is.", pink);
    revSize.slider.textFromValueFunction = percent; revSize.slider.valueFromTextFunction = percentFrom;
    addKnob(revWidth, "reverb_width", "WIDTH", "How wide the reverb is across the stereo field.", pink);
    revWidth.slider.textFromValueFunction = percent; revWidth.slider.valueFromTextFunction = percentFrom;
    addKnob(warp, "warp_k", "CURVE",
            "Shape of the sweep. Low is even; higher holds back and rushes up at the end for more tension.", white);
    warp.slider.textFromValueFunction = [](double v) { return juce::String(v, 1); };
    addKnob(drive, "saturation_drive", "DRIVE", "Saturation: adds grit and presence to the whole mix.", white);
    drive.slider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " x"; };
    addKnob(master, "master_volume", "MASTER", "Overall output level in decibels.", white);
    master.slider.textFromValueFunction = [](double v) { return juce::String(v, 1) + " dB"; };

    for (auto* k : {&barsKnob, &tonalVol, &pitchStart, &pitchEnd, &unison, &detune, &width, &pumpDepth, &pumpStart,
                    &pumpEnd, &noiseVol, &cutStart, &cutEnd, &noiseQ, &boomVol, &punchDecay, &rumbleDecay,
                    &subStart, &subEnd, &phaserMix, &phaserFeedback, &filterRes, &revMix, &revSize, &revWidth,
                    &warp, &drive, &master})
        k->slider.updateText();

    // ---- Sections and rows
    sections = {
        {"TONAL SWEEP", violet, {}, {}, {&tonalVol, &pitchStart, &pitchEnd, &unison, &detune, &width, &pumpDepth, &pumpStart, &pumpEnd}, 5, 5.0f, {}},
        {"ARP",         violet, {&arpEnable}, {&arpRate, &arpPattern}, {}, 1, 1.9f, {}},
        {"NOISE",       cyan,   {}, {&noiseType, &noiseFilterMode}, {&noiseVol, &cutStart, &cutEnd, &noiseQ}, 4, 3.6f, {}},
        {"BOOM",        orange, {}, {&boomTrigger, &boomPosition}, {&boomVol, &punchDecay, &rumbleDecay, &subStart, &subEnd}, 5, 4.4f, {}},
        {"FX",          lime,   {&phaserEnable, &filterEnable}, {&filterMode}, {&phaserMix, &phaserFeedback, &filterRes}, 3, 3.0f, {}},
        {"SPACE",       pink,   {}, {}, {&revMix, &revSize, &revWidth}, 3, 2.7f, {}},
        {"SHAPE & OUT", white,  {}, {}, {&warp, &drive, &master}, 3, 2.7f, {}},
    };
    rows = { {0, 1, 2, 3}, {4, 5, 6} };

    // ---- Preset browser
    presetBox.setTextWhenNothingSelected("- PRESET -");
    presetBox.setTooltip("Choose a factory or saved preset.");
    presetBox.setColour(juce::ComboBox::outlineColourId, white.withAlpha(0.7f));
    presetBox.onChange = [this] {
        const int id = presetBox.getSelectedId();
        const int nFactory = (int) Presets::factory().size();
        if (id >= 1 && id <= nFactory) Presets::applyFactory(audioProcessor, id - 1);
        else if (id > 1000 && id - 1001 < userPresetFiles.size()) Presets::loadUser(audioProcessor, userPresetFiles[id - 1001]);
    };
    addAndMakeVisible(presetBox);

    prevPresetButton.setButtonText("<");
    nextPresetButton.setButtonText(">");
    savePresetButton.setButtonText("SAVE");
    prevPresetButton.setTooltip("Previous preset.");
    nextPresetButton.setTooltip("Next preset.");
    savePresetButton.setTooltip("Save the current sound as a preset.");
    prevPresetButton.onClick = [this] { stepPreset(-1); };
    nextPresetButton.onClick = [this] { stepPreset(1); };
    savePresetButton.onClick = [this] { promptSavePreset(); };
    addAndMakeVisible(prevPresetButton);
    addAndMakeVisible(nextPresetButton);
    addAndMakeVisible(savePresetButton);
    refreshPresetList();

    // ---- Keyboard
    keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, violet.withAlpha(0.75f));
    keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, cyan.withAlpha(0.35f));
    keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xff1a1a24));
    keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colours::black);
    keyboard.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colours::white.withAlpha(0.18f));
    keyboard.setColour(juce::MidiKeyboardComponent::textLabelColourId, LAF::dimText);
    keyboard.setAvailableRange(24, 96);
    keyboard.setScrollButtonsVisible(false);
    addAndMakeVisible(keyboard);

    setResizable(true, true);
    setResizeLimits(kBaseWidth * 3 / 4, kBaseHeight * 3 / 4, kBaseWidth * 2, kBaseHeight * 2);
    getConstrainer()->setFixedAspectRatio((double) kBaseWidth / kBaseHeight);
    setSize(kBaseWidth, kBaseHeight);
    startTimerHz(30);
}

RDSBAudioProcessorEditor::~RDSBAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

float RDSBAudioProcessorEditor::param(const char* id) const {
    return audioProcessor.apvts.getRawParameterValue(id)->load();
}

void RDSBAudioProcessorEditor::addKnob(Knob& k, const juce::String& id, const juce::String& title,
                                       const juce::String& tip, juce::Colour accent) {
    k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    k.slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    k.slider.setTooltip(tip);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, id, k.slider);
    addAndMakeVisible(k.slider);

    k.label.setText(title, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    k.label.setColour(juce::Label::textColourId, accent.interpolatedWith(juce::Colours::white, 0.35f));
    k.label.setTooltip(tip);
    addAndMakeVisible(k.label);
}

void RDSBAudioProcessorEditor::addChoice(Choice& c, const juce::String& id, const juce::StringArray& items,
                                         const juce::String& tip, juce::Colour accent) {
    c.box.addItemList(items, 1);
    c.box.setTooltip(tip);
    c.box.setColour(juce::ComboBox::outlineColourId, accent);
    c.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, id, c.box);
    addAndMakeVisible(c.box);
}

void RDSBAudioProcessorEditor::addToggle(Toggle& t, const juce::String& id, const juce::String& title,
                                         const juce::String& tip, juce::Colour accent) {
    t.button.setButtonText(title);
    t.button.setTooltip(tip);
    t.button.setColour(juce::ToggleButton::tickColourId, accent);
    t.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, id, t.button);
    addAndMakeVisible(t.button);
}

//==============================================================================
void RDSBAudioProcessorEditor::refreshPresetList(const juce::String& selectName) {
    presetBox.clear(juce::dontSendNotification);
    int id = 1;
    for (const auto& p : Presets::factory()) presetBox.addItem(p.name, id++);

    userPresetFiles = Presets::userPresetFiles();
    if (userPresetFiles.size() > 0) {
        presetBox.addSeparator();
        presetBox.addSectionHeading("USER");
        int uid = 1001;
        for (const auto& f : userPresetFiles) {
            presetBox.addItem(f.getFileNameWithoutExtension(), uid);
            if (selectName.isNotEmpty() && f.getFileNameWithoutExtension() == juce::File::createLegalFileName(selectName))
                presetBox.setSelectedId(uid, juce::dontSendNotification);
            ++uid;
        }
    }
}

void RDSBAudioProcessorEditor::stepPreset(int delta) {
    const int count = presetBox.getNumItems();
    if (count == 0) return;
    int index = presetBox.getSelectedItemIndex();
    for (int n = 0; n < count; ++n) {
        index = (index + delta + count) % count;
        if (presetBox.getItemId(index) != 0) {
            presetBox.setSelectedItemIndex(index, juce::sendNotificationSync);
            return;
        }
    }
}

void RDSBAudioProcessorEditor::promptSavePreset() {
    auto* w = new juce::AlertWindow("Save preset", "Name this sound:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", "My Transition");
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    w->enterModalState(true, juce::ModalCallbackFunction::create([this, w](int result) {
        if (result == 1) {
            const auto name = w->getTextEditorContents("name").trim();
            if (name.isNotEmpty() && Presets::saveUser(audioProcessor, name)) refreshPresetList(name);
        }
    }), true);
}

void RDSBAudioProcessorEditor::timerCallback() { repaint(visualizerArea); }

//==============================================================================
void RDSBAudioProcessorEditor::paint(juce::Graphics& g) {
    uiScale = (float) getWidth() / (float) kBaseWidth;
    const float s = uiScale;

    g.fillAll(LAF::background);

    // Title: one colour per word, matching the engine it names.
    {
        struct Word { const char* text; juce::Colour c; };
        const Word words[] = { {"RISE", LAF::violet}, {".", LAF::dimText}, {"DROP", LAF::pink}, {".", LAF::dimText},
                               {"SWEEP", LAF::cyan}, {".", LAF::dimText}, {"BOOM", LAF::orange} };
        g.setFont(LAF::monoFont(22.0f * s, true));
        float x = 20.0f * s;
        const float y = 12.0f * s, h = 32.0f * s;
        for (const auto& w : words) {
            juce::GlyphArrangement ga;
            ga.addLineOfText(g.getCurrentFont(), w.text, 0.0f, 0.0f);
            const float tw = ga.getBoundingBox(0, -1, true).getWidth();
            g.setColour(w.c.withAlpha(0.28f));
            g.drawText(w.text, juce::Rectangle<float>(x + 1.5f, y + 1.5f, tw + 4.0f, h), juce::Justification::centredLeft, false);
            g.setColour(w.c);
            g.drawText(w.text, juce::Rectangle<float>(x, y, tw + 4.0f, h), juce::Justification::centredLeft, false);
            x += tw;
        }
    }

    // Section panels, tinted per engine.
    const float ch = 9.0f * s;
    for (const auto& sec : sections) {
        const auto r = sec.bounds;
        juce::Path p;
        p.startNewSubPath(r.getX() + ch, r.getY());
        p.lineTo(r.getRight(), r.getY());
        p.lineTo(r.getRight(), r.getBottom() - ch);
        p.lineTo(r.getRight() - ch, r.getBottom());
        p.lineTo(r.getX(), r.getBottom());
        p.lineTo(r.getX(), r.getY() + ch);
        p.closeSubPath();

        g.setColour(LAF::panel);
        g.fillPath(p);
        g.setColour(sec.accent.withAlpha(0.10f));
        g.strokePath(p, juce::PathStrokeType(4.0f * s));
        g.setColour(sec.accent.withAlpha(0.75f));
        g.strokePath(p, juce::PathStrokeType(1.0f));

        // Accent tab under the title.
        g.setColour(sec.accent);
        g.fillRect(juce::Rectangle<float>(r.getX() + 12.0f * s, r.getY() + 19.0f * s, 22.0f * s, 2.0f * s));
        g.setFont(LAF::monoFont(10.5f * s, true));
        g.drawText(sec.title, r.withHeight(18.0f * s).reduced(12.0f * s, 0), juce::Justification::centredLeft, false);
    }

    // Visualizer is drawn in base coordinates and scaled with the window.
    {
        juce::Graphics::ScopedSaveState ss(g);
        g.addTransform(juce::AffineTransform::scale(uiScale));
        drawVisualizer(g, juce::Rectangle<int>(12, kHeaderHeight + 4, kBaseWidth - 24, kVisualizerHeight));
    }
}

void RDSBAudioProcessorEditor::resized() {
    uiScale = (float) getWidth() / (float) kBaseWidth;
    const float s = uiScale;
    auto area = getLocalBounds();

    // ---- header
    auto header = area.removeFromTop((int) (kHeaderHeight * s));
    const int padY = (int) (18 * s);
    savePresetButton.setBounds(header.removeFromRight((int) (72 * s)).reduced((int) (3 * s), padY));
    nextPresetButton.setBounds(header.removeFromRight((int) (32 * s)).reduced((int) (2 * s), padY));
    presetBox.setBounds(header.removeFromRight((int) (220 * s)).reduced((int) (2 * s), padY));
    prevPresetButton.setBounds(header.removeFromRight((int) (32 * s)).reduced((int) (2 * s), padY));

    // title takes ~440px; controls follow
    int x = (int) (470 * s);
    modeCombo.box.setBounds(x, padY, (int) (120 * s), (int) (28 * s));
    x += (int) (130 * s);
    syncCombo.box.setBounds(x, padY, (int) (120 * s), (int) (28 * s));
    x += (int) (130 * s);
    triggerButton.setBounds(x, padY, (int) (86 * s), (int) (28 * s));
    x += (int) (100 * s);
    barsKnob.label.setBounds(x, (int) (6 * s), (int) (92 * s), (int) (14 * s));
    barsKnob.slider.setBounds(x, (int) (18 * s), (int) (92 * s), (int) (46 * s));
    barsKnob.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, (int) (62 * s), (int) (16 * s));

    visualizerArea = juce::Rectangle<int>((int) (12 * s), (int) ((kHeaderHeight + 4) * s),
                                          (int) ((kBaseWidth - 24) * s), (int) (kVisualizerHeight * s));
    area.removeFromTop((int) ((kVisualizerHeight + 10) * s));

    // ---- rows of sections
    auto body = area.reduced((int) (12 * s), (int) (2 * s));
    const int gap = (int) (8 * s);
    const int rowHeights[2] = { (int) (kRow1Height * s), (int) (kRow2Height * s) };

    for (size_t r = 0; r < rows.size(); ++r) {
        auto rowArea = body.removeFromTop(rowHeights[r]);
        body.removeFromTop(gap);

        // The keyboard shares the second row with its three sections.
        juce::Rectangle<int> keyArea;
        if (r == 1) keyArea = rowArea.removeFromRight((int) (kBaseWidth * 0.36f * s));

        float totalWeight = 0.0f;
        for (auto idx : rows[r]) totalWeight += sections[idx].weight;
        const float unit = (float) (rowArea.getWidth() - gap * ((int) rows[r].size() - (r == 1 ? 0 : 1))) / totalWeight;

        float sx = (float) rowArea.getX();
        for (auto idx : rows[r]) {
            auto& sec = sections[idx];
            const float w = unit * sec.weight;
            sec.bounds = {sx, (float) rowArea.getY(), w, (float) rowArea.getHeight()};

            auto inner = sec.bounds.toNearestInt().reduced((int) (7 * s), 0);
            inner.removeFromTop((int) (24 * s));
            inner.removeFromBottom((int) (5 * s));

            for (auto* t : sec.toggles) t->button.setBounds(inner.removeFromTop((int) (24 * s)));
            for (auto* c : sec.choices) {
                c->box.setBounds(inner.removeFromTop((int) (24 * s)).reduced(0, (int) (1 * s)));
                inner.removeFromTop((int) (2 * s));
            }

            if (!sec.knobs.empty()) {
                const int perRow = sec.perRow;
                const int numRows = ((int) sec.knobs.size() + perRow - 1) / perRow;
                const int cellW = inner.getWidth() / perRow;
                const int cellH = inner.getHeight() / numRows;
                for (size_t i = 0; i < sec.knobs.size(); ++i) {
                    auto* k = sec.knobs[i];
                    const int col = (int) i % perRow, row = (int) i / perRow;
                    juce::Rectangle<int> cell(inner.getX() + col * cellW, inner.getY() + row * cellH, cellW, cellH);
                    k->label.setBounds(cell.removeFromTop((int) (14 * s)));
                    k->label.setFont(LAF::monoFont(10.0f * s, true));
                    k->slider.setBounds(cell);
                    k->slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                              juce::jmin(cellW - 2, (int) (68 * s)), (int) (16 * s));
                }
            }
            sx += w + (float) gap;
        }
        if (r == 1) keyboard.setBounds(keyArea.reduced((int) (4 * s), (int) (8 * s)));
    }
    keyboard.setKeyWidth((float) keyboard.getWidth() / 43.0f);
}

//==============================================================================
void RDSBAudioProcessorEditor::drawVisualizer(juce::Graphics& g, juce::Rectangle<int> area) {
    const auto f = area.toFloat();
    g.setColour(juce::Colour(0xff08080d));
    g.fillRect(f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawRect(f, 1.0f);

    const bool downlifter = param("mode") > 0.5f;
    const float bars = param("duration_bars");
    const float k = param("warp_k");

    // Where the impact(s) fall, in time: before the sweep, after it, or both.
    const int boomPos = static_cast<int>(param("boom_position"));
    const bool boomAtStart = boomPos == 1 || boomPos == 3 || (boomPos == 0 && downlifter);
    const bool boomAtEnd   = boomPos == 2 || boomPos == 3 || (boomPos == 0 && !downlifter);

    // Timeline: [boom?] sweep [boom?]
    const float boomFrac = 0.14f;
    const float pad = 8.0f;
    const float left = f.getX() + pad, width = f.getWidth() - pad * 2.0f;
    const float top = f.getY() + 24.0f, bottom = f.getBottom() - 14.0f, hgt = bottom - top;
    const float boomW = width * boomFrac;
    const float sweepW = width - boomW * ((boomAtStart ? 1 : 0) + (boomAtEnd ? 1 : 0));
    const float sweepX = left + (boomAtStart ? boomW : 0.0f);

    // grid
    g.setColour(juce::Colours::white.withAlpha(0.045f));
    for (int i = 1; i < 8; ++i) g.drawVerticalLine((int) (sweepX + sweepW * i / 8.0f), top, bottom);
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine((int) (top + hgt * i / 4.0f), left, left + width);

    // Progress t (0..1, left to right in time) -> active sweep position.
    auto active = [&](float t) { return downlifter ? 1.0f - t : t; };
    auto yFor = [&](float norm) { return bottom - juce::jlimit(0.0f, 1.0f, norm) * hgt; };
    auto strokeCurve = [&](const juce::Path& path, juce::Colour c, float w) {
        g.setColour(c.withAlpha(0.18f));
        g.strokePath(path, juce::PathStrokeType(w * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(c);
        g.strokePath(path, juce::PathStrokeType(w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    constexpr int steps = 160;

    // --- Tonal pitch (violet)
    if (param("tonal_volume") > 0.001f) {
        const int key = audioProcessor.getTriggerMidiNote() - 36;
        const float ps = juce::jlimit(0.0f, 127.0f, param("tonal_pitch_start") + key);
        const float pe = juce::jlimit(0.0f, 127.0f, param("tonal_pitch_end") + key);
        juce::Path path, fill;
        for (int i = 0; i <= steps; ++i) {
            const float t = (float) i / steps;
            const float midi = ps + (pe - ps) * std::pow(active(t), k);
            const float x = sweepX + t * sweepW, y = yFor(midi / 127.0f);
            if (i == 0) { path.startNewSubPath(x, y); fill.startNewSubPath(x, bottom); fill.lineTo(x, y); }
            else { path.lineTo(x, y); fill.lineTo(x, y); }
        }
        fill.lineTo(sweepX + sweepW, bottom);
        fill.closeSubPath();
        g.setGradientFill(juce::ColourGradient(LAF::violet.withAlpha(0.20f), 0, top, LAF::violet.withAlpha(0.0f), 0, bottom, false));
        g.fillPath(fill);
        strokeCurve(path, LAF::violet, 2.0f);

        g.setFont(LAF::monoFont(9.0f, true));
        g.setColour(LAF::violet);
        const float y0 = yFor((ps + (pe - ps) * std::pow(active(0.0f), k)) / 127.0f);
        const float y1 = yFor((ps + (pe - ps) * std::pow(active(1.0f), k)) / 127.0f);
        g.drawText(noteName(ps + (pe - ps) * std::pow(active(0.0f), k)), (int) sweepX + 4, (int) y0 - 14, 48, 12, juce::Justification::left, false);
        g.drawText(noteName(ps + (pe - ps) * std::pow(active(1.0f), k)), (int) (sweepX + sweepW) - 52, (int) y1 - 14, 48, 12, juce::Justification::right, false);
    }

    // --- Noise cutoff (cyan), log frequency axis
    if (param("noise_volume") > 0.001f) {
        const float cs = param("noise_cutoff_start"), ce = param("noise_cutoff_end");
        auto logNorm = [](float hzv) { return (std::log(juce::jlimit(20.0f, 20000.0f, hzv)) - std::log(20.0f)) / (std::log(20000.0f) - std::log(20.0f)); };
        juce::Path path;
        for (int i = 0; i <= steps; ++i) {
            const float t = (float) i / steps;
            const float c = cs + (ce - cs) * std::pow(active(t), k);
            const float x = sweepX + t * sweepW, y = yFor(logNorm(c));
            if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        strokeCurve(path, LAF::cyan, 1.8f);
    }

    // --- Filter FX (lime)
    if (param("filter_enable") > 0.5f) {
        const bool hp = param("filter_mode") > 0.5f;
        auto logNorm = [](float hzv) { return (std::log(juce::jlimit(20.0f, 20000.0f, hzv)) - std::log(20.0f)) / (std::log(20000.0f) - std::log(20.0f)); };
        juce::Path path;
        for (int i = 0; i <= steps; ++i) {
            const float t = (float) i / steps;
            const float w = std::pow(active(t), k);
            const float c = hp ? 20000.0f - (20000.0f - 30.0f) * w : 100.0f + (20000.0f - 100.0f) * w;
            const float x = sweepX + t * sweepW, y = yFor(logNorm(c));
            if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(LAF::lime.withAlpha(0.9f));
        float dashes[] = { 5.0f, 4.0f };
        juce::Path dashed;
        juce::PathStrokeType(1.4f).createDashedStroke(dashed, path, dashes, 2);
        g.fillPath(dashed);
    }

    // --- Pump pulses (pink ticks along the floor)
    if (param("pumper_depth") > 0.01f && param("tonal_volume") > 0.001f) {
        const float rs = param("pumper_rate_start"), re = param("pumper_rate_end");
        const float totalBeats = bars * 4.0f;
        float acc = 0.0f;
        int lastWhole = 0;
        g.setColour(LAF::pink);
        for (int i = 1; i <= 800; ++i) {
            const float t = (float) i / 800.0f;
            const float rate = rs + (re - rs) * active(t);
            acc += rate * totalBeats / 800.0f;
            if ((int) acc > lastWhole) {
                lastWhole = (int) acc;
                const float x = sweepX + t * sweepW;
                g.fillRect(x - 0.75f, bottom - 7.0f, 1.5f, 7.0f);
            }
        }
    }

    // --- Boom (orange): punch spike then the sub tail, at the start and/or end
    if (param("boom_volume") > 0.001f) {
        const float punch = param("boom_noise_decay"), rumble = param("boom_sub_decay");
        const float total = juce::jmax(rumble, 0.1f);
        const float endFloor = std::exp(-5.0f);

        auto drawBoom = [&](float x0, bool label) {
            juce::Path tail;
            tail.startNewSubPath(x0, bottom);
            for (int i = 0; i <= 80; ++i) {
                const float t = (float) i / 80.0f * total;
                const float raw = std::exp(-t * (5.0f / rumble));
                const float env = juce::jmax(0.0f, (raw - endFloor) / (1.0f - endFloor));
                tail.lineTo(x0 + (float) i / 80.0f * boomW, yFor(0.15f + 0.8f * env * param("boom_volume")));
            }
            tail.lineTo(x0 + boomW, bottom);
            tail.closeSubPath();
            g.setGradientFill(juce::ColourGradient(LAF::orange.withAlpha(0.5f), 0, top, LAF::orange.withAlpha(0.05f), 0, bottom, false));
            g.fillPath(tail);
            g.setColour(LAF::orange);
            g.strokePath(tail, juce::PathStrokeType(1.6f));

            const float spikeW = juce::jlimit(3.0f, boomW * 0.4f, boomW * (punch / total) * 3.0f);
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.fillRect(x0, yFor(0.98f), spikeW * 0.35f, bottom - yFor(0.98f));
            g.setColour(LAF::orange.withAlpha(0.35f));
            g.fillRect(x0, yFor(0.98f), spikeW, bottom - yFor(0.98f));

            if (label) {
                g.setFont(LAF::monoFont(9.0f, true));
                g.setColour(LAF::orange);
                g.drawText("BOOM", (int) x0 + 4, (int) top - 2, 60, 12, juce::Justification::left, false);
            }
        };
        if (boomAtStart) drawBoom(left, true);
        if (boomAtEnd) drawBoom(sweepX + sweepW, true);
    }

    // Drop lines where the sweep meets an impact.
    {
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        float dashes[] = { 3.0f, 3.0f };
        auto dropLine = [&](float dx) {
            juce::Path line, dashed;
            line.startNewSubPath(dx, top - 4.0f);
            line.lineTo(dx, bottom);
            juce::PathStrokeType(1.0f).createDashedStroke(dashed, line, dashes, 2);
            g.fillPath(dashed);
        };
        if (boomAtStart) dropLine(sweepX);
        if (boomAtEnd) dropLine(sweepX + sweepW);
    }

    // Playhead
    if (audioProcessor.isPlaying()) {
        const float t = juce::jlimit(0.0f, 1.0f, audioProcessor.getMasterPhase());
        const float x = sweepX + t * sweepW;
        g.setColour(juce::Colours::white.withAlpha(0.25f));
        g.fillRect(x - 3.0f, top - 4.0f, 6.0f, bottom - top + 4.0f);
        g.setColour(juce::Colours::white);
        g.fillRect(x - 0.75f, top - 4.0f, 1.5f, bottom - top + 4.0f);
    }

    // Legend and readout
    g.setFont(LAF::monoFont(9.5f, true));
    float lx = f.getX() + 10.0f;
    auto legend = [&](const char* text, juce::Colour c, bool on) {
        g.setColour(c.withAlpha(on ? 1.0f : 0.25f));
        g.fillEllipse(lx, f.getY() + 8.0f, 7.0f, 7.0f);
        g.drawText(text, (int) lx + 11, (int) f.getY() + 4, 70, 14, juce::Justification::left, false);
        lx += 82.0f;
    };
    legend("TONAL", LAF::violet, param("tonal_volume") > 0.001f);
    legend("NOISE", LAF::cyan, param("noise_volume") > 0.001f);
    legend("FILTER", LAF::lime, param("filter_enable") > 0.5f);
    legend("PUMP", LAF::pink, param("pumper_depth") > 0.01f);
    legend("BOOM", LAF::orange, param("boom_volume") > 0.001f);

    g.setColour(LAF::dimText);
    const juce::String info = juce::String(downlifter ? "DOWNLIFTER" : "RISER") + "  "
                              + juce::String(bars, 1) + " BARS  KEY " + noteName(audioProcessor.getTriggerMidiNote());
    g.drawText(info, f.toNearestInt().removeFromRight(300).withTrimmedRight(10).withHeight(20),
               juce::Justification::centredRight, false);
}
