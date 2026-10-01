#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "VintageLookAndFeel.h"

// Knob with an engraved title above and a small readout below.
class KnobControl : public juce::Component
{
public:
    KnobControl (juce::AudioProcessorValueTreeState&, const char* paramID, const juce::String& title, bool big = false);

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::String title;
    bool big;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Moving-coil VU meter with needle ballistics.
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

// Green-phosphor display: scrolling dry/wet history plus a map of where the repeats land.
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

    static constexpr int editorWidth = 980;
    static constexpr int simpleHeight = 540;
    static constexpr int advancedHeight = 750;

private:
    void timerCallback() override;
    void setAdvanced (bool);
    bool isAdvanced() const;
    void refreshPresetList();
    void savePresetDialog();
    void deletePresetDialog();
    void stepPreset (int delta);
    void setChoiceParam (const char* id, int index);
    void drawSection (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);

    VoxSlapProcessor& processor;
    Vintage::LookAndFeel lnf;

    // Header
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, deleteButton { "DEL" };
    juce::TextButton simpleButton { "SIMPLE" }, advancedButton { "ADVANCED" };

    // Display
    VUMeter inputMeter { "INPUT" }, outputMeter { "OUTPUT" };
    EchoScope scope;

    // Main controls
    std::array<juce::TextButton, 3> modeButtons;
    KnobControl timeKnob, divisionKnob, feedbackKnob, driveKnob, duckKnob, mixKnob;
    juce::ToggleButton syncToggle { "SYNC" };
    juce::ComboBox driveTypeBox;

    // Advanced controls
    KnobControl hpfKnob, lpfKnob, widthKnob, offsetKnob, modRateKnob, modDepthKnob,
                pitchKnob, threshKnob, releaseKnob, outputKnob;
    juce::ToggleButton reverseToggle { "REVERSE" }, freezeToggle { "FREEZE" };

    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<ButtonAttachment> syncAttachment, reverseAttachment, freezeAttachment;
    std::unique_ptr<ComboAttachment> driveTypeAttachment;

    juce::Image panelTexture;
    juce::String shownPresetName;
    int shownPresetCount = -1;

    // Section frames (computed in resized, drawn in paint)
    juce::Rectangle<int> displayArea, mainArea, advancedArea;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> sections;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxSlapEditor)
};
