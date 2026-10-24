#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Deep-space palette: near-black navy, violet nebula, neon cyan + magenta
namespace Palette
{
    const juce::Colour space      { 0xff04050d };
    const juce::Colour spaceMid   { 0xff0a0d24 };
    const juce::Colour nebulaA    { 0xff5b2a9e };   // violet
    const juce::Colour nebulaB    { 0xff1b4fa8 };   // deep blue
    const juce::Colour nebulaC    { 0xffb02a8a };   // magenta dust
    const juce::Colour glass      { 0xb3080c1f };   // translucent panel
    const juce::Colour glassEdge  { 0xff2a3a78 };
    const juce::Colour text       { 0xffe6ecff };
    const juce::Colour textDim    { 0xff7f8ac0 };
    const juce::Colour cyan       { 0xff29e3ff };
    const juce::Colour magenta    { 0xffe040fb };
    const juce::Colour input      { 0xff6a4bd8 };   // violet (input level)
    const juce::Colour output     { 0xff29e3ff };   // cyan (output level)
    const juce::Colour reduction  { 0xffff3fa4 };   // hot pink (gain reduction)
}

// Shared drawing helper: a translucent glass panel with a soft neon edge glow
void drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner, float glow = 0.35f);

class GalaxyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GalaxyLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
};

// Scrolling input / output / gain-reduction history
class HistoryDisplay : public juce::Component
{
public:
    void pushFrame (const HistoryFrame& f)
    {
        frames[(size_t) writePos] = f;
        writePos = (writePos + 1) % kLen;
        if (count < kLen) ++count;
    }

    void setCeilingDb (float db) { ceilingDb = db; }
    void paint (juce::Graphics&) override;

private:
    static constexpr int   kLen      = 1024;
    static constexpr float kTopDb    = 3.0f;
    static constexpr float kBottomDb = -36.0f;

    float dbToY (float db, float h) const
    {
        return juce::jmap (juce::jlimit (kBottomDb, kTopDb, db), kTopDb, kBottomDb, 0.0f, h);
    }

    std::array<HistoryFrame, kLen> frames {};
    int writePos = 0, count = 0;
    float ceilingDb = -1.0f;
};

class MasterLimiterAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::Timer
{
public:
    explicit MasterLimiterAudioProcessorEditor (MasterLimiterAudioProcessor&);
    ~MasterLimiterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void renderBackground();
    void drawLogo (juce::Graphics&);
    void drawMeters (juce::Graphics&);
    void setupCombo (juce::ComboBox&, juce::Label&, const juce::String& paramId, const juce::String& labelText);

    MasterLimiterAudioProcessor& proc;
    GalaxyLookAndFeel lnf;

    HistoryDisplay display;
    std::array<Knob, 6> knobs;

    juce::ComboBox styleBox, osBox, ditherBox;
    juce::Label styleLabel, osLabel, ditherLabel, styleDescription;
    juce::ToggleButton truePeakButton { "True peak" }, unityButton { "Unity gain" }, deltaButton { "Delta" };
    juce::TextButton resetButton { "Reset loudness" };

    std::unique_ptr<ComboBoxAttachment> styleAttachment, osAttachment, ditherAttachment;
    std::unique_ptr<ButtonAttachment>   truePeakAttachment, unityAttachment, deltaAttachment;

    juce::Image background;
    juce::Rectangle<int> meterArea, logoArea, knobPanel, headerArea;
    float meterDb[2] { -100.0f, -100.0f };
    float grDisplayDb = 0.0f;
    float orbitPhase = 0.0f;
    int lastStyle = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterLimiterAudioProcessorEditor)
};
