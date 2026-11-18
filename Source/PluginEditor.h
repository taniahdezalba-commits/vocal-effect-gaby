#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace gabyui
{
namespace col
{
    const juce::Colour bgTop    { 0xff12121f };
    const juce::Colour bgBottom { 0xff07070d };
    const juce::Colour panelHi  { 0xff22222d };
    const juce::Colour border   { 0xff2e2e3c };
    const juce::Colour purple   { 0xffa855f7 };
    const juce::Colour violet   { 0xff7c3aed };
    const juce::Colour cyan     { 0xff22d3ee };
    const juce::Colour text     { 0xffe8e8f0 };
    const juce::Colour textDim  { 0xff9a9ab0 };
    const juce::Colour green    { 0xff22c55e };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos,
                           float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class LevelMeter : public juce::Component
{
public:
    void setLevel (float linearPeak);
    void paint (juce::Graphics&) override;
private:
    float levelDb = -60.0f;
};

class SpectrumDisplay : public juce::Component
{
public:
    void update (const VocalEffectGabyProcessor&);
    void paint (juce::Graphics&) override;
private:
    std::array<float, gabyvx::kDisplayBands> in {}, out {}, noise {};
    bool voice = false;
};

class PowerButton : public juce::ToggleButton
{
public:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

} // namespace gabyui

class VocalEffectGabyEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VocalEffectGabyEditor (VocalEffectGabyProcessor&);
    ~VocalEffectGabyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& text, const juce::String& suffix);
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title) const;
    void setMode (int m);

    VocalEffectGabyProcessor& proc;
    gabyui::LookAndFeel lnf;

    gabyui::LevelMeter inMeter, outMeter;
    gabyui::SpectrumDisplay spectrum;
    gabyui::PowerButton powerButton;

    juce::Slider clarityKnob, ambienceKnob, gateKnob, gainKnob, volumeKnob;
    juce::Label clarityLabel, ambienceLabel, gateLabel, gainLabel, volumeLabel;
    juce::Slider bandSliders[4];
    juce::Label bandLabels[4];
    juce::TextButton voiceButton, ambienceButton;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SliderAtt> clarityAtt, ambienceAtt, gateAtt, gainAtt, volumeAtt, bandAtt[4];
    std::unique_ptr<ButtonAtt> powerAtt;

    juce::Rectangle<int> headerArea, inPanel, outPanel, ambPanel, redPanel, clarityPanel, bandPanel;
    float shownReduction = 0.0f;
    bool shownVoice = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalEffectGabyEditor)
};
