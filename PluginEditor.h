#pragma once

#include "PluginProcessor.h"

//==============================================================================
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PedalLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

//==============================================================================
// One stomp box: a coloured panel with a footswitch (optional), knobs and drop-downs
class PedalSection : public juce::Component
{
public:
    struct Spec { juce::String id, label; };

    PedalSection (juce::AudioProcessorValueTreeState& state, const juce::String& title, juce::Colour colour,
                  const juce::String& powerId, std::vector<Spec> knobSpecs, std::vector<Spec> comboSpecs = {});

    int getPreferredWidth() const;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int knobW = 60, comboW = 112;

private:
    void refreshPower();

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Combo
    {
        juce::ComboBox box;
        juce::Label    label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    juce::String title;
    juce::Colour colour, ink;
    std::vector<std::unique_ptr<Knob>>  knobs;
    std::vector<std::unique_ptr<Combo>> combos;
    std::unique_ptr<juce::ToggleButton> power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
};

//==============================================================================
class BowStringEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BowStringEditor (BowStringProcessor&);
    ~BowStringEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    BowStringProcessor& proc;
    PedalLookAndFeel lnf;
    std::vector<std::unique_ptr<PedalSection>> topRow, pedalRow;
    juce::String noteText { "--" };
    float level = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BowStringEditor)
};
