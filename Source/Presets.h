#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values; // parameter ID -> real (not normalised) value
};

const std::vector<FactoryPreset>& getFactoryPresets();

// Handles factory presets (built in) and user presets (XML files in Documents/VoxSlap/Presets).
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState&);

    static juce::File getUserPresetFolder();
    static constexpr const char* extension = ".vspreset";

    juce::StringArray getUserPresetNames() const;

    void loadFactoryPreset (int index);
    bool loadUserPreset (const juce::String& name);
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);

    // Index into the combined list: factory presets first, then user presets.
    juce::StringArray getAllPresetNames() const;
    void loadByIndex (int index);
    int getCurrentIndex() const;

    juce::String getCurrentName() const;

private:
    void setCurrentName (const juce::String&);
    void resetToDefaults();
    void setParam (const juce::String& id, float realValue);

    juce::AudioProcessorValueTreeState& state;
};
