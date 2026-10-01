#include "Presets.h"
#include "Params.h"

using namespace ParamID;

// Choice parameters take the item index; bools take 0/1. Anything not listed uses its default.
// mode: 0 Slap, 1 Ping-Pong, 2 Wide.   driveType: 0 Tape, 1 Tube, 2 Fuzz, 3 Lo-Fi.
// division: 0 1/32, 1 1/16T, 2 1/16, 3 1/16D, 4 1/8T, 5 1/8, 6 1/8D, 7 1/4T, 8 1/4, 9 1/4D, 10 1/2.
const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets
    {
        { "Init", {} },

        { "Cupsize Slap", { { mode, 0 }, { ParamID::sync, 1 }, { division, 2 }, { feedback, 18 }, { mix, 38 },
                            { hpf, 420 }, { lpf, 4800 }, { drive, 35 }, { driveType, 0 },
                            { width, 120 }, { duck, 45 }, { duckThresh, -32 }, { duckRelease, 180 },
                            { modRate, 0.7f }, { modDepth, 12 } } },

        { "Classic 50s Slapback", { { mode, 0 }, { ParamID::sync, 0 }, { timeMs, 115 }, { feedback, 8 }, { mix, 32 },
                                    { hpf, 150 }, { lpf, 5500 }, { drive, 18 }, { driveType, 0 },
                                    { width, 60 }, { duck, 0 }, { modDepth, 6 } } },

        { "Phone Slap", { { mode, 0 }, { ParamID::sync, 1 }, { division, 2 }, { feedback, 22 }, { mix, 35 },
                          { hpf, 900 }, { lpf, 3000 }, { drive, 55 }, { driveType, 2 },
                          { width, 80 }, { duck, 35 } } },

        { "Ping-Pong Eighths", { { mode, 1 }, { ParamID::sync, 1 }, { division, 5 }, { feedback, 38 }, { mix, 30 },
                                 { hpf, 300 }, { lpf, 7000 }, { drive, 15 }, { width, 130 },
                                 { duck, 60 }, { duckRelease, 300 } } },

        { "Ping-Pong Sixteenths", { { mode, 1 }, { ParamID::sync, 1 }, { division, 2 }, { feedback, 30 }, { mix, 28 },
                                    { hpf, 400 }, { lpf, 6000 }, { drive, 25 }, { width, 150 },
                                    { duck, 50 } } },

        { "Wide Double", { { mode, 2 }, { ParamID::sync, 0 }, { timeMs, 28 }, { offset, 14 }, { feedback, 0 }, { mix, 35 },
                           { hpf, 200 }, { lpf, 12000 }, { drive, 0 }, { width, 170 },
                           { duck, 0 }, { modRate, 1.2f }, { modDepth, 18 } } },

        { "Lo-Fi Tape Echo", { { mode, 0 }, { ParamID::sync, 1 }, { division, 6 }, { feedback, 42 }, { mix, 30 },
                               { hpf, 350 }, { lpf, 3500 }, { drive, 45 }, { driveType, 3 },
                               { duck, 50 }, { modRate, 0.6f }, { modDepth, 40 } } },

        { "Dark Throw (After Only)", { { mode, 1 }, { ParamID::sync, 1 }, { division, 8 }, { feedback, 55 }, { mix, 42 },
                                       { hpf, 250 }, { lpf, 2500 }, { drive, 30 }, { driveType, 0 },
                                       { width, 140 }, { duck, 100 }, { duckThresh, -36 }, { duckRelease, 350 } } },

        { "Radio Ghost", { { mode, 0 }, { ParamID::sync, 1 }, { division, 4 }, { feedback, 30 }, { mix, 32 },
                           { hpf, 700 }, { lpf, 2800 }, { drive, 60 }, { driveType, 1 },
                           { duck, 70 }, { modDepth, 15 } } },

        { "Reverse Swell", { { mode, 2 }, { ParamID::sync, 1 }, { division, 8 }, { feedback, 30 }, { mix, 35 },
                             { hpf, 300 }, { lpf, 6000 }, { drive, 10 }, { reverse, 1 },
                             { width, 150 }, { duck, 50 } } },

        { "Octave Up Echo", { { mode, 1 }, { ParamID::sync, 1 }, { division, 5 }, { feedback, 40 }, { mix, 25 },
                              { hpf, 500 }, { lpf, 8000 }, { drive, 10 }, { pitch, 12 },
                              { width, 140 }, { duck, 60 } } },

        { "Fuzz Slap", { { mode, 0 }, { ParamID::sync, 0 }, { timeMs, 90 }, { feedback, 15 }, { mix, 30 },
                         { hpf, 500 }, { lpf, 4500 }, { drive, 70 }, { driveType, 2 }, { duck, 30 } } },

        { "Subtle Vocal Space", { { mode, 0 }, { ParamID::sync, 0 }, { timeMs, 80 }, { feedback, 5 }, { mix, 16 },
                                  { hpf, 500 }, { lpf, 8000 }, { drive, 10 }, { width, 140 },
                                  { duck, 20 } } },

        { "Dub Feedback", { { mode, 1 }, { ParamID::sync, 1 }, { division, 6 }, { feedback, 80 }, { mix, 35 },
                            { hpf, 300 }, { lpf, 3000 }, { drive, 40 }, { driveType, 0 },
                            { width, 160 }, { duck, 55 }, { modRate, 0.4f }, { modDepth, 25 } } },

        { "Tape Wobble", { { mode, 0 }, { ParamID::sync, 1 }, { division, 2 }, { feedback, 25 }, { mix, 30 },
                           { hpf, 300 }, { lpf, 5000 }, { drive, 30 }, { driveType, 0 },
                           { duck, 40 }, { modRate, 2.5f }, { modDepth, 70 } } },
    };
    return presets;
}

static const juce::Identifier presetNameProp { "presetName" };
static const juce::Identifier advancedProp { "advanced" };

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s)
{
    getUserPresetFolder().createDirectory();
}

juce::File PresetManager::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("VoxSlap").getChildFile ("Presets");
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    for (auto& f : getUserPresetFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

juce::StringArray PresetManager::getAllPresetNames() const
{
    juce::StringArray names;
    for (auto& p : getFactoryPresets())
        names.add (p.name);
    names.addArray (getUserPresetNames());
    return names;
}

void PresetManager::setParam (const juce::String& id, float realValue)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void PresetManager::resetToDefaults()
{
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            setParam (rp->getParameterID(), rp->convertFrom0to1 (rp->getDefaultValue()));
}

void PresetManager::loadFactoryPreset (int index)
{
    auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    resetToDefaults();
    for (auto& [id, value] : presets[(size_t) index].values)
        setParam (id, value);

    setCurrentName (presets[(size_t) index].name);
}

bool PresetManager::loadUserPreset (const juce::String& name)
{
    auto file = getUserPresetFolder().getChildFile (name + extension);
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return false;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType (state.state.getType()))
        return false;

    // Apply parameter values one by one so the host sees the changes.
    resetToDefaults();
    for (auto child : tree)
    {
        auto id = child.getProperty ("id").toString();
        if (auto* p = state.getParameter (id))
            setParam (id, p->convertFrom0to1 (p->convertTo0to1 ((float) child.getProperty ("value"))));
    }

    setCurrentName (name);
    return true;
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    auto copy = state.copyState();
    copy.removeProperty (advancedProp, nullptr);
    copy.removeProperty (presetNameProp, nullptr);

    auto xml = copy.createXml();
    auto folder = getUserPresetFolder();
    folder.createDirectory();
    if (xml == nullptr || ! xml->writeTo (folder.getChildFile (name + extension)))
        return false;

    setCurrentName (name);
    return true;
}

bool PresetManager::deleteUserPreset (const juce::String& name)
{
    auto file = getUserPresetFolder().getChildFile (name + extension);
    // Moved to the system trash rather than erased, so a mistaken delete can be undone.
    const bool ok = file.existsAsFile() && file.moveToTrash();
    if (ok)
        setCurrentName ("Init");
    return ok;
}

void PresetManager::loadByIndex (int index)
{
    const int numFactory = (int) getFactoryPresets().size();
    if (index < numFactory)
        loadFactoryPreset (index);
    else
    {
        auto users = getUserPresetNames();
        if (juce::isPositiveAndBelow (index - numFactory, users.size()))
            loadUserPreset (users[index - numFactory]);
    }
}

int PresetManager::getCurrentIndex() const
{
    return getAllPresetNames().indexOf (getCurrentName());
}

juce::String PresetManager::getCurrentName() const
{
    return state.state.getProperty (presetNameProp, "Init").toString();
}

void PresetManager::setCurrentName (const juce::String& name)
{
    state.state.setProperty (presetNameProp, name, nullptr);
}
