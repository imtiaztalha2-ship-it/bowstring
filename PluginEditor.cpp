#include "PluginEditor.h"

namespace palette
{
    const juce::Colour rosewoodTop    { 0xff4a2621 };
    const juce::Colour rosewoodBottom { 0xff2b1512 };
    const juce::Colour varnish        { 0xffe0a458 };
    const juce::Colour parchment      { 0xfff3e6cc };
}

//==============================================================================
PedalLookAndFeel::PedalLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff221110));
    setColour (juce::ComboBox::textColourId, palette::parchment);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::black.withAlpha (0.6f));
    setColour (juce::ComboBox::arrowColourId, palette::varnish);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff2b1512));
    setColour (juce::PopupMenu::textColourId, palette::parchment);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, palette::varnish);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
    const float r  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  c  = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto  accent = slider.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path arc;
    arc.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, angle, true);
    g.setColour (accent);
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // chicken-head style knob cap
    const float kr = r * 0.72f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4b4642), c.x, c.y - kr,
                                             juce::Colour (0xff171514), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.2f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -kr + 2.0f, 3.0f, kr * 0.6f, 1.5f);
    g.setColour (palette::parchment);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

//==============================================================================
PedalSection::PedalSection (juce::AudioProcessorValueTreeState& state, const juce::String& t, juce::Colour col,
                            const juce::String& powerId, std::vector<Spec> knobSpecs, std::vector<Spec> comboSpecs)
    : title (t), colour (col)
{
    ink = colour.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff2a1a12) : palette::parchment;

    for (auto& s : comboSpecs)
    {
        auto c = std::make_unique<Combo>();
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (s.id)))
            c->box.addItemList (choice->choices, 1);
        c->attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, s.id, c->box);
        c->label.setText (s.label, juce::dontSendNotification);
        c->label.setFont (juce::FontOptions (12.0f));
        c->label.setColour (juce::Label::textColourId, ink);
        addAndMakeVisible (c->box);
        addAndMakeVisible (c->label);
        combos.push_back (std::move (c));
    }

    for (auto& s : knobSpecs)
    {
        auto k = std::make_unique<Knob>();
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW - 2, 16);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, ink);
        k->slider.setColour (juce::Slider::textBoxTextColourId, ink);
        k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, s.id, k->slider);
        k->label.setText (s.label, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setFont (juce::FontOptions (12.0f));
        k->label.setColour (juce::Label::textColourId, ink);
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        knobs.push_back (std::move (k));
    }

    if (powerId.isNotEmpty())
    {
        power = std::make_unique<juce::ToggleButton> ("On");
        power->setColour (juce::ToggleButton::textColourId, ink);
        power->setColour (juce::ToggleButton::tickColourId, ink);
        power->setColour (juce::ToggleButton::tickDisabledColourId, ink.withAlpha (0.5f));
        powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, powerId, *power);
        power->onStateChange = [this] { refreshPower(); };
        addAndMakeVisible (*power);
    }

    refreshPower();
}

int PedalSection::getPreferredWidth() const
{
    return 16 + (combos.empty() ? 0 : comboW) + (int) knobs.size() * knobW;
}

void PedalSection::refreshPower()
{
    const bool on = power == nullptr || power->getToggleState();
    for (auto& k : knobs)  { k->slider.setAlpha (on ? 1.0f : 0.45f); k->label.setAlpha (on ? 1.0f : 0.45f); }
    for (auto& c : combos) { c->box.setAlpha (on ? 1.0f : 0.45f); }
    repaint();
}

void PedalSection::paint (juce::Graphics& g)
{
    const bool on = power == nullptr || power->getToggleState();
    const auto b  = getLocalBounds().toFloat().reduced (1.0f);

    g.setGradientFill (juce::ColourGradient (colour.brighter (0.15f), 0.0f, b.getY(),
                                             colour.darker (0.35f), 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, 9.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (b, 9.0f, 1.5f);

    // status LED
    auto titleArea = getLocalBounds().reduced (10, 8).removeFromTop (22);
    if (power != nullptr)
    {
        const auto led = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ (float) titleArea.getX() + 4.0f,
                                                                            (float) titleArea.getCentreY() });
        g.setColour (on ? juce::Colour (0xffff4d3a) : juce::Colour (0xff4a1e18));
        g.fillEllipse (led);
        if (on)
        {
            g.setColour (juce::Colour (0x55ff4d3a));
            g.fillEllipse (led.expanded (3.0f));
        }
        titleArea.removeFromLeft (14);
    }

    g.setColour (ink);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (title, titleArea, juce::Justification::centredLeft);
}

void PedalSection::resized()
{
    auto r   = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (24);
    if (power != nullptr)
        power->setBounds (top.removeFromRight (50));
    r.removeFromTop (4);

    if (! combos.empty())
    {
        auto col = r.removeFromLeft (comboW);
        for (auto& c : combos)
        {
            auto cell = col.removeFromTop (38);
            c->label.setBounds (cell.removeFromTop (14));
            c->box.setBounds (cell.removeFromTop (22).reduced (2, 0));
        }
    }

    for (auto& k : knobs)
    {
        auto cell = r.removeFromLeft (knobW);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    }
}

//==============================================================================
BowStringEditor::BowStringEditor (BowStringProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    auto make = [&s] (const char* title, juce::uint32 argb, const char* powerId,
                      std::vector<PedalSection::Spec> knobs, std::vector<PedalSection::Spec> combos = {})
    {
        return std::make_unique<PedalSection> (s, title, juce::Colour (argb), powerId, std::move (knobs), std::move (combos));
    };

    topRow.push_back (make ("Instrument", 0xffb5763a, "",
                            { { "blend", "Blend" }, { "bright", "Bright" }, { "bownoise", "Bow noise" }, { "sympath", "Sympathetic" } },
                            { { "mode", "Instrument" }, { "key", "Sarangi key" }, { "octave", "Octave" } }));
    topRow.push_back (make ("Bowing", 0xff5a3d2b, "",
                            { { "attack", "Attack" }, { "release", "Release" }, { "dynamics", "Dynamics" },
                              { "glide", "Glide" }, { "vibrate", "Vib rate" }, { "vibdepth", "Vib depth" } }));

    pedalRow.push_back (make ("Gate",   0xff5b7083, "gate_on",   { { "gate", "Threshold" } }));
    pedalRow.push_back (make ("Comp",   0xff2f7f78, "comp_on",   { { "sustain", "Sustain" } }));
    pedalRow.push_back (make ("Drive",  0xffc2532b, "drive_on",  { { "drive", "Drive" }, { "drivelvl", "Level" } }));
    pedalRow.push_back (make ("EQ",     0xffd8cfb8, "eq_on",     { { "bass", "Bass" }, { "mid", "Mid" }, { "treble", "Treble" } }));
    pedalRow.push_back (make ("Chorus", 0xff7c5da3, "chorus_on", { { "chorusrate", "Rate" }, { "chorusdepth", "Depth" }, { "chorusmix", "Mix" } }));
    pedalRow.push_back (make ("Delay",  0xff2d5f9e, "delay_on",  { { "delaytime", "Time" }, { "delayfb", "Feedback" }, { "delaymix", "Mix" } }));
    pedalRow.push_back (make ("Reverb", 0xff3f6b45, "reverb_on", { { "revsize", "Size" }, { "revdamp", "Damping" }, { "revmix", "Mix" } }));
    pedalRow.push_back (make ("Output", 0xffa8873c, "",          { { "output", "Volume" } }));

    for (auto& sec : topRow)   addAndMakeVisible (*sec);
    for (auto& sec : pedalRow) addAndMakeVisible (*sec);

    setSize (1240, 400);
    startTimerHz (20);
}

BowStringEditor::~BowStringEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BowStringEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (palette::rosewoodTop, 0.0f, 0.0f,
                                             palette::rosewoodBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    auto header = getLocalBounds().reduced (16, 10).removeFromTop (56);

    g.setColour (palette::varnish);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("BowString", header.removeFromLeft (200), juce::Justification::centredLeft);

    g.setColour (palette::parchment.withAlpha (0.7f));
    g.setFont (juce::FontOptions (14.0f));
    g.drawText ("Play guitar, hear violin or sarangi", header.removeFromLeft (300), juce::Justification::centredLeft);

    // note readout
    auto noteBox = header.removeFromRight (230).reduced (0, 8);
    g.setColour (juce::Colour (0xff1c0e0c));
    g.fillRoundedRectangle (noteBox.toFloat(), 6.0f);
    g.setColour (palette::varnish);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText (noteText, noteBox, juce::Justification::centred);

    // input meter
    auto meter = header.removeFromRight (180).reduced (12, 20);
    g.setColour (juce::Colour (0xff1c0e0c));
    g.fillRoundedRectangle (meter.toFloat(), 4.0f);
    const float db   = juce::Decibels::gainToDecibels (level, -80.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (db + 80.0f) / 80.0f);
    g.setColour (frac > 0.9f ? juce::Colour (0xffff4d3a) : palette::varnish);
    g.fillRoundedRectangle (meter.toFloat().withWidth ((float) meter.getWidth() * frac), 4.0f);
    g.setColour (palette::parchment.withAlpha (0.6f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Input", meter.translated (0, -16), juce::Justification::centredLeft);
}

void BowStringEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (64);

    auto layoutRow = [] (std::vector<std::unique_ptr<PedalSection>>& row, juce::Rectangle<int> rowArea)
    {
        constexpr int gap = 6;
        int total = gap * ((int) row.size() - 1);
        for (auto& s : row) total += s->getPreferredWidth();

        int x = rowArea.getX() + juce::jmax (0, (rowArea.getWidth() - total) / 2);
        for (auto& s : row)
        {
            s->setBounds (x, rowArea.getY(), s->getPreferredWidth(), rowArea.getHeight());
            x += s->getPreferredWidth() + gap;
        }
    };

    layoutRow (topRow, area.removeFromTop (150));
    area.removeFromTop (10);
    layoutRow (pedalRow, area.removeFromTop (150));
}

void BowStringEditor::timerCallback()
{
    const float hz = proc.detectedPitch.load();
    level = proc.inputLevel.load();

    if (hz > 20.0f)
    {
        const int midi = juce::roundToInt (69.0f + 12.0f * std::log2 (hz / 440.0f));
        noteText = juce::MidiMessage::getMidiNoteName (midi, true, true, 4) + "   " + juce::String (hz, 1) + " Hz";
    }
    else
    {
        noteText = "--";
    }

    repaint (0, 0, getWidth(), 76);
}
