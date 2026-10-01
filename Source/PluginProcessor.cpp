#include "PluginProcessor.h"
#include "PluginEditor.h"

VoxSlapProcessor::VoxSlapProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VoxSlap", createParameterLayout())
{
    auto choice = [this] (const char* id) { return dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)); };
    auto boolean = [this] (const char* id) { return dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (id)); };
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    modeParam      = choice (ParamID::mode);
    divisionParam  = choice (ParamID::division);
    driveTypeParam = choice (ParamID::driveType);
    syncParam      = boolean (ParamID::sync);
    reverseParam   = boolean (ParamID::reverse);
    freezeParam    = boolean (ParamID::freeze);

    timeMs      = raw (ParamID::timeMs);
    feedback    = raw (ParamID::feedback);
    mix         = raw (ParamID::mix);
    hpf         = raw (ParamID::hpf);
    lpf         = raw (ParamID::lpf);
    drive       = raw (ParamID::drive);
    width       = raw (ParamID::width);
    offset      = raw (ParamID::offset);
    duck        = raw (ParamID::duck);
    duckThresh  = raw (ParamID::duckThresh);
    duckRelease = raw (ParamID::duckRelease);
    modRate     = raw (ParamID::modRate);
    modDepth    = raw (ParamID::modDepth);
    pitch       = raw (ParamID::pitch);
    output      = raw (ParamID::output);
}

bool VoxSlapProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;

    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void VoxSlapProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    // Reverse mode reads up to twice the delay time back, plus stereo offset and modulation.
    const int lineSize = (int) (sr * (2.0 * maxDelayMs + 100.0) / 1000.0);
    for (auto& c : channels)
    {
        c.line.prepare (lineSize);
        c.hp.reset();
        c.lp.reset();
        c.sat.reset();
        c.shifter.prepare (sr);
        c.reversePhase = 0.0f;
    }

    smoothedDelayL = smoothedDelayR = (float) (getCurrentDelayMs() * 0.001 * sr);
    lfoPhase = 0.0f;
    envelope = 0.0f;
    duckSmoothed = 1.0f;
    freezeAmount = freezeParam->get() ? 1.0f : 0.0f;
    reverseAmount = reverseParam->get() ? 1.0f : 0.0f;

    for (auto* s : { &mixSmoothed, &outSmoothed, &feedbackSmoothed, &makeupSmoothed, &widthSmoothed })
        s->reset (sr, 0.03);

    mixSmoothed.setCurrentAndTargetValue (mix->load() / 100.0f);
    outSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (output->load()));
    feedbackSmoothed.setCurrentAndTargetValue (feedback->load() / 100.0f);
    makeupSmoothed.setCurrentAndTargetValue (fx::Saturator::makeup (driveTypeParam->getIndex(), drive->load() / 100.0f));
    widthSmoothed.setCurrentAndTargetValue (width->load() / 100.0f);

    scopeChunk = juce::jmax (1, (int) (sr * 0.01));
    scopeCounter = 0;
    scopeDryMax = scopeWetMax = 0.0f;
}

double VoxSlapProcessor::getCurrentDelayMs() const
{
    double ms = timeMs->load();
    if (syncParam->get())
        ms = 60000.0 / juce::jmax (20.0, hostBpm.load()) * divisionBeats (divisionParam->getIndex());
    return juce::jlimit (1.0, maxDelayMs, ms);
}

float VoxSlapProcessor::processEchoChain (Channel& c, float x, int driveType, float drive01, float pitchRatio, bool shiftPitch)
{
    x = c.hp.highpass (x);
    x = c.lp.lowpass (x);
    if (shiftPitch)
        x = c.shifter.process (x, pitchRatio);
    x = c.sat.process (x, driveType, drive01);
    return std::isfinite (x) ? x : 0.0f;
}

float VoxSlapProcessor::readTap (Channel& c, float delaySamples, float reverseLength)
{
    const float normal = reverseAmount < 1.0f ? c.line.read (delaySamples) : 0.0f;
    if (reverseAmount <= 0.0f)
        return normal;

    // Reverse: two overlapping, Hann-windowed read heads sweeping backwards through the last
    // `reverseLength` samples. Each head's delay grows by 2 samples per sample => backwards playback.
    const float len = juce::jmax (64.0f, reverseLength);
    c.reversePhase += 1.0f;
    if (c.reversePhase >= len)
        c.reversePhase = std::fmod (c.reversePhase, len);

    const float pA = c.reversePhase / len;
    const float pB = std::fmod (pA + 0.5f, 1.0f);
    const float gA = std::sin (fx::pi * pA), gB = std::sin (fx::pi * pB);
    const float rev = gA * gA * c.line.read (2.0f * pA * len + 32.0f)
                    + gB * gB * c.line.read (2.0f * pB * len + 32.0f);

    return normal + reverseAmount * (rev - normal);
}

void VoxSlapProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    if (numIn == 1 && numOut > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);
    else
        for (int ch = numIn; ch < numOut; ++ch)
            buffer.clear (ch, 0, numSamples);

    if (auto* playHead = getPlayHead())
        if (auto pos = playHead->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 0.0)
                    hostBpm.store (*bpm);

    // ---- Block-rate parameter setup ----
    const auto mode = (EchoMode) modeParam->getIndex();
    const float baseDelay = (float) (getCurrentDelayMs() * 0.001 * sr);
    const float offsetSamples = mode == EchoMode::wide ? offset->load() * 0.001f * (float) sr : 0.0f;
    const float targetL = baseDelay;
    const float targetR = baseDelay + offsetSamples;

    for (auto& c : channels)
    {
        c.hp.set (hpf->load(), (float) sr);
        c.lp.set (lpf->load(), (float) sr);
    }

    const int driveType = driveTypeParam->getIndex();
    const float drive01 = drive->load() / 100.0f;
    const float pitchSemis = pitch->load();
    const float pitchRatio = std::pow (2.0f, pitchSemis / 12.0f);
    const bool shiftPitch = std::abs (pitchSemis) > 0.05f;

    feedbackSmoothed.setTargetValue (feedback->load() / 100.0f);
    makeupSmoothed.setTargetValue (fx::Saturator::makeup (driveType, drive01));
    mixSmoothed.setTargetValue (mix->load() / 100.0f);
    outSmoothed.setTargetValue (juce::Decibels::decibelsToGain (output->load()));
    widthSmoothed.setTargetValue (width->load() / 100.0f);

    const float duckAmount = duck->load() / 100.0f;
    const float threshDb = duckThresh->load();
    // Fast detector tells whether the voice is present; the gain then recovers over the release time.
    const float envAttackCoef = std::exp (-1.0f / (0.002f * (float) sr));
    const float envReleaseCoef = std::exp (-1.0f / (0.025f * (float) sr));
    const float duckAttack = 1.0f - std::exp (-1.0f / (0.008f * (float) sr));
    const float duckReleaseStep = 1.0f - std::exp (-1.0f / (duckRelease->load() * 0.001f * 0.3f * (float) sr));

    const float modDepthSamples = modDepth->load() / 100.0f * 0.003f * (float) sr;
    const float lfoInc = 2.0f * fx::pi * modRate->load() / (float) sr;
    const float rightLfoOffset = mode == EchoMode::wide ? fx::pi * 0.5f : 0.0f;

    const float delaySmoothCoef = 1.0f - std::exp (-1.0f / (0.06f * (float) sr));
    const float rampStep = 1.0f / (0.02f * (float) sr);
    const float freezeTarget = freezeParam->get() ? 1.0f : 0.0f;
    const float reverseTarget = reverseParam->get() ? 1.0f : 0.0f;

    float* left = buffer.getWritePointer (0);
    float* right = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    float inPeak = 0.0f, outPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = left[i];
        const float inR = right != nullptr ? right[i] : inL;
        const float mono = 0.5f * (inL + inR);
        inPeak = juce::jmax (inPeak, std::abs (inL), std::abs (inR));

        // ---- Ducking: how much the echoes hide under the voice ----
        const float level = std::abs (mono);
        envelope = level + (level > envelope ? envAttackCoef : envReleaseCoef) * (envelope - level);
        const float envDb = juce::Decibels::gainToDecibels (envelope, -100.0f);
        const float amountOver = juce::jlimit (0.0f, 1.0f, (envDb - (threshDb - 9.0f)) / 9.0f);
        const float duckTarget = 1.0f - duckAmount * amountOver;
        duckSmoothed += (duckTarget - duckSmoothed) * (duckTarget < duckSmoothed ? duckAttack : duckReleaseStep);

        // ---- Delay time with tape-like glide and wow/flutter ----
        smoothedDelayL += (targetL - smoothedDelayL) * delaySmoothCoef;
        smoothedDelayR += (targetR - smoothedDelayR) * delaySmoothCoef;

        lfoPhase += lfoInc;
        if (lfoPhase > 2.0f * fx::pi)
            lfoPhase -= 2.0f * fx::pi;

        const float modL = std::sin (lfoPhase) + 0.3f * std::sin (2.7f * lfoPhase + 1.0f);
        const float modR = std::sin (lfoPhase + rightLfoOffset) + 0.3f * std::sin (2.7f * (lfoPhase + rightLfoOffset) + 1.0f);
        const float dL = smoothedDelayL + modDepthSamples * (1.3f + modL);
        const float dR = smoothedDelayR + modDepthSamples * (1.3f + modR);

        freezeAmount = freezeAmount < freezeTarget ? juce::jmin (freezeTarget, freezeAmount + rampStep)
                                                   : juce::jmax (freezeTarget, freezeAmount - rampStep);
        reverseAmount = reverseAmount < reverseTarget ? juce::jmin (reverseTarget, reverseAmount + rampStep)
                                                      : juce::jmax (reverseTarget, reverseAmount - rampStep);

        const float tapL = readTap (channels[0], dL, smoothedDelayL);
        const float tapR = readTap (channels[1], dR, smoothedDelayR);

        // ---- Feedback routing ----
        const float fb = feedbackSmoothed.getNextValue();
        float xL, xR, frozenL, frozenR;
        if (mode == EchoMode::pingPong)
        {
            xL = mono + fb * tapR;
            xR = fb * tapL;
            frozenL = tapR;
            frozenR = tapL;
        }
        else
        {
            xL = inL + fb * tapL;
            xR = inR + fb * tapR;
            frozenL = tapL;
            frozenR = tapR;
        }

        float writeL = processEchoChain (channels[0], xL, driveType, drive01, pitchRatio, shiftPitch);
        float writeR = processEchoChain (channels[1], xR, driveType, drive01, pitchRatio, shiftPitch);

        if (freezeAmount > 0.0f)
        {
            writeL += freezeAmount * (frozenL - writeL);
            writeR += freezeAmount * (frozenR - writeR);
        }

        channels[0].line.push (writeL);
        channels[1].line.push (writeR);

        // ---- Wet output: makeup, stereo width, ducking ----
        const float makeup = makeupSmoothed.getNextValue();
        float wetL = tapL * makeup, wetR = tapR * makeup;

        const float w = widthSmoothed.getNextValue();
        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * w;
        wetL = (mid + side) * duckSmoothed;
        wetR = (mid - side) * duckSmoothed;

        const float m = mixSmoothed.getNextValue();
        const float dryGain = juce::jmin (1.0f, 2.0f * (1.0f - m));
        const float wetGain = juce::jmin (1.0f, 2.0f * m);
        const float outGain = outSmoothed.getNextValue();

        const float oL = (inL * dryGain + wetL * wetGain) * outGain;
        const float oR = (inR * dryGain + wetR * wetGain) * outGain;

        if (right != nullptr)
        {
            left[i] = oL;
            right[i] = oR;
        }
        else
        {
            left[i] = 0.5f * (oL + oR);
        }

        outPeak = juce::jmax (outPeak, std::abs (oL), std::abs (oR));

        // ---- Scope history ----
        scopeDryMax = juce::jmax (scopeDryMax, std::abs (mono));
        scopeWetMax = juce::jmax (scopeWetMax, std::abs (0.5f * (wetL + wetR)) * wetGain);
        if (++scopeCounter >= scopeChunk)
        {
            scope.push (scopeDryMax, scopeWetMax);
            scopeCounter = 0;
            scopeDryMax = scopeWetMax = 0.0f;
        }
    }

    inputPeak.store (juce::jmax (inputPeak.load(), inPeak));
    outputPeak.store (juce::jmax (outputPeak.load(), outPeak));
    duckGain.store (duckSmoothed);
}

int VoxSlapProcessor::getNumPrograms() { return 1; }
int VoxSlapProcessor::getCurrentProgram() { return 0; }
void VoxSlapProcessor::setCurrentProgram (int) {}
const juce::String VoxSlapProcessor::getProgramName (int) { return presets.getCurrentName(); }

void VoxSlapProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void VoxSlapProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VoxSlapProcessor::createEditor()
{
    return new VoxSlapEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxSlapProcessor();
}
