// Offline check: renders a synthetic "vocal" through every factory preset, reports levels,
// writes WAV files, and saves PNG screenshots of the editor.
//
// Usage: VoxSlapRender <outputDir> [input.wav]

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static juce::AudioBuffer<float> makeTestVocal (double sr)
{
    // Four short "syllables" (glottal-ish sawtooth through two formant bandpasses) with gaps.
    const int len = (int) (sr * 4.0);
    juce::AudioBuffer<float> b (2, len);
    b.clear();

    struct Syl { double start, dur, f0; };
    const Syl syls[] { { 0.10, 0.22, 220.0 }, { 0.40, 0.18, 247.0 }, { 0.65, 0.35, 196.0 }, { 2.0, 0.25, 262.0 } };

    juce::dsp::IIR::Filter<float> f1, f2;
    f1.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, 700.0, 3.0);
    f2.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, 1800.0, 4.0);

    double phase = 0.0;
    for (int i = 0; i < len; ++i)
    {
        const double t = i / sr;
        float env = 0.0f, f0 = 200.0f;
        for (auto& s : syls)
            if (t >= s.start && t < s.start + s.dur)
            {
                const double x = (t - s.start) / s.dur;
                env = (float) std::sin (juce::MathConstants<double>::pi * x);
                f0 = (float) s.f0;
            }
        phase += f0 / sr;
        phase -= std::floor (phase);
        const float saw = (float) (2.0 * phase - 1.0);
        const float v = (f1.processSample (saw) * 1.2f + f2.processSample (saw) * 0.6f) * env * 0.5f;
        b.setSample (0, i, v);
        b.setSample (1, i, v);
    }
    return b;
}

static float rmsDb (const juce::AudioBuffer<float>& b, double sr, double from, double to)
{
    const int s0 = (int) (from * sr), s1 = juce::jmin (b.getNumSamples(), (int) (to * sr));
    double sum = 0.0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = s0; i < s1; ++i)
            sum += b.getSample (ch, i) * b.getSample (ch, i);
    return juce::Decibels::gainToDecibels ((float) std::sqrt (sum / juce::jmax (1, (s1 - s0) * b.getNumChannels())), -120.0f);
}

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    auto writer = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr)
                                                                              .withNumChannels (b.getNumChannels())
                                                                              .withBitsPerSample (24));
    if (writer != nullptr)
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    double sr = 48000.0;
    juce::AudioBuffer<float> input;
    if (argc > 2)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (juce::File (argv[2])));
        if (reader == nullptr) { std::cerr << "Cannot read " << argv[2] << "\n"; return 1; }
        sr = reader->sampleRate;
        input.setSize (2, (int) reader->lengthInSamples + (int) (sr * 3));
        input.clear();
        reader->read (&input, 0, (int) reader->lengthInSamples, 0, true, true);
    }
    else
    {
        input = makeTestVocal (sr);
    }

    writeWav (outDir.getChildFile ("00_dry.wav"), input, sr);

    const int block = 512;
    bool allOk = true;
    const auto& presets = getFactoryPresets();

    std::cout << "preset                         peak dBFS   voice-section RMS   tail RMS (after voice)\n";
    for (int p = 0; p < (int) presets.size(); ++p)
    {
        VoxSlapProcessor proc;
        proc.setPlayConfigDetails (2, 2, sr, block);
        proc.presets.loadFactoryPreset (p);
        proc.prepareToPlay (sr, block);

        auto buffer = input;
        juce::MidiBuffer midi;
        for (int pos = 0; pos < buffer.getNumSamples(); pos += block)
        {
            const int n = juce::jmin (block, buffer.getNumSamples() - pos);
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, pos, n);
            proc.processBlock (view, midi);
        }

        bool finite = true;
        float peak = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float v = buffer.getSample (ch, i);
                finite &= std::isfinite (v);
                peak = juce::jmax (peak, std::abs (v));
            }

        const float peakDb = juce::Decibels::gainToDecibels (peak, -120.0f);
        allOk &= finite && peakDb < 6.0f;

        std::cout << juce::String (presets[(size_t) p].name).paddedRight (' ', 30) << " "
                  << juce::String (peakDb, 1).paddedLeft (' ', 8) << "   "
                  << juce::String (rmsDb (buffer, sr, 0.1, 1.0), 1).paddedLeft (' ', 10) << "        "
                  << juce::String (rmsDb (buffer, sr, 1.05, 1.9), 1).paddedLeft (' ', 10)
                  << (finite ? "" : "   NaN/Inf!") << "\n";

        writeWav (outDir.getChildFile (juce::String (p + 1).paddedLeft ('0', 2) + "_"
                                       + juce::File::createLegalFileName (presets[(size_t) p].name) + ".wav"),
                  buffer, sr);
    }

    // Editor screenshots (simple and advanced) with some audio in the meters.
    for (bool advanced : { false, true })
    {
        VoxSlapProcessor proc;
        proc.setPlayConfigDetails (2, 2, sr, block);
        proc.presets.loadFactoryPreset (1);
        proc.apvts.state.setProperty ("advanced", advanced, nullptr);
        proc.prepareToPlay (sr, block);

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        auto buffer = input;
        juce::MidiBuffer midi;
        for (int pos = 0; pos + block <= (int) (sr * 1.6); pos += block)
        {
            juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, pos, block);
            proc.processBlock (view, midi);
            if ((pos / block) % 3 == 0)
                if (auto* t = dynamic_cast<juce::Timer*> (editor.get()))
                    t->timerCallback();
        }

        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto file = outDir.getChildFile (advanced ? "ui_advanced.png" : "ui_simple.png");
        file.deleteFile();
        juce::FileOutputStream fos (file);
        juce::PNGImageFormat().writeImageToStream (image, fos);
    }

    std::cout << (allOk ? "ALL OK" : "PROBLEMS FOUND") << "\n";
    return allOk ? 0 : 1;
}
