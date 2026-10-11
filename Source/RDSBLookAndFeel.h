#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Rise.Drop.Sweep.Boom look: black stage, one vivid accent per engine.
//   Tonal sweeper -> violet   Noise engine -> cyan     Boom engine -> orange
//   FX            -> lime     Space/reverb -> pink     Global/master -> white
// Controls take their colour from rotarySliderFillColourId / ComboBox outline /
// ToggleButton tick colour, so a section is tinted by setting those once.
//==============================================================================
class RDSBLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static inline const juce::Colour background { 0xff050507 };
    static inline const juce::Colour panel      { 0xff0c0c12 };
    static inline const juce::Colour violet     { 0xffa66bff };
    static inline const juce::Colour cyan       { 0xff19e3ff };
    static inline const juce::Colour orange     { 0xffff7a1a };
    static inline const juce::Colour lime       { 0xffa8ff2e };
    static inline const juce::Colour pink       { 0xffff4fa3 };
    static inline const juce::Colour white      { 0xfff2f2f7 };
    static inline const juce::Colour dimText    { 0xff8d8da0 };

    RDSBLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Label::textColourId, juce::Colour (0xffe9e9f2));
        setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xfff6f6fb));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.5f));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::white.withAlpha (0.12f));
        setColour (juce::Slider::textBoxHighlightColourId, violet.withAlpha (0.5f));
        setColour (juce::Slider::rotarySliderFillColourId, white);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf010101a));
        setColour (juce::TooltipWindow::textColourId, juce::Colours::white);
        setColour (juce::TooltipWindow::outlineColourId, white);
        setColour (juce::TextButton::buttonColourId, panel);
        setColour (juce::TextButton::textColourOffId, white);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, panel);
        setColour (juce::ComboBox::outlineColourId, white.withAlpha (0.5f));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xfff6f6fb));
        setColour (juce::ComboBox::arrowColourId, white);
        setColour (juce::ToggleButton::tickColourId, white);
        setColour (juce::ToggleButton::textColourId, juce::Colour (0xffe9e9f2));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0e0e16));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xfff6f6fb));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, violet.withAlpha (0.4f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    static juce::Font monoFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Menlo", height,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        const bool isSliderText = dynamic_cast<juce::Slider*> (l.getParentComponent()) != nullptr;
        return monoFont (isSliderText ? 10.5f : 11.0f, true);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return monoFont (12.0f, true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override          { return monoFont (11.0f, true); }
    juce::Font getPopupMenuFont() override                          { return monoFont (13.0f, true); }

    //==========================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto base = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const bool active = slider.isMouseOverOrDragging();
        const auto accent = active ? base.brighter (0.35f) : base;
        const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;

        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        const auto c = b.withSizeKeepingCentre (d, d).getCentre();
        const float R = d * 0.5f;

        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
            const bool major = (i % 5 == 0);
            const float r0 = R - R * (major ? 0.15f : 0.09f);
            g.setColour (dimText.withAlpha (major ? 0.8f : 0.35f));
            g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a),
                        c.x + R * std::sin (a), c.y - R * std::cos (a), major ? 1.3f : 0.9f);
        }

        const float arcR = R * 0.80f;
        const float trackW = juce::jmax (2.0f, R * 0.09f);
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (slider.isEnabled() && std::abs (angle - from) > 0.002f)
        {
            juce::Path v;
            v.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
            g.setColour (accent.withAlpha (0.16f));
            g.strokePath (v, juce::PathStrokeType (trackW * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent.withAlpha (0.35f));
            g.strokePath (v, juce::PathStrokeType (trackW * 1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (v, juce::PathStrokeType (trackW * 0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float capR = arcR * 0.80f;
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillEllipse (c.x - capR - 1.5f, c.y - capR + 1.0f, (capR + 1.5f) * 2.0f, (capR + 1.5f) * 2.0f);
        juce::ColourGradient cap (juce::Colour (0xff4b4b58), c.x - capR * 0.5f, c.y - capR * 0.6f,
                                  juce::Colour (0xff101016), c.x + capR * 0.7f, c.y + capR * 0.8f, true);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
        g.setColour (accent.withAlpha (active ? 0.95f : 0.6f));
        g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.2f);

        const float p0 = capR * 0.25f, p1 = capR * 0.92f;
        g.setColour (juce::Colours::white);
        g.drawLine (c.x + p0 * std::sin (angle), c.y - p0 * std::cos (angle),
                    c.x + p1 * std::sin (angle), c.y - p1 * std::cos (angle), juce::jmax (1.8f, R * 0.07f));
    }

    //==========================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const auto c = down ? juce::Colours::white : findColour (juce::TextButton::textColourOffId);
        g.setColour (c.withAlpha (highlighted || down ? 0.25f : 0.10f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (c);
        g.drawRoundedRectangle (r, 3.0f, 1.2f);
    }

    // A pill switch in the control's accent colour.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        const auto accent = b.findColour (juce::ToggleButton::tickColourId);
        auto r = b.getLocalBounds().toFloat();
        const float h = juce::jmin (16.0f, r.getHeight() - 2.0f), w = h * 1.9f;
        const auto pill = juce::Rectangle<float> (r.getX() + 2.0f, r.getCentreY() - h * 0.5f, w, h);

        g.setColour (b.getToggleState() ? accent.withAlpha (0.28f) : juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (pill, h * 0.5f);
        g.setColour (b.getToggleState() ? accent : dimText);
        g.drawRoundedRectangle (pill, h * 0.5f, 1.2f);

        const float knobX = b.getToggleState() ? pill.getRight() - h * 0.5f - 1.0f : pill.getX() + h * 0.5f + 1.0f;
        if (b.getToggleState())
        {
            g.setColour (accent.withAlpha (0.35f));
            g.fillEllipse (knobX - h * 0.55f, pill.getCentreY() - h * 0.55f, h * 1.1f, h * 1.1f);
        }
        g.setColour (b.getToggleState() ? juce::Colours::white : dimText);
        g.fillEllipse (knobX - h * 0.32f, pill.getCentreY() - h * 0.32f, h * 0.64f, h * 0.64f);

        g.setColour (highlighted ? juce::Colours::white : b.findColour (juce::ToggleButton::textColourId));
        g.setFont (monoFont (11.0f, true));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (w + 10.0f).toNearestInt(), juce::Justification::centredLeft, false);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                       juce::ComboBox& box) override
    {
        const auto accent = box.findColour (juce::ComboBox::outlineColourId);
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
        g.setColour (panel);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (accent.withAlpha (box.isMouseOver() ? 1.0f : 0.65f));
        g.drawRoundedRectangle (r, 3.0f, 1.2f);

        juce::Path arrow;
        const float ax = (float) width - 13.0f, ay = (float) height * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (accent);
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (5, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
        g.setColour (juce::Colour (0xf010101a));
        g.fillRect (r);
        g.setColour (violet);
        g.drawRect (r, 1.0f);
        g.setColour (cyan);
        g.fillRect (r.removeFromLeft (3.0f));
        g.setColour (juce::Colours::white);
        g.setFont (monoFont (12.0f));
        g.drawFittedText (text, r.toNearestInt().reduced (8, 4), juce::Justification::centredLeft, 5);
    }

    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override
    {
        const int w = juce::jmin (280, parentArea.getWidth() - 10);
        juce::AttributedString s;
        s.append (tipText, monoFont (12.0f));
        juce::TextLayout tl;
        tl.createLayout (s, (float) (w - 16));
        const int h = (int) tl.getHeight() + 12;
        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - w - 12 : screenPos.x + 24,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - h - 6 : screenPos.y + 6, w, h)
                   .constrainedWithin (parentArea);
    }
};
