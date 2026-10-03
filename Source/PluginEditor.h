#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "AnalogLookAndFeel.h"

enum class KnobStyle { aluminium, black, chicken };

// Knob with a silkscreened title; the value appears in a small readout while hovering/dragging.
// Shift-drag = fine adjust, double-click or Alt-click = default value.
class KnobControl : public juce::Component
{
public:
    KnobControl (juce::AudioProcessorValueTreeState&, const char* paramID, const juce::String& title,
                 KnobStyle style, const juce::String& scaleLabels = {});

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseUp (const juce::MouseEvent&) override    { repaint(); }
    void mouseDown (const juce::MouseEvent&) override;

    juce::Slider slider;

private:
    juce::String title;
    double defaultValue = 0.0;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Back-lit moving-coil VU meter with real needle ballistics (mass-spring, ~300 ms, slight overshoot).
// The component covers the whole well; the face is drawn inside the rendered bezel.
class VUMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Mode { input, output, duck };

    void setMode (Mode m) { mode = m; repaint(); }
    Mode getMode() const { return mode; }
    void update (float meanSquare, float peak, float duckGain, float dt);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }

    std::function<void()> onClick;

private:
    Mode mode = Mode::input;
    float needle = 0.0f, velocity = 0.0f, peakLamp = 0.0f;
};

// Green-phosphor CRT: scrolling voice/echo history plus a map of where the repeats land.
class EchoScope : public juce::Component
{
public:
    struct Info
    {
        int mode = 0;
        double delayMs = 100.0, offsetMs = 0.0, bpm = 120.0;
        float feedback = 0.2f, duck = 0.0f, duckGain = 1.0f;
        bool sync = true, reverse = false, freeze = false, power = true;
        juce::String division;
    };

    static constexpr int historySize = 300;

    void pushHistory (float dry, float wet);
    void setInfo (const Info& i) { info = i; repaint(); }
    void paint (juce::Graphics&) override;

private:
    void paintHistory (juce::Graphics&, juce::Rectangle<float>);
    void paintEchoMap (juce::Graphics&, juce::Rectangle<float>);

    std::array<float, historySize> dryHistory {}, wetHistory {};
    int writeIndex = 0;
    Info info;
    int frameCounter = 0;
};

// Everything at 100% scale: the rendered hardware (main unit + optional expander) and the software
// toolbar underneath. The editor scales this component for the 100/125/150% sizes.
class Faceplate : public juce::Component
{
public:
    explicit Faceplate (VoxSlapProcessor&);
    ~Faceplate() override;

    static constexpr int width = 1000, topHeight = 540, bottomHeight = 196, toolbarHeight = 40;
    int getBaseHeight() const { return topHeight + (isAdvanced() ? bottomHeight : 0) + toolbarHeight; }

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void tick (float dt);

    float getScale() const;
    std::function<void()> onLayoutChanged;

private:
    bool isAdvanced() const;
    void setAdvanced (bool);
    void setScale (float);
    void refreshPresetList();
    void savePresetDialog();
    void deletePresetDialog();
    void stepPreset (int delta);
    void paintGroupLabel (juce::Graphics&, juce::Rectangle<int> span, const juce::String& title);

    VoxSlapProcessor& processor;
    Analog::LookAndFeel lnf;

    // Hardware
    VUMeter inputMeter, outputMeter;
    EchoScope scope;
    KnobControl modeKnob, timeKnob, divisionKnob, feedbackKnob, driveKnob, driveTypeKnob, duckKnob, mixKnob;
    juce::ToggleButton syncToggle { "SYNC" }, powerSwitch;
    KnobControl hpfKnob, lpfKnob, widthKnob, offsetKnob, modRateKnob, modDepthKnob,
                pitchKnob, threshKnob, releaseKnob, outputKnob;
    juce::ToggleButton reverseToggle { "REVERSE" }, freezeToggle { "FREEZE" };

    // Software toolbar
    juce::ComboBox presetBox, sizeBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" }, deleteButton { "Delete" };
    juce::TextButton slotA { "A" }, slotB { "B" }, copyButton { "Copy" };
    juce::TextButton simpleButton { "Simple" }, advancedButton { "Advanced" };

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAttachment> syncAttachment, reverseAttachment, freezeAttachment;

    juce::Image topImage, bottomImage;
    juce::String shownPresetName;
    int shownPresetCount = -1;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> groupLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Faceplate)
};

class VoxSlapEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit VoxSlapEditor (VoxSlapProcessor&);
    ~VoxSlapEditor() override;

    void timerCallback() override;
    void resized() override {}

private:
    void applyLayout();

    Faceplate faceplate;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxSlapEditor)
};
