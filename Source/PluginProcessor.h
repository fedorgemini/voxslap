#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP.h"
#include "Params.h"
#include "Presets.h"

// Lock-free hand-off of level history (dry/wet peaks every ~10 ms, plus beat marks) to the scope display.
class ScopeFifo
{
public:
    static constexpr int capacity = 1024;

    // beat: 0 = none, 1 = a beat started in this chunk, 2 = a bar started (only while the host plays)
    void push (float dry, float wet, float beat)
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) { dryBuf[(size_t) s1] = dry; wetBuf[(size_t) s1] = wet; beatBuf[(size_t) s1] = beat; }
        fifo.finishedWrite (n1 + n2);
    }

    template <typename Fn>
    void popAll (Fn&& fn)
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) fn (dryBuf[(size_t) (s1 + i)], wetBuf[(size_t) (s1 + i)], beatBuf[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) fn (dryBuf[(size_t) (s2 + i)], wetBuf[(size_t) (s2 + i)], beatBuf[(size_t) (s2 + i)]);
        fifo.finishedRead (n1 + n2);
    }

private:
    juce::AbstractFifo fifo { capacity };
    std::array<float, capacity> dryBuf {}, wetBuf {}, beatBuf {};
};

class VoxSlapProcessor : public juce::AudioProcessor
{
public:
    VoxSlapProcessor();
    ~VoxSlapProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Delay time in ms for the current settings (uses host tempo when synced).
    double getCurrentDelayMs() const;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets { apvts };

    // Metering for the UI.
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, duckGain { 1.0f };
    std::atomic<float> inputMeanSquare { 0.0f }, outputMeanSquare { 0.0f }; // ~50 ms averaged, for the VU needles
    std::atomic<double> hostBpm { 120.0 };
    ScopeFifo scope;

private:
    struct Channel
    {
        fx::DelayLine line;
        fx::SVF hp, lp;
        fx::Saturator sat;
        fx::PitchShifter shifter;
        float reversePhase = 0.0f;
    };

    float processEchoChain (Channel&, float x, int driveType, float drive01, float pitchRatio, bool shiftPitch);
    float readTap (Channel&, float delaySamples, float reverseLength);

    std::array<Channel, 2> channels;

    juce::AudioParameterChoice *modeParam, *divisionParam, *driveTypeParam;
    juce::AudioParameterBool *syncParam, *reverseParam, *freezeParam, *bypassParam;
    std::atomic<float> *timeMs, *feedback, *mix, *hpf, *lpf, *drive, *width, *offset,
                       *duck, *duckThresh, *duckRelease, *modRate, *modDepth, *pitch, *output;

    double sr = 44100.0;
    float smoothedDelayL = 0.0f, smoothedDelayR = 0.0f;
    float lfoPhase = 0.0f;
    float envelope = 0.0f, duckSmoothed = 1.0f;
    float freezeAmount = 0.0f, reverseAmount = 0.0f, bypassAmount = 0.0f;
    float inMs = 0.0f, outMs = 0.0f;
    juce::SmoothedValue<float> mixSmoothed, outSmoothed, feedbackSmoothed, makeupSmoothed, widthSmoothed;

    int scopeCounter = 0, scopeChunk = 441;
    float scopeDryMax = 0.0f, scopeWetMax = 0.0f;
    juce::int64 lastBeatIndex = -1, lastBarIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxSlapProcessor)
};
