#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

// Overall size of the plugin window. The whole UI is designed at 1620 x 280
// and drawn at this scale (0.7 -> 1134 x 196).
static constexpr float uiScale = 0.7f;

//==============================================================================
class SunrizeLook : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

//==============================================================================
class NavArrow : public juce::Button
{
public:
    NavArrow (bool isNext) : juce::Button (isNext ? "Next sound" : "Previous sound"), next (isNext) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    bool next;
};

class InfoButton : public juce::Button
{
public:
    InfoButton() : juce::Button ("Info") {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::Font font { juce::FontOptions (22.0f) };
};

//==============================================================================
class CloseX : public juce::Button
{
public:
    CloseX() : juce::Button ("Close") {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// The info window: a borderless window of its own that pops up under the synth.
class InfoContent : public juce::Component
{
public:
    InfoContent (juce::Typeface::Ptr mono);
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onClose;
    static constexpr float scale = 280.0f * uiScale / 165.0f;   // same height as the synth
    static constexpr float pad = 16.0f;               // padding from every edge (design px)
private:
    juce::Image logo;
    juce::Typeface::Ptr mono;
    CloseX closeBtn;
};

//==============================================================================
class SunrizeAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SunrizeAudioProcessorEditor (SunrizeAudioProcessor&);
    ~SunrizeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void stepSound (int delta);
    juce::Font mono (float size, float kerning = 0.08f) const;
    void repaintDesign (juce::Rectangle<int> r);

    SunrizeAudioProcessor& proc;
    SunrizeLook look;
    juce::Typeface::Ptr monoFace;

    juce::Slider sun;
    std::array<juce::Slider, 10> knobs;
    juce::Slider output;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;

    NavArrow prevBtn { false }, nextBtn { true };
    InfoButton infoBtn;
    std::unique_ptr<InfoContent> infoWindow;

    std::array<float, 2048> scopeData {};
    std::array<float, 180> trace {};
    int soundIndex = -1;

    static constexpr int W = 1620, H = 280;
    static constexpr int knobSize = 85;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SunrizeAudioProcessorEditor)
};
