#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "AnalogLookAndFeel.h"

enum class KnobStyle { aluminium, black, chicken };

// Knob with a silkscreened title; the value appears in a small readout while hovering/dragging.
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

    juce::Slider slider;

private:
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Back-lit moving-coil VU meter.
class VUMeter : public juce::Component
{
public:
    explicit VUMeter (juce::String label) : label (std::move (label)) {}

    void setPeak (float linearPeak);
    void paint (juce::Graphics&) override;

private:
    juce::String label;
    float needle = 0.0f, peakLamp = 0.0f;
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
        bool sync = true, reverse = false, freeze = false;
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

class VoxSlapEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit VoxSlapEditor (VoxSlapProcessor&);
    ~VoxSlapEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    static constexpr int editorWidth = 1000;
    static constexpr int cheekWidth = 26;
    static constexpr int topUnitHeight = 540;
    static constexpr int bottomUnitHeight = 196;

private:
    void setAdvanced (bool);
    bool isAdvanced() const;
    void refreshPresetList();
    void savePresetDialog();
    void deletePresetDialog();
    void stepPreset (int delta);
    void buildTextures();
    void paintUnit (juce::Graphics&, juce::Rectangle<int>, const juce::Image& texture);
    void paintGroupLabel (juce::Graphics&, juce::Rectangle<int> span, const juce::String& title);

    VoxSlapProcessor& processor;
    Analog::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // Name plate
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, deleteButton { "DEL" };
    juce::TextButton simpleButton { "SIMPLE" }, advancedButton { "ADVANCED" };

    // Meter bridge
    VUMeter inputMeter { "INPUT" }, outputMeter { "OUTPUT" };
    EchoScope scope;

    // Main unit
    KnobControl modeKnob, timeKnob, divisionKnob, feedbackKnob, driveKnob, driveTypeKnob, duckKnob, mixKnob;
    juce::ToggleButton syncToggle { "SYNC" };

    // Advanced unit
    KnobControl hpfKnob, lpfKnob, widthKnob, offsetKnob, modRateKnob, modDepthKnob,
                pitchKnob, threshKnob, releaseKnob, outputKnob;
    juce::ToggleButton reverseToggle { "REVERSE" }, freezeToggle { "FREEZE" };

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ButtonAttachment> syncAttachment, reverseAttachment, freezeAttachment;

    juce::Image woodTexture, burgundyTexture, blackTexture, plateTexture;
    juce::String shownPresetName;
    int shownPresetCount = -1;

    juce::Rectangle<int> topUnit, bottomUnit, namePlate;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> groupLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxSlapEditor)
};
