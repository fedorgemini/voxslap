#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamID
{
    inline constexpr const char* mode        = "mode";
    inline constexpr const char* sync        = "sync";
    inline constexpr const char* timeMs      = "timeMs";
    inline constexpr const char* division    = "division";
    inline constexpr const char* feedback    = "feedback";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* hpf         = "hpf";
    inline constexpr const char* lpf         = "lpf";
    inline constexpr const char* drive       = "drive";
    inline constexpr const char* driveType   = "driveType";
    inline constexpr const char* width       = "width";
    inline constexpr const char* offset      = "offset";
    inline constexpr const char* duck        = "duck";
    inline constexpr const char* duckThresh  = "duckThresh";
    inline constexpr const char* duckRelease = "duckRelease";
    inline constexpr const char* modRate     = "modRate";
    inline constexpr const char* modDepth    = "modDepth";
    inline constexpr const char* pitch       = "pitch";
    inline constexpr const char* reverse     = "reverse";
    inline constexpr const char* freeze      = "freeze";
    inline constexpr const char* output      = "output";
}

enum class EchoMode { slap = 0, pingPong, wide };
enum class DriveType { tape = 0, tube, fuzz, lofi };

inline const juce::StringArray& modeNames()
{
    static const juce::StringArray names { "Slap", "Ping-Pong", "Wide" };
    return names;
}

inline const juce::StringArray& driveTypeNames()
{
    static const juce::StringArray names { "Tape", "Tube", "Fuzz", "Lo-Fi" };
    return names;
}

inline const juce::StringArray& divisionNames()
{
    static const juce::StringArray names { "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
                                           "1/8D", "1/4T", "1/4", "1/4D", "1/2" };
    return names;
}

// Length of each division in quarter-note beats.
inline double divisionBeats (int index)
{
    static const double beats[] { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5,
                                  0.75, 2.0 / 3.0, 1.0, 1.5, 2.0 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

inline constexpr double maxDelayMs = 2000.0;

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    using APF = AudioParameterFloat;
    using Attr = AudioParameterFloatAttributes;

    auto pct = Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; })
                     .withLabel ("%");
    auto hz = Attr().withStringFromValueFunction ([] (float v, int)
    {
        return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz";
    });
    auto ms = Attr().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; });
    auto db = Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });

    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::mode, 1 }, "Mode", modeNames(), 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::sync, 1 }, "Sync", true));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::timeMs, 1 }, "Time", skewed (10.0f, (float) maxDelayMs, 200.0f), 110.0f, ms));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::division, 1 }, "Division", divisionNames(), 2));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::feedback, 1 }, "Feedback", NormalisableRange<float> (0.0f, 95.0f), 20.0f, pct));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f), 30.0f, pct));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::hpf, 1 }, "Low Cut", skewed (20.0f, 2000.0f, 300.0f), 250.0f, hz));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::lpf, 1 }, "High Cut", skewed (1000.0f, 20000.0f, 5000.0f), 6000.0f, hz));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::drive, 1 }, "Drive", NormalisableRange<float> (0.0f, 100.0f), 20.0f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::driveType, 1 }, "Drive Type", driveTypeNames(), 0));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::width, 1 }, "Width", NormalisableRange<float> (0.0f, 200.0f), 100.0f, pct));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::offset, 1 }, "Stereo Offset", NormalisableRange<float> (0.0f, 40.0f), 12.0f, ms));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::duck, 1 }, "Under/After", NormalisableRange<float> (0.0f, 100.0f), 30.0f, pct));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::duckThresh, 1 }, "Duck Threshold", NormalisableRange<float> (-60.0f, 0.0f), -30.0f, db));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::duckRelease, 1 }, "Duck Release", skewed (20.0f, 1000.0f, 200.0f), 250.0f, ms));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::modRate, 1 }, "Mod Rate", skewed (0.1f, 8.0f, 1.0f), 0.8f,
                                       Attr().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::modDepth, 1 }, "Mod Depth", NormalisableRange<float> (0.0f, 100.0f), 10.0f, pct));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::pitch, 1 }, "Pitch", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                       Attr().withStringFromValueFunction ([] (float v, int)
                                       {
                                           return (v > 0.0f ? "+" : "") + String (v, 1) + " st";
                                       })));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::reverse, 1 }, "Reverse", false));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { ParamID::freeze, 1 }, "Freeze", false));
    layout.add (std::make_unique<APF> (ParameterID { ParamID::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f), 0.0f, db));

    return layout;
}
