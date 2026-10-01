#include "PluginEditor.h"

using namespace Vintage;

static const juce::Identifier advancedProp { "advanced" };

//==============================================================================
KnobControl::KnobControl (juce::AudioProcessorValueTreeState& state, const char* paramID,
                          const juce::String& t, bool isBig)
    : title (t), big (isBig)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (big ? 260 : 200);
    slider.setVelocityBasedMode (false);

    auto* param = state.getParameter (paramID);
    if (dynamic_cast<juce::AudioParameterChoice*> (param) != nullptr)
        slider.getProperties().set ("stepped", true);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}

void KnobControl::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (big ? 22 : 18);
    r.removeFromBottom (big ? 24 : 20);
    slider.setBounds (r);
}

void KnobControl::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (Palette::ink);
    g.setFont (font (big ? 13.5f : 11.5f));
    g.drawText (title, r.removeFromTop (big ? 22 : 18), juce::Justification::centred);

    auto valueArea = r.removeFromBottom (big ? 24 : 20).toFloat();
    const float plateW = juce::jmin (valueArea.getWidth() - 4.0f, big ? 84.0f : 70.0f);
    auto plate = valueArea.withSizeKeepingCentre (plateW, big ? 19.0f : 16.0f);
    g.setColour (Palette::headerEdge);
    g.fillRoundedRectangle (plate, 2.5f);
    g.setColour (Palette::crtBack);
    g.fillRoundedRectangle (plate.reduced (1.5f), 2.0f);
    g.setColour (Palette::amber);
    g.setFont (mono (big ? 12.5f : 11.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()), plate, juce::Justification::centred);
}

//==============================================================================
void VUMeter::setPeak (float linearPeak)
{
    // 0 VU calibrated to -12 dBFS peak; the scale is linear in voltage like a real VU.
    const float vuDb = juce::Decibels::gainToDecibels (linearPeak, -60.0f) + 12.0f;
    const float target = juce::jlimit (0.0f, 1.08f, std::pow (10.0f, vuDb / 20.0f) / 1.4125f);
    needle += (target - needle) * (target > needle ? 0.35f : 0.12f);
    peakLamp = linearPeak > 0.89f ? 1.0f : peakLamp * 0.9f;
    repaint();
}

void VUMeter::paint (juce::Graphics& g)
{
    using namespace juce;
    auto bounds = getLocalBounds().toFloat();

    // Bezel
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 8.0f);
    g.setGradientFill (ColourGradient (juce::Colour (0xff3b3631), 0.0f, bounds.getY(),
                                       juce::Colour (0xff171411), 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 8.0f);

    auto face = bounds.reduced (10.0f);
    g.setGradientFill (ColourGradient (Palette::vuFace.brighter (0.1f), face.getCentreX(), face.getY(),
                                       Palette::vuFace.darker (0.18f), face.getCentreX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 4.0f);

    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (face.toNearestInt());

    // Wide, flat arc like a real VU: the pivot sits well below the visible face.
    const float arcTop = face.getY() + 44.0f;
    const Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.4f);
    const float radius = pivot.y - arcTop;
    const float maxAngle = std::asin (jmin (0.95f, (face.getWidth() * 0.5f - 16.0f) / radius));
    auto angleFor = [maxAngle] (float pos) { return -maxAngle + 2.0f * maxAngle * pos; };
    auto pointAt = [&] (float angle, float r) { return Point<float> (pivot.x + r * std::sin (angle), pivot.y - r * std::cos (angle)); };
    auto posForDb = [] (float db) { return std::pow (10.0f, db / 20.0f) / 1.4125f; };

    // Scale arc (black up to 0 VU, red above)
    Path blackArc, redArc;
    blackArc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleFor (posForDb (-20.0f)), angleFor (posForDb (0.0f)), true);
    redArc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleFor (posForDb (0.0f)), angleFor (1.0f), true);
    g.setColour (Palette::ink);
    g.strokePath (blackArc, PathStrokeType (1.6f));
    g.setColour (Palette::lampRed);
    g.strokePath (redArc, PathStrokeType (4.0f));

    g.setFont (font (10.0f));
    for (float db : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
    {
        const float a = angleFor (posForDb (db));
        g.setColour (db > 0.0f ? Palette::lampRed : Palette::ink);
        g.drawLine (Line<float> (pointAt (a, radius), pointAt (a, radius + 7.0f)), 1.4f);
        if (db == -20.0f || db == -10.0f || db == -5.0f || db == -3.0f || db == 0.0f || db == 3.0f)
        {
            const auto text = (db > 0.0f ? "+" : "") + String ((int) db);
            g.drawText (text, Rectangle<float> (26.0f, 12.0f).withCentre (pointAt (a, radius + 16.0f)), Justification::centred);
        }
    }

    g.setColour (Palette::ink);
    g.setFont (font (22.0f));
    g.drawText ("VU", Rectangle<float> (face.getX(), face.getBottom() - 54.0f, face.getWidth(), 26.0f), Justification::centred);
    g.setFont (font (10.0f));
    g.setColour (Palette::inkSoft);
    g.drawText (label, Rectangle<float> (face.getX(), face.getBottom() - 28.0f, face.getWidth(), 14.0f), Justification::centred);

    // Needle
    const float a = angleFor (needle);
    g.setColour (juce::Colours::black.withAlpha (0.2f));
    g.drawLine (Line<float> (pivot.translated (2.0f, 2.0f), pointAt (a, radius + 10.0f).translated (2.0f, 2.0f)), 2.0f);
    g.setColour (juce::Colour (0xff1a1410));
    g.drawLine (Line<float> (pivot, pointAt (a, radius + 10.0f)), 1.6f);

    // Glass reflection
    g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.22f), face.getX(), face.getY(),
                                       juce::Colours::white.withAlpha (0.0f), face.getX(), face.getCentreY(), false));
    g.fillRect (face.withHeight (face.getHeight() * 0.5f));

    drawLamp (g, { face.getRight() - 12.0f, face.getY() + 12.0f }, 4.0f, Palette::lampRed, peakLamp);
}

//==============================================================================
void EchoScope::pushHistory (float dry, float wet)
{
    dryHistory[(size_t) writeIndex] = dry;
    wetHistory[(size_t) writeIndex] = wet;
    writeIndex = (writeIndex + 1) % historySize;
}

void EchoScope::paint (juce::Graphics& g)
{
    using namespace juce;
    ++frameCounter;
    auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 8.0f);
    g.setGradientFill (ColourGradient (juce::Colour (0xff3b3631), 0.0f, bounds.getY(),
                                       juce::Colour (0xff171411), 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 8.0f);

    auto screen = bounds.reduced (10.0f);
    g.setColour (Palette::crtBack);
    g.fillRoundedRectangle (screen, 10.0f);

    {
        Graphics::ScopedSaveState save (g);
        Path clip;
        clip.addRoundedRectangle (screen, 10.0f);
        g.reduceClipRegion (clip);

        // Phosphor glow in the middle of the tube
        g.setGradientFill (ColourGradient (Palette::phosphor.withAlpha (0.07f), screen.getCentreX(), screen.getCentreY(),
                                           juce::Colours::transparentBlack, screen.getX(), screen.getY(), true));
        g.fillRect (screen);

        // Grid
        g.setColour (Palette::phosphor.withAlpha (0.08f));
        for (int i = 1; i < 10; ++i)
        {
            const float x = screen.getX() + screen.getWidth() * (float) i / 10.0f;
            g.drawVerticalLine ((int) x, screen.getY(), screen.getBottom());
        }

        auto content = screen.reduced (10.0f, 6.0f);
        auto top = content.removeFromTop (16.0f);
        auto mapArea = content.removeFromBottom (54.0f);
        content.removeFromBottom (4.0f);

        // Header line
        g.setFont (mono (11.5f));
        g.setColour (Palette::phosphor.withAlpha (0.65f));
        g.drawText ("ECHO-SCOPE", top, Justification::centredLeft);

        String timing = info.sync ? info.division + " = " + String (roundToInt (info.delayMs)) + " ms @ "
                                        + String (info.bpm, 1) + " BPM"
                                  : String (roundToInt (info.delayMs)) + " ms";
        if (info.mode == 2)
            timing << "  +" << roundToInt (info.offsetMs) << " R";
        g.setColour (Palette::phosphor);
        g.drawText (modeNames()[info.mode].toUpperCase() + "  " + timing, top, Justification::centredRight);

        paintHistory (g, content);
        paintEchoMap (g, mapArea);

        // Scanlines + glass
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        for (float y = screen.getY(); y < screen.getBottom(); y += 3.0f)
            g.drawHorizontalLine ((int) y, screen.getX(), screen.getRight());

        g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.07f), screen.getX(), screen.getY(),
                                           juce::Colours::transparentWhite, screen.getX(), screen.getCentreY(), false));
        g.fillRect (screen.withHeight (screen.getHeight() * 0.45f));
    }

    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (screen, 10.0f, 2.0f);
}

void EchoScope::paintHistory (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace juce;
    const float midY = area.getCentreY();
    const float halfH = area.getHeight() * 0.5f;

    g.setColour (Palette::phosphor.withAlpha (0.18f));
    g.drawHorizontalLine ((int) midY, area.getX(), area.getRight());

    auto toHeight = [halfH] (float level)
    {
        const float db = Decibels::gainToDecibels (level, -60.0f);
        return jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f) * halfH;
    };

    auto buildShape = [&] (const std::array<float, historySize>& data)
    {
        Path p;
        p.startNewSubPath (area.getX(), midY);
        for (int i = 0; i < historySize; ++i)
        {
            const float v = data[(size_t) ((writeIndex + i) % historySize)];
            p.lineTo (area.getX() + area.getWidth() * (float) i / (float) (historySize - 1), midY - toHeight (v));
        }
        for (int i = historySize - 1; i >= 0; --i)
        {
            const float v = data[(size_t) ((writeIndex + i) % historySize)];
            p.lineTo (area.getX() + area.getWidth() * (float) i / (float) (historySize - 1), midY + toHeight (v));
        }
        p.closeSubPath();
        return p;
    };

    const auto dryShape = buildShape (dryHistory);
    const auto wetShape = buildShape (wetHistory);

    g.setColour (Palette::phosphor.withAlpha (0.13f));
    g.fillPath (dryShape);
    g.setColour (Palette::phosphor.withAlpha (0.25f));
    g.strokePath (dryShape, PathStrokeType (0.8f));

    g.setColour (Palette::amber.withAlpha (0.28f));
    g.fillPath (wetShape);
    g.setColour (Palette::amber.withAlpha (0.25f));
    g.strokePath (wetShape, PathStrokeType (3.5f));
    g.setColour (Palette::amber);
    g.strokePath (wetShape, PathStrokeType (1.2f));

    g.setFont (mono (10.0f));
    g.setColour (Palette::phosphor.withAlpha (0.6f));
    g.drawText ("VOICE", area.removeFromLeft (60.0f).removeFromTop (14.0f), Justification::centredLeft);
    g.setColour (Palette::amber.withAlpha (0.9f));
    g.drawText ("ECHO", area.withX (area.getX()).removeFromTop (14.0f), Justification::centredLeft);
}

void EchoScope::paintEchoMap (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace juce;

    struct Tap { double timeMs; float amp; int side; }; // side: 0 both, 1 left, 2 right
    std::vector<Tap> taps;

    const double d = jmax (1.0, info.delayMs);
    const float fb = info.freeze ? 1.0f : info.feedback;
    float amp = 1.0f;
    for (int k = 1; k <= 40 && amp > 0.04f; ++k)
    {
        const double t = d * k;
        if (t > 4000.0) break;
        switch (info.mode)
        {
            case 1:  taps.push_back ({ t, amp, (k % 2 == 1) ? 1 : 2 }); break;
            case 2:  taps.push_back ({ t, amp, 1 });
                     taps.push_back ({ (d + info.offsetMs) * k, amp, 2 }); break;
            default: taps.push_back ({ t, amp, 0 }); break;
        }
        amp *= fb;
    }

    double range = 300.0;
    for (auto& t : taps)
        range = jmax (range, t.timeMs * 1.12);
    range = jmin (range, 4000.0);

    auto labelArea = area.removeFromLeft (18.0f);
    const float midY = area.getCentreY();
    const float halfH = area.getHeight() * 0.5f - 6.0f;

    g.setFont (mono (10.0f));
    g.setColour (Palette::phosphor.withAlpha (0.6f));
    g.drawText ("L", labelArea.withTrimmedBottom (area.getHeight() * 0.5f), Justification::centred);
    g.drawText ("R", labelArea.withTrimmedTop (area.getHeight() * 0.5f), Justification::centred);

    // Time ruler
    const double step = range > 2000.0 ? 500.0 : range > 800.0 ? 250.0 : 100.0;
    g.setColour (Palette::phosphor.withAlpha (0.25f));
    g.drawHorizontalLine ((int) midY, area.getX(), area.getRight());
    for (double t = 0.0; t <= range; t += step)
    {
        const float x = area.getX() + (float) (t / range) * area.getWidth();
        g.drawVerticalLine ((int) x, midY - 3.0f, midY + 3.0f);
        g.setColour (Palette::phosphor.withAlpha (0.35f));
        if (t > 0.0)
            g.drawText (String ((int) t), Rectangle<float> (x - 20.0f, area.getBottom() - 10.0f, 40.0f, 10.0f), Justification::centred);
        g.setColour (Palette::phosphor.withAlpha (0.25f));
    }

    // Dry hit at t = 0
    g.setColour (Palette::phosphor.withAlpha (0.5f));
    g.drawRect (Rectangle<float> (area.getX() - 1.0f, midY - halfH, 3.0f, halfH * 2.0f), 1.0f);

    // Duck reduces how loud the repeats are while the voice is present: show it as a dim overlay height.
    for (auto& t : taps)
    {
        const float x = area.getX() + (float) (t.timeMs / range) * area.getWidth();
        if (x > area.getRight()) continue;
        const float h = halfH * t.amp;
        const float top = t.side == 2 ? midY : midY - h;
        const float height = t.side == 0 ? h * 2.0f : h;
        Rectangle<float> bar (x - 2.0f, top, 4.0f, height);

        g.setColour (Palette::phosphor.withAlpha (0.25f));
        g.fillRect (bar.expanded (2.0f, 0.0f));
        g.setColour (Palette::phosphor);
        if (info.reverse)
        {
            Path ramp;
            ramp.addTriangle (x - 10.0f, bar.getBottom(), x + 2.0f, bar.getBottom(), x + 2.0f, bar.getY());
            if (t.side == 2)
                ramp = Path(), ramp.addTriangle (x - 10.0f, bar.getY(), x + 2.0f, bar.getY(), x + 2.0f, bar.getBottom());
            g.fillPath (ramp);
        }
        else
        {
            g.fillRect (bar);
        }
    }

    // Status lamps on the right
    auto status = area.removeFromRight (70.0f).removeFromTop (14.0f);
    if (info.freeze && (frameCounter / 15) % 2 == 0)
    {
        g.setColour (Palette::lampRed);
        g.drawText ("FREEZE", status, Justification::centredRight);
    }
    else if (info.duck > 0.01f)
    {
        g.setColour (Palette::amber.withAlpha (0.3f + 0.7f * (1.0f - info.duckGain)));
        g.drawText ("DUCK", status, Justification::centredRight);
    }
}

//==============================================================================
VoxSlapEditor::VoxSlapEditor (VoxSlapProcessor& p)
    : AudioProcessorEditor (p), processor (p),
      timeKnob (p.apvts, ParamID::timeMs, "TIME", true),
      divisionKnob (p.apvts, ParamID::division, "TIME", true),
      feedbackKnob (p.apvts, ParamID::feedback, "FEEDBACK", true),
      driveKnob (p.apvts, ParamID::drive, "DRIVE", true),
      duckKnob (p.apvts, ParamID::duck, "UNDER / AFTER", true),
      mixKnob (p.apvts, ParamID::mix, "MIX", true),
      hpfKnob (p.apvts, ParamID::hpf, "LOW CUT"),
      lpfKnob (p.apvts, ParamID::lpf, "HIGH CUT"),
      widthKnob (p.apvts, ParamID::width, "WIDTH"),
      offsetKnob (p.apvts, ParamID::offset, "R OFFSET"),
      modRateKnob (p.apvts, ParamID::modRate, "RATE"),
      modDepthKnob (p.apvts, ParamID::modDepth, "DEPTH"),
      pitchKnob (p.apvts, ParamID::pitch, "PITCH"),
      threshKnob (p.apvts, ParamID::duckThresh, "THRESHOLD"),
      releaseKnob (p.apvts, ParamID::duckRelease, "RELEASE"),
      outputKnob (p.apvts, ParamID::output, "OUTPUT")
{
    setLookAndFeel (&lnf);

    // Brushed, slightly speckled faceplate texture.
    panelTexture = juce::Image (juce::Image::ARGB, 256, 256, true);
    {
        juce::Graphics tg (panelTexture);
        juce::Random rng (1973);
        for (int i = 0; i < 2600; ++i)
        {
            tg.setColour (juce::Colours::black.withAlpha (rng.nextFloat() * 0.06f));
            tg.fillRect ((float) rng.nextInt (256), (float) rng.nextInt (256), 1.0f, 1.0f);
        }
        for (int y = 0; y < 256; y += 2)
        {
            tg.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (rng.nextFloat() * 0.025f));
            tg.drawHorizontalLine (y, 0.0f, 256.0f);
        }
    }

    // Header
    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("- preset -");
    presetBox.onChange = [this]
    {
        if (const int id = presetBox.getSelectedId(); id > 0)
            processor.presets.loadByIndex (id - 1);
    };

    for (auto* b : { &prevButton, &nextButton, &saveButton, &deleteButton, &simpleButton, &advancedButton })
    {
        b->getProperties().set ("dark", true);
        addAndMakeVisible (*b);
    }
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { savePresetDialog(); };
    deleteButton.onClick = [this] { deletePresetDialog(); };
    simpleButton.onClick = [this] { setAdvanced (false); };
    advancedButton.onClick = [this] { setAdvanced (true); };
    saveButton.setTooltip ("Save current settings as a user preset");

    // Display
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);
    addAndMakeVisible (scope);

    // Mode buttons
    for (int i = 0; i < 3; ++i)
    {
        auto& b = modeButtons[(size_t) i];
        b.setButtonText (modeNames()[i].toUpperCase());
        b.setClickingTogglesState (false);
        b.onClick = [this, i] { setChoiceParam (ParamID::mode, i); };
        addAndMakeVisible (b);
    }

    for (auto* k : { &timeKnob, &divisionKnob, &feedbackKnob, &driveKnob, &duckKnob, &mixKnob,
                     &hpfKnob, &lpfKnob, &widthKnob, &offsetKnob, &modRateKnob, &modDepthKnob,
                     &pitchKnob, &threshKnob, &releaseKnob, &outputKnob })
        addAndMakeVisible (*k);

    duckKnob.slider.setTooltip ("0% = echoes sit under the voice, 100% = echoes only come out after the phrase");

    addAndMakeVisible (syncToggle);
    syncAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ParamID::sync, syncToggle);

    driveTypeBox.addItemList (driveTypeNames(), 1);
    addAndMakeVisible (driveTypeBox);
    driveTypeAttachment = std::make_unique<ComboAttachment> (processor.apvts, ParamID::driveType, driveTypeBox);

    addAndMakeVisible (reverseToggle);
    addAndMakeVisible (freezeToggle);
    reverseAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ParamID::reverse, reverseToggle);
    freezeAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ParamID::freeze, freezeToggle);

    refreshPresetList();
    setAdvanced (isAdvanced());
    timerCallback();
    startTimerHz (30);
}

VoxSlapEditor::~VoxSlapEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

bool VoxSlapEditor::isAdvanced() const
{
    return (bool) processor.apvts.state.getProperty (advancedProp, false);
}

void VoxSlapEditor::setAdvanced (bool shouldBeAdvanced)
{
    processor.apvts.state.setProperty (advancedProp, shouldBeAdvanced, nullptr);
    simpleButton.setToggleState (! shouldBeAdvanced, juce::dontSendNotification);
    advancedButton.setToggleState (shouldBeAdvanced, juce::dontSendNotification);

    for (auto* c : std::initializer_list<juce::Component*> { &hpfKnob, &lpfKnob, &widthKnob, &offsetKnob, &modRateKnob,
                                                             &modDepthKnob, &pitchKnob, &threshKnob, &releaseKnob,
                                                             &outputKnob, &reverseToggle, &freezeToggle })
        c->setVisible (shouldBeAdvanced);

    const int h = shouldBeAdvanced ? advancedHeight : simpleHeight;
    if (getHeight() != h)
        setSize (editorWidth, h);
    else
        resized();
    repaint();
}

void VoxSlapEditor::setChoiceParam (const char* id, int index)
{
    if (auto* p = processor.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        p->endChangeGesture();
    }
}

void VoxSlapEditor::refreshPresetList()
{
    auto& pm = processor.presets;
    const auto& factory = getFactoryPresets();
    const auto users = pm.getUserPresetNames();

    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("FACTORY");
    int id = 1;
    for (auto& f : factory)
        presetBox.addItem (f.name, id++);

    if (! users.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("USER");
        for (auto& u : users)
            presetBox.addItem (u, id++);
    }

    shownPresetName = pm.getCurrentName();
    shownPresetCount = users.size();
    const int index = pm.getCurrentIndex();
    if (index >= 0)
        presetBox.setSelectedId (index + 1, juce::dontSendNotification);
    else
        presetBox.setText (shownPresetName, juce::dontSendNotification);

    deleteButton.setEnabled (index >= (int) factory.size());
}

void VoxSlapEditor::stepPreset (int delta)
{
    const int count = processor.presets.getAllPresetNames().size();
    const int current = juce::jmax (0, processor.presets.getCurrentIndex());
    processor.presets.loadByIndex ((current + delta + count) % count);
    refreshPresetList();
}

void VoxSlapEditor::savePresetDialog()
{
    auto* window = new juce::AlertWindow ("Save preset", "Name for your preset:", juce::MessageBoxIconType::NoIcon, this);
    window->setLookAndFeel (&lnf);
    window->addTextEditor ("name", processor.presets.getCurrentName());
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<VoxSlapEditor> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window] (int result)
    {
        if (result == 1 && safeThis != nullptr)
        {
            safeThis->processor.presets.saveUserPreset (window->getTextEditorContents ("name"));
            safeThis->refreshPresetList();
        }
    }), true);
}

void VoxSlapEditor::deletePresetDialog()
{
    const auto name = processor.presets.getCurrentName();
    if (! processor.presets.getUserPresetNames().contains (name))
        return;

    juce::Component::SafePointer<VoxSlapEditor> safeThis (this);
    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::QuestionIcon)
                       .withTitle ("Delete preset")
                       .withMessage ("Move \"" + name + "\" to the trash?")
                       .withButton ("Delete")
                       .withButton ("Cancel")
                       .withAssociatedComponent (this);

    juce::AlertWindow::showAsync (options, [safeThis, name] (int result)
    {
        if (result == 1 && safeThis != nullptr)
        {
            safeThis->processor.presets.deleteUserPreset (name);
            safeThis->refreshPresetList();
        }
    });
}

void VoxSlapEditor::timerCallback()
{
    inputMeter.setPeak (processor.inputPeak.exchange (0.0f));
    outputMeter.setPeak (processor.outputPeak.exchange (0.0f));
    processor.scope.popAll ([this] (float dry, float wet) { scope.pushHistory (dry, wet); });

    auto& s = processor.apvts;
    const int mode = (int) s.getRawParameterValue (ParamID::mode)->load();
    const bool sync = s.getRawParameterValue (ParamID::sync)->load() > 0.5f;

    EchoScope::Info info;
    info.mode = mode;
    info.delayMs = processor.getCurrentDelayMs();
    info.offsetMs = s.getRawParameterValue (ParamID::offset)->load();
    info.bpm = processor.hostBpm.load();
    info.feedback = s.getRawParameterValue (ParamID::feedback)->load() / 100.0f;
    info.duck = s.getRawParameterValue (ParamID::duck)->load() / 100.0f;
    info.duckGain = processor.duckGain.load();
    info.sync = sync;
    info.reverse = s.getRawParameterValue (ParamID::reverse)->load() > 0.5f;
    info.freeze = s.getRawParameterValue (ParamID::freeze)->load() > 0.5f;
    info.division = divisionNames()[(int) s.getRawParameterValue (ParamID::division)->load()];
    scope.setInfo (info);

    for (int i = 0; i < 3; ++i)
        modeButtons[(size_t) i].setToggleState (i == mode, juce::dontSendNotification);

    timeKnob.setVisible (! sync);
    divisionKnob.setVisible (sync);
    offsetKnob.setAlpha (mode == 2 ? 1.0f : 0.4f);

    if (processor.presets.getCurrentName() != shownPresetName
        || processor.presets.getUserPresetNames().size() != shownPresetCount)
        refreshPresetList();

    if (isAdvanced() != advancedButton.getToggleState())
        setAdvanced (isAdvanced());
}

void VoxSlapEditor::drawSection (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    auto r = area.toFloat().reduced (0.5f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (r.translated (1.0f, 1.0f), 6.0f, 1.2f);
    g.setColour (Palette::ink.withAlpha (0.55f));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);

    const auto f = font (11.0f);
    const float tw = juce::GlyphArrangement::getStringWidth (f, title) + 16.0f;
    auto label = juce::Rectangle<float> (tw, 14.0f).withCentre ({ r.getCentreX(), r.getY() });
    g.setColour (Palette::panel);
    g.fillRect (label);
    g.setColour (Palette::ink);
    g.setFont (f);
    g.drawText (title, label, juce::Justification::centred);
}

void VoxSlapEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    auto bounds = getLocalBounds();

    // Faceplate
    g.fillAll (Palette::panel);
    g.setTiledImageFill (panelTexture, 0, 0, 1.0f);
    g.fillRect (bounds);
    g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.12f), 0.0f, 70.0f,
                                       juce::Colours::black.withAlpha (0.10f), 0.0f, (float) getHeight(), false));
    g.fillRect (bounds);

    // Header band
    auto header = bounds.removeFromTop (70).toFloat();
    g.setGradientFill (ColourGradient (juce::Colour (0xff3a342e), 0.0f, 0.0f, Palette::header, 0.0f, header.getBottom(), false));
    g.fillRect (header);
    g.setColour (Palette::headerEdge);
    g.fillRect (header.removeFromBottom (3.0f));
    g.setColour (Palette::brassDark);
    g.drawHorizontalLine (67, 0.0f, (float) getWidth());

    // Logo plate
    auto logo = Rectangle<float> (20.0f, 12.0f, 220.0f, 44.0f);
    g.setGradientFill (ColourGradient (Palette::brass.brighter (0.3f), logo.getX(), logo.getY(),
                                       Palette::brassDark, logo.getX(), logo.getBottom(), false));
    g.fillRoundedRectangle (logo, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (logo, 4.0f, 1.0f);
    g.setColour (Palette::header);
    g.setFont (font (25.0f).withExtraKerningFactor (0.18f));
    g.drawText ("VOXSLAP", logo.withTrimmedLeft (12.0f).withTrimmedBottom (14.0f), Justification::centredLeft);
    g.setFont (font (9.5f).withExtraKerningFactor (0.15f));
    g.drawText ("VS-1  VOCAL SLAPBACK ECHO", logo.withTrimmedLeft (12.0f).withTrimmedTop (26.0f), Justification::centredLeft);
    drawScrew (g, { logo.getRight() - 12.0f, logo.getCentreY() }, 5.0f, 0.7f);

    // Power lamp
    drawLamp (g, { (float) getWidth() - 24.0f, 34.0f }, 6.0f, Palette::lampRed, 1.0f);

    // Screws on the faceplate corners
    drawScrew (g, { 10.0f, 82.0f }, 5.0f, 0.3f);
    drawScrew (g, { (float) getWidth() - 10.0f, 82.0f }, 5.0f, 1.2f);
    drawScrew (g, { 10.0f, (float) getHeight() - 12.0f }, 5.0f, 2.1f);
    drawScrew (g, { (float) getWidth() - 10.0f, (float) getHeight() - 12.0f }, 5.0f, 0.9f);

    for (auto& [area, title] : sections)
        drawSection (g, area, title);

    // Engraved hints under the duck knob
    if (duckKnob.isVisible())
    {
        auto k = duckKnob.getBounds().toFloat();
        g.setFont (font (9.0f));
        g.setColour (Palette::inkSoft);
        g.drawText ("UNDER", Rectangle<float> (k.getX() - 6.0f, k.getBottom() - 44.0f, 50.0f, 12.0f), Justification::centredLeft);
        g.drawText ("AFTER", Rectangle<float> (k.getRight() - 44.0f, k.getBottom() - 44.0f, 50.0f, 12.0f), Justification::centredRight);
    }

    g.setFont (font (9.5f).withExtraKerningFactor (0.2f));
    g.setColour (Palette::inkSoft);
    g.drawText ("HOMEBREW AUDIO  -  MOD. VS-1  -  SER. NO. 0001",
                Rectangle<int> (0, getHeight() - 20, getWidth() - 26, 14), Justification::centredRight);
}

void VoxSlapEditor::resized()
{
    sections.clear();

    // ---- Header ----
    {
        auto h = juce::Rectangle<int> (0, 0, getWidth(), 67).reduced (0, 19);
        h.removeFromLeft (262);
        prevButton.setBounds (h.removeFromLeft (30));
        h.removeFromLeft (6);
        presetBox.setBounds (h.removeFromLeft (270));
        h.removeFromLeft (6);
        nextButton.setBounds (h.removeFromLeft (30));
        h.removeFromLeft (12);
        saveButton.setBounds (h.removeFromLeft (62));
        h.removeFromLeft (6);
        deleteButton.setBounds (h.removeFromLeft (50));

        h.removeFromRight (46);
        advancedButton.setBounds (h.removeFromRight (100));
        h.removeFromRight (4);
        simpleButton.setBounds (h.removeFromRight (80));
    }

    const int margin = 24;
    auto area = getLocalBounds().withTrimmedTop (84).reduced (margin, 0);

    // ---- Display row ----
    displayArea = area.removeFromTop (196);
    {
        auto d = displayArea;
        inputMeter.setBounds (d.removeFromLeft (190));
        outputMeter.setBounds (d.removeFromRight (190));
        scope.setBounds (d.reduced (12, 0));
    }

    area.removeFromTop (18);

    // ---- Main row ----
    mainArea = area.removeFromTop (220);
    {
        auto m = mainArea;
        auto modeArea = m.removeFromLeft (150);
        sections.push_back ({ modeArea, "MODE" });
        m.removeFromLeft (10);
        sections.push_back ({ m, "ECHO" });

        auto buttons = modeArea.reduced (16, 0).withTrimmedTop (26).withTrimmedBottom (14);
        const int bh = 44, gap = (buttons.getHeight() - 3 * bh) / 2;
        for (auto& b : modeButtons)
        {
            b.setBounds (buttons.removeFromTop (bh));
            buttons.removeFromTop (gap);
        }

        auto inner = m.reduced (8, 0).withTrimmedTop (14).withTrimmedBottom (8);
        auto timeArea = inner.removeFromLeft (200);
        syncToggle.setBounds (timeArea.removeFromRight (56).withSizeKeepingCentre (56, 110));
        timeKnob.setBounds (timeArea);
        divisionKnob.setBounds (timeArea);

        const int w = inner.getWidth() / 4;
        feedbackKnob.setBounds (inner.removeFromLeft (w));
        auto driveArea = inner.removeFromLeft (w);
        driveTypeBox.setBounds (driveArea.removeFromBottom (26).withSizeKeepingCentre (110, 24));
        driveArea.removeFromBottom (2);
        driveKnob.setBounds (driveArea);
        duckKnob.setBounds (inner.removeFromLeft (w));
        mixKnob.setBounds (inner);
    }

    if (! isAdvanced())
        return;

    area.removeFromTop (18);

    // ---- Advanced row ----
    advancedArea = area.removeFromTop (180);
    {
        auto a = advancedArea;
        const int gap = 6;
        auto takeSection = [&] (int width, const juce::String& title)
        {
            auto s = a.removeFromLeft (width);
            a.removeFromLeft (gap);
            sections.push_back ({ s, title });
            return s.reduced (4, 0).withTrimmedTop (14).withTrimmedBottom (6);
        };

        auto filter = takeSection (156, "FILTER");
        hpfKnob.setBounds (filter.removeFromLeft (filter.getWidth() / 2));
        lpfKnob.setBounds (filter);

        auto stereo = takeSection (156, "STEREO");
        widthKnob.setBounds (stereo.removeFromLeft (stereo.getWidth() / 2));
        offsetKnob.setBounds (stereo);

        auto wobble = takeSection (156, "WOBBLE");
        modRateKnob.setBounds (wobble.removeFromLeft (wobble.getWidth() / 2));
        modDepthKnob.setBounds (wobble);

        pitchKnob.setBounds (takeSection (80, "SHIFT"));

        auto duckArea = takeSection (156, "DUCK");
        threshKnob.setBounds (duckArea.removeFromLeft (duckArea.getWidth() / 2));
        releaseKnob.setBounds (duckArea);

        auto fx = takeSection (112, "FX");
        reverseToggle.setBounds (fx.removeFromLeft (fx.getWidth() / 2).withSizeKeepingCentre (56, 120));
        freezeToggle.setBounds (fx.withSizeKeepingCentre (56, 120));

        outputKnob.setBounds (a.reduced (4, 0).withTrimmedTop (14).withTrimmedBottom (6));
        sections.push_back ({ a, "OUT" });
    }
}
