#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "AnalogLookAndFeel.h"
#include <deque>

enum class KnobStyle { aluminium, black, chicken };

// Slider with an optional centre detent and a right-click hook.
class DetentSlider : public juce::Slider
{
public:
    void setDetent (double value) { detent = value; hasDetent = true; }
    double snapValue (double attempted, DragMode mode) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onRightClick;

private:
    double detent = 0.0;
    bool hasDetent = false, rightClicked = false;
};

// Knob with a silkscreened title. While it is touched, its name and value are shown on the
// echo scope (onReadout). Shift-drag = fine, double-click / Alt-click = default, right-click = menu.
class KnobControl : public juce::Component
{
public:
    KnobControl (juce::AudioProcessorValueTreeState&, const char* paramID, const juce::String& title,
                 KnobStyle style, const juce::String& scaleLabels = {});

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { announce (false); }
    void mouseDown (const juce::MouseEvent&) override;
    void tick (float dt);

    DetentSlider slider;
    std::function<void (const juce::String& title, const juce::String& value, bool changed)> onReadout;

private:
    void announce (bool changed);
    void showMenu();
    void enterValueDialog();

    juce::String title;
    double defaultValue = 0.0;
    bool stepped = false;
    juce::AudioParameterChoice* choiceParam = nullptr;
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

// Green-phosphor CRT: scrolling voice/echo history with afterglow, beat grid, a map of where the
// repeats land (repeats flash as they sound; drag to change time/feedback) and a readout of the
// knob being touched. Warms up / collapses to a dot with the POWER switch.
class EchoScope : public juce::Component, public juce::SettableTooltipClient
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

    static constexpr int historySize = 300;   // 10 ms chunks = 3 s

    void pushHistory (float dry, float wet, float beat);
    void setInfo (const Info& i) { info = i; }
    void showReadout (const juce::String& title, const juce::String& value, bool changed);
    void tick (float dt);
    void paint (juce::Graphics&) override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onDragStart, onDragEnd;
    std::function<void (double delayMs, float feedback01)> onDrag;

private:
    struct Tap { double timeMs; float amp; int side; }; // side: 0 both, 1 left, 2 right
    std::vector<Tap> computeTaps() const;
    double mapRange (const std::vector<Tap>&) const;
    juce::Rectangle<float> screenArea() const;
    juce::Rectangle<float> historyArea() const;
    juce::Rectangle<float> mapArea() const;
    void paintTrace (juce::Graphics&, juce::Rectangle<float>);
    void paintEchoMap (juce::Graphics&, juce::Rectangle<float>);
    void paintReadout (juce::Graphics&, juce::Rectangle<float>);

    std::array<float, historySize> dryHistory {}, wetHistory {}, beatHistory {};
    int writeIndex = 0;
    juce::int64 chunkClock = 0, lastOnset = -1000;
    float slowDry = 0.0f;
    std::vector<juce::int64> onsets;
    Info info;

    std::deque<juce::Path> ghosts;   // previous frames of the echo trace: phosphor afterglow
    juce::Path buildShape (const std::array<float, historySize>&, juce::Rectangle<float>) const;
    juce::String readoutTitle, readoutValue;
    float readoutAlpha = 0.0f, powerAnim = 1.0f, blink = 0.0f;

    bool dragging = false;
    double dragRange = 300.0;
    float dragStartFeedback = 0.0f;
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
    std::vector<KnobControl*> allKnobs()
    {
        return { &modeKnob, &timeKnob, &divisionKnob, &feedbackKnob, &driveKnob, &driveTypeKnob, &duckKnob, &mixKnob,
                 &hpfKnob, &lpfKnob, &widthKnob, &offsetKnob, &modRateKnob, &modDepthKnob,
                 &pitchKnob, &threshKnob, &releaseKnob, &outputKnob };
    }

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
