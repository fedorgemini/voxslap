#include "PluginEditor.h"
#include "BinaryData.h"

using namespace Analog;

static const juce::Identifier advancedProp { "advanced" }, scaleProp { "uiScale" }, meterModeProp { "meterMode" };

// Rendered faceplate geometry (must match tools/render_faceplate.py): meter wells, logical px.
static const juce::Rectangle<int> inputWell { 56, 92, 214, 196 }, scopeWell { 284, 92, 432, 196 }, outputWell { 730, 92, 214, 196 };
static const juce::Rectangle<int> namePlate { 44, 16, 912, 60 };
static constexpr int wellInset = 15; // face/screen starts at the bottom of the sloped well walls

// Short numbers for printed scales: 2 significant digits, "k" for thousands.
static juce::String scaleNumber (float v)
{
    const float a = std::abs (v);
    if (a < 0.001f) return "0";
    const float mag = std::pow (10.0f, std::floor (std::log10 (a)) - 1.0f);
    const float r = std::round (v / mag) * mag;
    if (std::abs (r) >= 1000.0f)
    {
        const float k = r / 1000.0f;
        return (std::abs (k - std::round (k)) < 0.05f ? juce::String (juce::roundToInt (k)) : juce::String (k, 1)) + "k";
    }
    if (std::abs (r) >= 10.0f) return juce::String (juce::roundToInt (r));
    return std::abs (r - std::round (r)) < 0.05f ? juce::String (juce::roundToInt (r)) : juce::String (r, 1);
}

//==============================================================================
double DetentSlider::snapValue (double attempted, DragMode mode)
{
    if (hasDetent && mode != notDragging
        && std::abs (valueToProportionOfLength (attempted) - valueToProportionOfLength (detent)) < 0.025)
        return detent;
    return attempted;
}

void DetentSlider::mouseDown (const juce::MouseEvent& e)
{
    rightClicked = e.mods.isPopupMenu();
    if (rightClicked)
    {
        if (onRightClick) onRightClick();
        return;
    }
    Slider::mouseDown (e);
}

void DetentSlider::mouseDrag (const juce::MouseEvent& e) { if (! rightClicked) Slider::mouseDrag (e); }
void DetentSlider::mouseUp (const juce::MouseEvent& e)   { if (! rightClicked) Slider::mouseUp (e); rightClicked = false; }

//==============================================================================
KnobControl::KnobControl (juce::AudioProcessorValueTreeState& state, const char* paramID,
                          const juce::String& t, KnobStyle style, const juce::String& scaleLabels)
    : title (t)
{
    using namespace juce;
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (style == KnobStyle::chicken ? 140 : 240);
    // Holding Shift switches to slow, velocity-based dragging for fine adjustments.
    slider.setVelocityModeParameters (0.25, 1, 0.0, true, ModifierKeys::shiftModifier);
    slider.setScrollWheelEnabled (true);
    slider.addMouseListener (this, false);

    auto* param = state.getParameter (paramID);
    choiceParam = dynamic_cast<AudioParameterChoice*> (param);
    stepped = choiceParam != nullptr;
    String labels = scaleLabels;

    if (labels == "auto")
    {
        labels.clear();
        const bool sparse = style == KnobStyle::black;
        const auto& range = param->getNormalisableRange();
        for (int i = 0; i < 5; ++i)
            labels << (i > 0 ? "|" : "") << (sparse && (i % 2 == 1) ? String() : scaleNumber (range.convertFrom0to1 ((float) i / 4.0f)));
    }
    else if (choiceParam != nullptr && labels.isEmpty())
    {
        labels = choiceParam->choices.joinIntoString ("|").toUpperCase();
    }

    auto& props = slider.getProperties();
    props.set ("knob", style == KnobStyle::aluminium ? "alu" : style == KnobStyle::chicken ? "chicken" : "black");
    if (labels.isNotEmpty())
        props.set ("labels", labels);

    const float twoPi = MathConstants<float>::twoPi;
    if (style == KnobStyle::chicken)
    {
        const int n = StringArray::fromTokens (labels, "|", "").size();
        const float span = jmin (degreesToRadians (270.0f), degreesToRadians (42.0f) * (float) (n - 1));
        slider.setRotaryParameters (twoPi - span * 0.5f, twoPi + span * 0.5f, true);
    }
    else
    {
        slider.setRotaryParameters (twoPi - degreesToRadians (135.0f), twoPi + degreesToRadians (135.0f), true);
    }

    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
    defaultValue = param->convertFrom0to1 (param->getDefaultValue());
    slider.setDoubleClickReturnValue (true, defaultValue);
    slider.onValueChange = [this] { announce (true); };
    slider.onRightClick = [this] { showMenu(); };
    props.set ("displayPos", slider.valueToProportionOfLength (slider.getValue()));
    addAndMakeVisible (slider);
}

void KnobControl::announce (bool changed)
{
    if (onReadout)
        onReadout (title, slider.getTextFromValue (slider.getValue()), changed);
}

void KnobControl::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isAltDown() && ! e.mods.isPopupMenu())
        slider.setValue (defaultValue, juce::sendNotificationSync);
    announce (false);
}

// The drawn position follows the value: stepped selectors swing over in ~0.1 s,
// continuous knobs only glide on big jumps (preset changes, automation) and track the mouse 1:1.
void KnobControl::tick (float dt)
{
    auto& props = slider.getProperties();
    const float target = (float) slider.valueToProportionOfLength (slider.getValue());
    const float current = (float) props.getWithDefault ("displayPos", target);
    if (current == target)
        return;

    float next = target;
    if (stepped || std::abs (target - current) > 0.08f)
    {
        const float speed = dt / (stepped ? 0.09f : 0.16f);
        next = current + juce::jlimit (-speed, speed, target - current);
        if (std::abs (target - next) < 0.002f) next = target;
    }
    props.set ("displayPos", next);
    slider.repaint();
}

void KnobControl::showMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader (title);
    m.addItem (1, "Reset to default");
    m.addItem (2, "Enter value...", ! stepped);
    if (choiceParam != nullptr)
    {
        m.addSeparator();
        for (int i = 0; i < choiceParam->choices.size(); ++i)
            m.addItem (100 + i, choiceParam->choices[i], true, choiceParam->getIndex() == i);
    }
    m.addSeparator();
    m.addItem (3, "Tip: Shift-drag = fine, Alt-click = reset", false);

    juce::Component::SafePointer<KnobControl> safeThis (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider), [safeThis] (int result)
    {
        if (safeThis == nullptr || result == 0) return;
        auto& s = safeThis->slider;
        if (result == 1)        s.setValue (safeThis->defaultValue, juce::sendNotificationSync);
        else if (result == 2)   safeThis->enterValueDialog();
        else if (result >= 100) s.setValue (result - 100, juce::sendNotificationSync);
    });
}

void KnobControl::enterValueDialog()
{
    auto* window = new juce::AlertWindow (title, "Enter a value:", juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("value", slider.getTextFromValue (slider.getValue()));
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<KnobControl> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window] (int result)
    {
        if (result == 1 && safeThis != nullptr)
        {
            auto& s = safeThis->slider;
            s.setValue (s.getValueFromText (window->getTextEditorContents ("value")), juce::sendNotificationSync);
        }
    }), true);
}

void KnobControl::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (20);
    r.removeFromBottom (16);
    slider.setBounds (r);
}

void KnobControl::paint (juce::Graphics& g)
{
    auto titleArea = getLocalBounds().removeFromTop (20);
    g.setFont (font (getWidth() < 90 ? 12.5f : 14.5f));
    setInk (g, Palette::silkscreen, panelOrigin (*this));
    g.drawText (title, titleArea, juce::Justification::centred);
}

//==============================================================================
static float vuPosForDb (float db) { return std::pow (10.0f, db / 20.0f) / 1.4125f; }

void VUMeter::update (float meanSquare, float peak, float duckGain, float dt)
{
    float target;
    if (mode == Mode::duck)
    {
        // Like a compressor's GR meter: rests on 0 VU and swings left by the ducking in dB.
        const float grDb = -juce::Decibels::gainToDecibels (juce::jmax (duckGain, 0.001f));
        target = vuPosForDb (-grDb);
    }
    else
    {
        // 0 VU = -18 dBFS RMS.
        const float vuDb = juce::Decibels::gainToDecibels (std::sqrt (juce::jmax (meanSquare, 0.0f)), -80.0f) + 18.0f;
        target = juce::jmin (1.15f, vuPosForDb (vuDb));
    }

    // Damped mass-spring movement: ~300 ms to settle with ~1.5% overshoot, bounces off the end stops.
    constexpr float wn = 13.0f, zeta = 0.78f;
    const int steps = 4;
    const float h = dt / (float) steps;
    for (int i = 0; i < steps; ++i)
    {
        const float acc = wn * wn * (target - needle) - 2.0f * zeta * wn * velocity;
        velocity += acc * h;
        needle += velocity * h;
        if (needle < -0.02f) { needle = -0.02f; velocity = -velocity * 0.25f; }
        if (needle > 1.12f)  { needle = 1.12f;  velocity = -velocity * 0.25f; }
    }

    peakLamp = (mode != Mode::duck && peak > 0.89f) ? 1.0f : peakLamp * 0.92f;
    repaint();
}

void VUMeter::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto face = getLocalBounds().toFloat().reduced ((float) wellInset);

    // Warm incandescent backlight from below
    ColourGradient light (juce::Colour (0xfffff3cf), face.getCentreX(), face.getBottom() + face.getHeight() * 0.1f,
                          juce::Colour (0xffb98c48), face.getX() - face.getWidth() * 0.1f, face.getY(), true);
    light.addColour (0.55, Palette::vuFace);
    g.setGradientFill (light);
    g.fillRect (face);

    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (face.toNearestInt());

    const float arcTop = face.getY() + 42.0f;
    const Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.42f);
    const float radius = pivot.y - arcTop;
    const float maxAngle = std::asin (jmin (0.95f, (face.getWidth() * 0.5f - 18.0f) / radius));
    auto angleFor = [maxAngle] (float pos) { return -maxAngle + 2.0f * maxAngle * pos; };
    const Colour ink (0xff221a12), red (0xffc92a1c);

    Path blackArc, redArc;
    blackArc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleFor (vuPosForDb (-20.0f)), angleFor (vuPosForDb (0.0f)), true);
    redArc.addCentredArc (pivot.x, pivot.y, radius + 1.5f, radius + 1.5f, 0.0f, angleFor (vuPosForDb (0.0f)), angleFor (1.0f), true);
    g.setColour (ink);
    g.strokePath (blackArc, PathStrokeType (3.5f));
    g.setColour (red);
    g.strokePath (redArc, PathStrokeType (7.0f));

    g.setFont (font (12.0f));
    for (float db : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
    {
        const float a = angleFor (vuPosForDb (db));
        g.setColour (db > 0.0f ? red : ink);
        g.drawLine (Line<float> (polar (pivot, radius + 1.0f, a), polar (pivot, radius + 9.0f, a)), 1.5f);
        if (db == -20.0f || db == -10.0f || db == -5.0f || db == -3.0f || db == 0.0f || db == 3.0f)
        {
            const auto text = (db > 0.0f ? "+" : "") + String ((int) db);
            g.drawText (text, Rectangle<float> (28.0f, 13.0f).withCentre (polar (pivot, radius + 19.0f, a)), Justification::centred);
        }
    }

    g.setFont (font (9.0f));
    g.setColour (ink.withAlpha (0.75f));
    for (int pct : { 0, 20, 40, 60, 80, 100 })
    {
        const float a = angleFor ((float) pct / 100.0f / 1.4125f);
        g.drawLine (Line<float> (polar (pivot, radius - 2.0f, a), polar (pivot, radius - 7.0f, a)), 1.0f);
        if (pct % 50 == 0)
            g.drawText (String (pct), Rectangle<float> (30.0f, 11.0f).withCentre (polar (pivot, radius - 14.0f, a)), Justification::centred);
    }

    const char* modeName = mode == Mode::input ? "INPUT" : mode == Mode::output ? "OUTPUT" : "ECHO DUCK";
    g.setColour (ink);
    g.setFont (font (24.0f, true));
    g.drawText ("VU", Rectangle<float> (face.getX(), face.getBottom() - 70.0f, face.getWidth(), 26.0f), Justification::centred);
    g.setFont (font (11.0f));
    g.setColour (ink.withAlpha (0.75f));
    g.drawText (modeName, Rectangle<float> (face.getX(), face.getBottom() - 46.0f, face.getWidth(), 13.0f), Justification::centred);

    const float a = angleFor (needle);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine (Line<float> (pivot.translated (3.0f, 5.0f), polar (pivot, radius + 12.0f, a).translated (3.0f, 5.0f)), 2.2f);
    g.setColour (juce::Colour (0xff120d0a));
    g.drawLine (Line<float> (pivot, polar (pivot, radius + 12.0f, a)), 1.4f);

    auto dome = Rectangle<float> (face.getWidth() * 0.42f, 22.0f).withCentre ({ face.getCentreX(), face.getBottom() });
    g.setGradientFill (ColourGradient (juce::Colour (0xff3a3632), dome.getCentreX(), dome.getY(),
                                       juce::Colour (0xff0c0b0a), dome.getCentreX(), dome.getBottom(), false));
    g.fillEllipse (dome);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawEllipse (dome.reduced (1.0f), 0.8f);

    // Shadows cast by the bezel, then the glass
    g.setGradientFill (ColourGradient (juce::Colours::black.withAlpha (0.55f), 0.0f, face.getY(),
                                       juce::Colours::transparentBlack, 0.0f, face.getY() + 22.0f, false));
    g.fillRect (face.withHeight (22.0f));
    g.setGradientFill (ColourGradient (juce::Colours::black.withAlpha (0.3f), face.getX(), 0.0f,
                                       juce::Colours::transparentBlack, face.getX() + 14.0f, 0.0f, false));
    g.fillRect (face.withWidth (14.0f));
    g.setGradientFill (ColourGradient (juce::Colours::black.withAlpha (0.2f), face.getRight(), 0.0f,
                                       juce::Colours::transparentBlack, face.getRight() - 12.0f, 0.0f, false));
    g.fillRect (face.withTrimmedLeft (face.getWidth() - 12.0f));

    Path glare;
    glare.startNewSubPath (face.getX(), face.getY());
    glare.lineTo (face.getX() + face.getWidth() * 0.55f, face.getY());
    glare.lineTo (face.getX() + face.getWidth() * 0.25f, face.getBottom());
    glare.lineTo (face.getX(), face.getBottom());
    glare.closeSubPath();
    g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.14f), face.getX(), face.getY(),
                                       juce::Colours::transparentWhite, face.getX() + face.getWidth() * 0.4f, face.getCentreY(), false));
    g.fillPath (glare);

    drawJewel (g, { face.getRight() - 13.0f, face.getY() + 15.0f }, 3.5f, Palette::lampRed, peakLamp);
}

//==============================================================================
void EchoScope::pushHistory (float dry, float wet, float beat)
{
    dryHistory[(size_t) writeIndex] = dry;
    wetHistory[(size_t) writeIndex] = wet;
    beatHistory[(size_t) writeIndex] = beat;
    writeIndex = (writeIndex + 1) % historySize;
    ++chunkClock;

    // Phrase/syllable onsets: each one triggers the echo-map flashes at the repeat times.
    slowDry += (dry - slowDry) * 0.05f;
    if (dry > 0.02f && dry > slowDry * 2.2f && chunkClock - lastOnset > 12)
    {
        onsets.push_back (chunkClock);
        lastOnset = chunkClock;
    }
    onsets.erase (std::remove_if (onsets.begin(), onsets.end(), [this] (juce::int64 o) { return chunkClock - o > 450; }), onsets.end());
}

void EchoScope::showReadout (const juce::String& title, const juce::String& value, bool changed)
{
    readoutTitle = title;
    readoutValue = value;
    readoutAlpha = changed ? 1.6f : juce::jmax (readoutAlpha, 1.0f);
}

juce::Path EchoScope::buildShape (const std::array<float, historySize>& data, juce::Rectangle<float> area) const
{
    using namespace juce;
    const float midY = area.getCentreY(), halfH = area.getHeight() * 0.5f;
    auto xFor = [&] (int i) { return area.getX() + area.getWidth() * (float) i / (float) (historySize - 1); };
    auto toHeight = [halfH] (float level)
    {
        const float db = Decibels::gainToDecibels (level, -60.0f);
        return jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f) * halfH;
    };
    Path p;
    p.startNewSubPath (area.getX(), midY);
    for (int i = 0; i < historySize; ++i)
        p.lineTo (xFor (i), midY - toHeight (data[(size_t) ((writeIndex + i) % historySize)]));
    for (int i = historySize - 1; i >= 0; --i)
        p.lineTo (xFor (i), midY + toHeight (data[(size_t) ((writeIndex + i) % historySize)]));
    p.closeSubPath();
    return p;
}

void EchoScope::tick (float dt)
{
    // Once per frame: remember this frame's trace for the afterglow (newest last).
    ghosts.push_back (buildShape (wetHistory, historyArea()));
    while (ghosts.size() > 4)
        ghosts.pop_front();

    readoutAlpha = juce::jmax (0.0f, readoutAlpha - dt);
    const float target = info.power ? 1.0f : 0.0f;
    powerAnim = powerAnim < target ? juce::jmin (target, powerAnim + dt / 0.7f)
                                   : juce::jmax (target, powerAnim - dt / 0.35f);
    blink += dt;
    repaint();
}

juce::Rectangle<float> EchoScope::screenArea() const  { return getLocalBounds().toFloat().reduced ((float) wellInset + 5.0f); }
juce::Rectangle<float> EchoScope::mapArea() const     { return screenArea().reduced (14.0f, 8.0f).removeFromBottom (52.0f); }
juce::Rectangle<float> EchoScope::historyArea() const
{
    auto c = screenArea().reduced (14.0f, 8.0f);
    c.removeFromTop (16.0f);
    c.removeFromBottom (56.0f);
    return c;
}

std::vector<EchoScope::Tap> EchoScope::computeTaps() const
{
    std::vector<Tap> taps;
    const double d = juce::jmax (1.0, info.delayMs);
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
    return taps;
}

double EchoScope::mapRange (const std::vector<Tap>& taps) const
{
    if (dragging)
        return dragRange;   // keep the scale still while the user drags
    double range = 300.0;
    for (auto& t : taps)
        range = juce::jmax (range, t.timeMs * 1.12);
    return juce::jmin (range, 4000.0);
}

static void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width)
{
    g.setColour (c.withAlpha (0.10f));
    g.strokePath (p, juce::PathStrokeType (width * 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.25f));
    g.strokePath (p, juce::PathStrokeType (width * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c.brighter (0.4f));
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

static void glowText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j, float alpha)
{
    g.setColour (Palette::phosphor.withAlpha (0.22f * alpha));
    for (auto o : { juce::Point<float> (-1.2f, 0.0f), { 1.2f, 0.0f }, { 0.0f, -1.2f }, { 0.0f, 1.2f } })
        g.drawText (text, r.translated (o.x, o.y), j, false);
    g.setColour (Palette::phosphor.brighter (0.3f).withAlpha (alpha));
    g.drawText (text, r, j, false);
}

void EchoScope::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto inner = getLocalBounds().toFloat().reduced ((float) wellInset);
    g.setColour (juce::Colour (0xff0b0b0a));
    g.fillRect (inner);

    const auto screen = screenArea();
    Path tube;
    tube.addRoundedRectangle (screen, 18.0f);

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (tube);
        g.setColour (Palette::crtBack);
        g.fillRect (screen);

        // Power-on: a dot stretches into a line, then the picture opens vertically (and the reverse).
        const float p = powerAnim;
        if (p > 0.0f)
        {
            const float open = jlimit (0.0f, 1.0f, (p - 0.25f) / 0.35f);
            const float lineW = jlimit (0.0f, 1.0f, p / 0.25f);
            auto lit = screen.withSizeKeepingCentre (screen.getWidth(), jmax (2.0f, screen.getHeight() * open));

            if (open > 0.0f)
            {
                Graphics::ScopedSaveState s2 (g);
                g.reduceClipRegion (lit.toNearestInt());
                g.setOpacity (jlimit (0.0f, 1.0f, (p - 0.25f) / 0.6f));

                g.setGradientFill (ColourGradient (juce::Colour (0xff0f2318), screen.getCentreX(), screen.getCentreY(),
                                                   Palette::crtBack, screen.getX(), screen.getY(), true));
                g.fillRect (screen);
                g.setColour (Palette::phosphor.withAlpha (0.09f));
                for (int i = 1; i < 10; ++i)
                    g.drawVerticalLine ((int) (screen.getX() + screen.getWidth() * (float) i / 10.0f), screen.getY(), screen.getBottom());
                for (int i = 1; i < 6; ++i)
                    g.drawHorizontalLine ((int) (screen.getY() + screen.getHeight() * (float) i / 6.0f), screen.getX(), screen.getRight());

                paintTrace (g, historyArea());

                auto top = screen.reduced (14.0f, 8.0f).removeFromTop (16.0f);
                g.setFont (mono (12.0f));
                g.setColour (Palette::phosphor.withAlpha (0.6f));
                g.drawText ("ECHO-SCOPE", top, Justification::centredLeft);
                String timing = info.sync ? info.division + " = " + String (roundToInt (info.delayMs)) + " ms @ "
                                                + String (info.bpm, 1) + " BPM"
                                          : String (roundToInt (info.delayMs)) + " ms";
                if (info.mode == 2)
                    timing << "  +" << roundToInt (info.offsetMs) << " R";
                g.setColour (Palette::phosphor);
                g.drawText (modeNames()[info.mode].toUpperCase() + "  " + timing, top, Justification::centredRight);

                paintEchoMap (g, mapArea());
                paintReadout (g, historyArea());
            }

            // The bright scan line of a tube warming up / switching off
            const float lineAlpha = 1.0f - open;
            if (lineAlpha > 0.0f)
            {
                auto line = Rectangle<float> (screen.getWidth() * lineW, 2.5f).withCentre (screen.getCentre());
                g.setColour (Palette::phosphor.withAlpha (0.25f * lineAlpha));
                g.fillRect (line.expanded (0.0f, 6.0f));
                g.setColour (juce::Colours::white.withAlpha (lineAlpha));
                g.fillRect (line);
            }
        }

        g.setGradientFill (ColourGradient (juce::Colours::transparentBlack, screen.getCentreX(), screen.getCentreY(),
                                           juce::Colours::black.withAlpha (0.65f), screen.getX(), screen.getY(), true));
        g.fillRect (screen);
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        for (float y = screen.getY(); y < screen.getBottom(); y += 3.0f)
            g.drawHorizontalLine ((int) y, screen.getX(), screen.getRight());
        g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.10f), screen.getX(), screen.getY(),
                                           juce::Colours::transparentWhite, screen.getX() + screen.getWidth() * 0.35f, screen.getCentreY(), false));
        g.fillRect (screen);
    }

    g.setColour (juce::Colours::black);
    g.strokePath (tube, PathStrokeType (2.5f));
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (screen.expanded (1.5f), 19.0f, 1.0f);
}

void EchoScope::paintTrace (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace juce;
    const float midY = area.getCentreY();
    const float halfH = area.getHeight() * 0.5f;
    auto xFor = [&] (int i) { return area.getX() + area.getWidth() * (float) i / (float) (historySize - 1); };

    // Beat grid from the host transport (bars brighter); scrolls with the history.
    for (int i = 0; i < historySize; ++i)
    {
        const float b = beatHistory[(size_t) ((writeIndex + i) % historySize)];
        if (b > 0.0f)
        {
            g.setColour (Palette::phosphor.withAlpha (b > 1.5f ? 0.35f : 0.14f));
            g.drawVerticalLine ((int) xFor (i), area.getY(), area.getBottom());
        }
    }

    g.setColour (Palette::phosphor.withAlpha (0.2f));
    g.drawHorizontalLine ((int) midY, area.getX(), area.getRight());
    for (int i = 0; i <= 50; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / 50.0f;
        g.drawVerticalLine ((int) x, midY - (i % 5 == 0 ? 4.0f : 2.0f), midY + (i % 5 == 0 ? 4.0f : 2.0f));
    }

    juce::ignoreUnused (halfH);
    const auto dryShape = buildShape (dryHistory, area);
    const auto wetShape = buildShape (wetHistory, area);

    g.setColour (Palette::phosphor.withAlpha (0.10f));
    g.fillPath (dryShape);
    g.setColour (Palette::phosphor.withAlpha (0.3f));
    g.strokePath (dryShape, PathStrokeType (0.8f));

    // Afterglow: the last few frames of the trace linger and fade like real phosphor.
    float ghostAlpha = 0.07f * (float) ghosts.size();
    for (auto& ghost : ghosts)
    {
        g.setColour (Palette::phosphor.withAlpha (ghostAlpha));
        g.strokePath (ghost, PathStrokeType (2.2f));
        ghostAlpha -= 0.07f;
    }

    g.setColour (Palette::phosphor.withAlpha (0.22f));
    g.fillPath (wetShape);
    glowStroke (g, wetShape, Palette::phosphor, 1.2f);

    g.setFont (mono (11.0f));
    g.setColour (Palette::phosphor.withAlpha (0.45f));
    g.drawText ("VOICE", area.withWidth (60.0f).withHeight (14.0f), Justification::centredLeft);
    g.setColour (Palette::phosphor);
    g.drawText ("ECHO", area.withTrimmedLeft (52.0f).withWidth (60.0f).withHeight (14.0f), Justification::centredLeft);
}

void EchoScope::paintEchoMap (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace juce;
    const auto taps = computeTaps();
    const double range = mapRange (taps);

    auto labelArea = area.removeFromLeft (18.0f);
    area.removeFromLeft (4.0f);
    const float midY = area.getCentreY();
    const float halfH = area.getHeight() * 0.5f - 6.0f;

    g.setFont (mono (11.0f));
    g.setColour (Palette::phosphor.withAlpha (0.6f));
    g.drawText ("L", labelArea.withTrimmedBottom (area.getHeight() * 0.5f), Justification::centred);
    g.drawText ("R", labelArea.withTrimmedTop (area.getHeight() * 0.5f), Justification::centred);

    const double step = range > 2000.0 ? 500.0 : range > 800.0 ? 250.0 : 100.0;
    g.setColour (Palette::phosphor.withAlpha (0.25f));
    g.drawHorizontalLine ((int) midY, area.getX(), area.getRight());
    for (double t = 0.0; t <= range; t += step)
    {
        const float x = area.getX() + (float) (t / range) * area.getWidth();
        g.setColour (Palette::phosphor.withAlpha (0.3f));
        g.drawVerticalLine ((int) x, midY - 3.0f, midY + 3.0f);
        if (t > 0.0)
            g.drawText (String ((int) t), Rectangle<float> (x - 20.0f, area.getBottom() - 10.0f, 40.0f, 10.0f), Justification::centred);
    }

    g.setColour (Palette::phosphor.withAlpha (0.5f));
    g.drawRect (Rectangle<float> (area.getX(), midY - halfH, 3.0f, halfH * 2.0f), 1.0f);

    for (auto& t : taps)
    {
        const float x = area.getX() + (float) (t.timeMs / range) * area.getWidth();
        if (x > area.getRight()) continue;

        // Flash when this repeat actually sounds after a sung syllable.
        float flash = 0.0f;
        const double tapChunks = t.timeMs / 10.0;
        for (auto o : onsets)
        {
            const double d = (double) chunkClock - ((double) o + tapChunks);
            if (d >= 0.0 && d < 30.0)
                flash = jmax (flash, (float) std::exp (-d / 6.0));
        }

        const float h = halfH * t.amp;
        const float top = t.side == 2 ? midY : midY - h;
        const float height = t.side == 0 ? h * 2.0f : h;
        Path mark;
        if (info.reverse)
        {
            if (t.side == 2) mark.addTriangle (x - 10.0f, top, x + 2.0f, top, x + 2.0f, top + height);
            else             mark.addTriangle (x - 10.0f, top + height, x + 2.0f, top + height, x + 2.0f, top);
        }
        else
        {
            mark.addRectangle (x - 1.5f, top, 3.0f, height);
        }
        g.setColour (Palette::phosphor.withAlpha (0.15f + 0.35f * flash));
        g.strokePath (mark, PathStrokeType (5.0f + 6.0f * flash));
        g.setColour (Palette::phosphor.withMultipliedBrightness (0.75f + 0.25f * flash).brighter (0.3f + 0.7f * flash));
        g.fillPath (mark);
    }

    auto status = area.removeFromRight (70.0f).removeFromTop (14.0f);
    if (info.freeze && std::fmod (blink, 1.0f) < 0.5f)
    {
        g.setColour (Palette::phosphor);
        g.drawText ("FREEZE", status, Justification::centredRight);
    }
    else if (info.duck > 0.01f)
    {
        g.setColour (Palette::phosphor.withAlpha (0.25f + 0.75f * (1.0f - info.duckGain)));
        g.drawText ("DUCK", status, Justification::centredRight);
    }
}

void EchoScope::paintReadout (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace juce;
    const float a = jlimit (0.0f, 1.0f, readoutAlpha / 0.4f);
    if (a <= 0.0f)
        return;

    auto box = area.withSizeKeepingCentre (jmin (area.getWidth() - 20.0f, 300.0f), 64.0f);
    g.setColour (Palette::crtBack.withAlpha (0.82f * a));
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (Palette::phosphor.withAlpha (0.35f * a));
    g.drawRoundedRectangle (box, 6.0f, 1.0f);

    g.setFont (mono (13.0f));
    glowText (g, readoutTitle, box.removeFromTop (24.0f).withTrimmedTop (6.0f), Justification::centred, 0.75f * a);
    g.setFont (mono (28.0f));
    glowText (g, readoutValue, box.withTrimmedBottom (4.0f), Justification::centred, a);
}

void EchoScope::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (mapArea().contains (e.position) ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void EchoScope::mouseDown (const juce::MouseEvent& e)
{
    if (! info.power || ! mapArea().contains (e.position))
        return;
    dragRange = mapRange (computeTaps());
    dragging = true;
    dragStartFeedback = info.feedback;
    if (onDragStart) onDragStart();
    mouseDrag (e);
}

void EchoScope::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;
    auto plot = mapArea().withTrimmedLeft (22.0f);
    const double ms = juce::jlimit (10.0, maxDelayMs, (double) ((e.position.x - plot.getX()) / plot.getWidth()) * dragRange);
    const float fb = juce::jlimit (0.0f, 0.95f, dragStartFeedback - (float) e.getDistanceFromDragStartY() / 120.0f);
    if (onDrag) onDrag (ms, fb);
}

void EchoScope::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;
    dragging = false;
    if (onDragEnd) onDragEnd();
}

//==============================================================================
Faceplate::Faceplate (VoxSlapProcessor& p)
    : processor (p),
      modeKnob      (p.apvts, ParamID::mode,        "MODE",          KnobStyle::chicken),
      timeKnob      (p.apvts, ParamID::timeMs,      "TIME",          KnobStyle::aluminium, "auto"),
      divisionKnob  (p.apvts, ParamID::division,    "TIME",          KnobStyle::aluminium, "1/32||1/16|||1/8|||1/4||1/2"),
      feedbackKnob  (p.apvts, ParamID::feedback,    "FEEDBACK",      KnobStyle::aluminium),
      driveKnob     (p.apvts, ParamID::drive,       "DRIVE",         KnobStyle::aluminium),
      driveTypeKnob (p.apvts, ParamID::driveType,   "CHARACTER",     KnobStyle::chicken),
      duckKnob      (p.apvts, ParamID::duck,        "UNDER / AFTER", KnobStyle::aluminium, "UNDER||||AFTER"),
      mixKnob       (p.apvts, ParamID::mix,         "MIX",           KnobStyle::aluminium),
      hpfKnob       (p.apvts, ParamID::hpf,         "LOW CUT",       KnobStyle::black, "auto"),
      lpfKnob       (p.apvts, ParamID::lpf,         "HIGH CUT",      KnobStyle::black, "auto"),
      widthKnob     (p.apvts, ParamID::width,       "WIDTH",         KnobStyle::black, "0||100||200"),
      offsetKnob    (p.apvts, ParamID::offset,      "R OFFSET",      KnobStyle::black, "0||20||40"),
      modRateKnob   (p.apvts, ParamID::modRate,     "RATE",          KnobStyle::black, "auto"),
      modDepthKnob  (p.apvts, ParamID::modDepth,    "DEPTH",         KnobStyle::black, "0||5||10"),
      pitchKnob     (p.apvts, ParamID::pitch,       "PITCH",         KnobStyle::black, "-12||0||+12"),
      threshKnob    (p.apvts, ParamID::duckThresh,  "THRESH",        KnobStyle::black, "-60||-30||0"),
      releaseKnob   (p.apvts, ParamID::duckRelease, "RELEASE",       KnobStyle::black, "auto"),
      outputKnob    (p.apvts, ParamID::output,      "OUTPUT",        KnobStyle::black, "-24|||+12")
{
    getProperties().set ("isFaceplate", true);
    setLookAndFeel (&lnf);

    topImage = juce::ImageCache::getFromMemory (BinaryData::faceplate_top_jpg, BinaryData::faceplate_top_jpgSize);
    bottomImage = juce::ImageCache::getFromMemory (BinaryData::faceplate_bottom_jpg, BinaryData::faceplate_bottom_jpgSize);

    // ---- Hardware ----
    inputMeter.setMode (VUMeter::Mode::input);
    outputMeter.setMode ((int) processor.apvts.state.getProperty (meterModeProp, 0) == 1 ? VUMeter::Mode::duck : VUMeter::Mode::output);
    outputMeter.setTooltip ("Click to switch between OUTPUT level and ECHO DUCK (how much the echoes are pushed down)");
    outputMeter.onClick = [this]
    {
        const bool duck = outputMeter.getMode() != VUMeter::Mode::duck;
        outputMeter.setMode (duck ? VUMeter::Mode::duck : VUMeter::Mode::output);
        processor.apvts.state.setProperty (meterModeProp, duck ? 1 : 0, nullptr);
    };
    for (auto* c : std::initializer_list<juce::Component*> { &inputMeter, &outputMeter, &scope })
        addAndMakeVisible (c);

    for (auto* k : { &modeKnob, &timeKnob, &divisionKnob, &feedbackKnob, &driveKnob, &driveTypeKnob, &duckKnob, &mixKnob,
                     &hpfKnob, &lpfKnob, &widthKnob, &offsetKnob, &modRateKnob, &modDepthKnob,
                     &pitchKnob, &threshKnob, &releaseKnob, &outputKnob })
        addAndMakeVisible (*k);

    for (auto* k : allKnobs())
        k->onReadout = [this] (const juce::String& t, const juce::String& v, bool changed) { scope.showReadout (t, v, changed); };

    pitchKnob.slider.setDetent (0.0);
    widthKnob.slider.setDetent (100.0);
    outputKnob.slider.setDetent (0.0);

    // Drag the echo marks on the scope: left/right = time (snaps to note values when synced), up/down = feedback.
    scope.setTooltip ("Drag the echo marks: left/right = time, up/down = feedback");
    scope.onDragStart = [this]
    {
        for (auto* id : { ParamID::timeMs, ParamID::division, ParamID::feedback })
            processor.apvts.getParameter (id)->beginChangeGesture();
    };
    scope.onDragEnd = [this]
    {
        for (auto* id : { ParamID::timeMs, ParamID::division, ParamID::feedback })
            processor.apvts.getParameter (id)->endChangeGesture();
    };
    scope.onDrag = [this] (double ms, float fb)
    {
        auto set = [this] (const char* id, float value)
        {
            auto* p = processor.apvts.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (value));
        };
        if (processor.apvts.getRawParameterValue (ParamID::sync)->load() > 0.5f)
        {
            const double quarterMs = 60000.0 / juce::jmax (20.0, processor.hostBpm.load());
            int best = 0;
            for (int i = 1; i < divisionNames().size(); ++i)
                if (std::abs (divisionBeats (i) * quarterMs - ms) < std::abs (divisionBeats (best) * quarterMs - ms))
                    best = i;
            set (ParamID::division, (float) best);
        }
        else
        {
            set (ParamID::timeMs, (float) ms);
        }
        set (ParamID::feedback, fb * 100.0f);
        scope.showReadout ("TIME / FEEDBACK", juce::String (juce::roundToInt (processor.getCurrentDelayMs())) + " ms  "
                                                  + juce::String (juce::roundToInt (fb * 100.0f)) + " %", true);
    };

    duckKnob.slider.setTooltip ("UNDER: echoes play under the voice.  AFTER: echoes hide while you sing and come out in the gaps.");
    driveKnob.slider.setTooltip ("Distortion on the repeats only - the dry voice stays clean");

    for (auto* t : { &syncToggle, &reverseToggle, &freezeToggle, &powerSwitch })
        addAndMakeVisible (*t);
    syncAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ParamID::sync, syncToggle);
    reverseAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ParamID::reverse, reverseToggle);
    freezeAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ParamID::freeze, freezeToggle);

    // POWER = not bypassed
    powerSwitch.getProperties().set ("sheet", "power");
    powerSwitch.setTooltip ("Power: switch off to bypass the echo");
    powerSwitch.setClickingTogglesState (true);
    powerSwitch.onClick = [this]
    {
        if (auto* bypass = processor.apvts.getParameter (ParamID::bypass))
        {
            bypass->beginChangeGesture();
            bypass->setValueNotifyingHost (powerSwitch.getToggleState() ? 0.0f : 1.0f);
            bypass->endChangeGesture();
        }
    };

    // ---- Toolbar ----
    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("- preset -");
    presetBox.onChange = [this]
    {
        if (const int id = presetBox.getSelectedId(); id > 0)
            processor.presets.loadByIndex (id - 1);
    };

    for (auto* b : { &prevButton, &nextButton, &saveButton, &deleteButton, &slotA, &slotB, &copyButton, &simpleButton, &advancedButton })
        addAndMakeVisible (*b);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { savePresetDialog(); };
    deleteButton.onClick = [this] { deletePresetDialog(); };
    slotA.onClick = [this] { processor.presets.switchToSlot (0); };
    slotB.onClick = [this] { processor.presets.switchToSlot (1); };
    copyButton.onClick = [this] { processor.presets.copyActiveToOther(); };
    slotA.setTooltip ("Compare: setting A");
    slotB.setTooltip ("Compare: setting B");
    copyButton.setTooltip ("Copy the current setting to the other slot");
    simpleButton.onClick = [this] { setAdvanced (false); };
    advancedButton.onClick = [this] { setAdvanced (true); };

    addAndMakeVisible (sizeBox);
    sizeBox.addItem ("100%", 100);
    sizeBox.addItem ("125%", 125);
    sizeBox.addItem ("150%", 150);
    sizeBox.setSelectedId (juce::roundToInt (getScale() * 100.0f), juce::dontSendNotification);
    sizeBox.onChange = [this] { setScale ((float) sizeBox.getSelectedId() / 100.0f); };
    sizeBox.setTooltip ("Window size");

    refreshPresetList();
    tick (0.0f);
}

Faceplate::~Faceplate()
{
    setLookAndFeel (nullptr);
}

bool Faceplate::isAdvanced() const { return (bool) processor.apvts.state.getProperty (advancedProp, false); }

float Faceplate::getScale() const
{
    return juce::jlimit (1.0f, 1.5f, (float) (double) processor.apvts.state.getProperty (scaleProp, 1.0));
}

void Faceplate::setAdvanced (bool shouldBeAdvanced)
{
    processor.apvts.state.setProperty (advancedProp, shouldBeAdvanced, nullptr);
    if (onLayoutChanged) onLayoutChanged();
}

void Faceplate::setScale (float s)
{
    processor.apvts.state.setProperty (scaleProp, s, nullptr);
    if (onLayoutChanged) onLayoutChanged();
}

void Faceplate::refreshPresetList()
{
    auto& pm = processor.presets;
    const auto& factory = getFactoryPresets();
    const auto users = pm.getUserPresetNames();

    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    int id = 1;
    for (auto& f : factory)
        presetBox.addItem (f.name, id++);

    if (! users.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("User");
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

void Faceplate::stepPreset (int delta)
{
    const int count = processor.presets.getAllPresetNames().size();
    const int current = juce::jmax (0, processor.presets.getCurrentIndex());
    processor.presets.loadByIndex ((current + delta + count) % count);
    refreshPresetList();
}

void Faceplate::savePresetDialog()
{
    auto* window = new juce::AlertWindow ("Save preset", "Name for your preset:", juce::MessageBoxIconType::NoIcon, this);
    window->setLookAndFeel (&lnf);
    window->addTextEditor ("name", processor.presets.getCurrentName());
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<Faceplate> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window] (int result)
    {
        if (result == 1 && safeThis != nullptr)
        {
            safeThis->processor.presets.saveUserPreset (window->getTextEditorContents ("name"));
            safeThis->refreshPresetList();
        }
    }), true);
}

void Faceplate::deletePresetDialog()
{
    const auto name = processor.presets.getCurrentName();
    if (! processor.presets.getUserPresetNames().contains (name))
        return;

    juce::Component::SafePointer<Faceplate> safeThis (this);
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

void Faceplate::tick (float dt)
{
    auto& s = processor.apvts;
    const bool power = s.getRawParameterValue (ParamID::bypass)->load() < 0.5f;

    inputMeter.update (processor.inputMeanSquare.load(), processor.inputPeak.exchange (0.0f), 1.0f, dt);
    outputMeter.update (processor.outputMeanSquare.load(), processor.outputPeak.exchange (0.0f), processor.duckGain.load(), dt);
    processor.scope.popAll ([this] (float dry, float wet, float beat) { scope.pushHistory (dry, wet, beat); });

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
    info.power = power;
    info.division = divisionNames()[(int) s.getRawParameterValue (ParamID::division)->load()];
    scope.setInfo (info);
    scope.tick (dt);
    for (auto* k : allKnobs())
        k->tick (dt);

    // Scales are painted by the panel, so redraw it when a knob's scale swaps or dims.
    const float offsetAlpha = mode == 2 ? 1.0f : 0.45f;
    if (timeKnob.isVisible() == sync || divisionKnob.isVisible() != sync || offsetKnob.getAlpha() != offsetAlpha)
    {
        timeKnob.setVisible (! sync);
        divisionKnob.setVisible (sync);
        offsetKnob.setAlpha (offsetAlpha);
        repaint();
    }

    // Switch levers travel through the in-between frames instead of jumping.
    if (powerSwitch.getToggleState() != power)
        powerSwitch.setToggleState (power, juce::dontSendNotification);
    for (auto* t : { &syncToggle, &reverseToggle, &freezeToggle, &powerSwitch })
    {
        const float target = t->getToggleState() ? 0.0f : 1.0f;
        auto& props = t->getProperties();
        const float current = props.contains ("anim") ? (float) props["anim"] : target;
        const float next = dt <= 0.0f ? target
                                      : (current < target ? juce::jmin (target, current + dt / 0.06f)
                                                          : juce::jmax (target, current - dt / 0.06f));
        if (! props.contains ("anim") || next != current)
        {
            props.set ("anim", next);
            t->repaint();
        }
    }

    const int slot = processor.presets.getActiveSlot();
    slotA.setToggleState (slot == 0, juce::dontSendNotification);
    slotB.setToggleState (slot == 1, juce::dontSendNotification);
    copyButton.setButtonText (slot == 0 ? "A > B" : "B > A");
    simpleButton.setToggleState (! isAdvanced(), juce::dontSendNotification);
    advancedButton.setToggleState (isAdvanced(), juce::dontSendNotification);

    if (processor.presets.getCurrentName() != shownPresetName
        || processor.presets.getUserPresetNames().size() != shownPresetCount)
        refreshPresetList();

    if (! getProperties().contains ("powerShown") || power != (bool) getProperties()["powerShown"])
    {
        getProperties().set ("powerShown", power);
        repaint (juce::Rectangle<int> (800, topHeight - 50, 200, 50));
    }
}

//==============================================================================
void Faceplate::paintGroupLabel (juce::Graphics& g, juce::Rectangle<int> span, const juce::String& title)
{
    using namespace juce;
    const auto f = font (12.5f);
    const float tw = GlyphArrangement::getStringWidth (f, title) + 14.0f;
    const auto r = span.toFloat().reduced (10.0f, 0.0f);
    const float y = r.getCentreY();
    const float cx = r.getCentreX();

    setInk (g, Palette::silkscreen.withAlpha (0.85f));
    g.fillRect (Rectangle<float> (r.getX(), y - 0.5f, cx - tw * 0.5f - r.getX(), 1.1f));
    g.fillRect (Rectangle<float> (cx + tw * 0.5f, y - 0.5f, r.getRight() - cx - tw * 0.5f, 1.1f));
    g.fillRect (Rectangle<float> (r.getX(), y - 0.5f, 1.1f, 5.5f));
    g.fillRect (Rectangle<float> (r.getRight() - 1.1f, y - 0.5f, 1.1f, 5.5f));
    g.setFont (f);
    g.drawText (title, Rectangle<float> (tw, 15.0f).withCentre ({ cx, y }), Justification::centred);
}

void Faceplate::paint (juce::Graphics& g)
{
    using namespace juce;
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (topImage, Rectangle<float> (0.0f, 0.0f, (float) width, (float) topHeight));
    if (isAdvanced())
        g.drawImage (bottomImage, Rectangle<float> (0.0f, (float) topHeight, (float) width, (float) bottomHeight));

    // Name plate: printed (not embossed) wordmark next to the rendered emblem
    {
        const auto plate = namePlate.toFloat();
        setInk (g, Palette::engraved.withAlpha (0.92f));
        g.setFont (font (36.0f, true).withExtraKerningFactor (0.16f));
        g.drawText ("VOXSLAP", Rectangle<float> (plate.getX() + 86.0f, plate.getY() + 4.0f, 300.0f, plate.getHeight() - 8.0f),
                    Justification::centredLeft);
        g.setFont (font (14.0f, true).withExtraKerningFactor (0.22f));
        const auto right = Rectangle<float> (plate.getRight() - 360.0f, plate.getY() + 12.0f, 330.0f, 18.0f);
        g.drawText ("STUDIO VOCAL ECHO", right, Justification::centredRight);
        g.setFont (font (11.5f).withExtraKerningFactor (0.25f));
        g.drawText ("MODEL VS-1   -   SLAPBACK / DOUBLER", right.translated (0.0f, 18.0f), Justification::centredRight);
    }

    for (auto& [span, title] : groupLabels)
        paintGroupLabel (g, span, title);

    // Knob scales are silkscreen on the panel, so they are drawn here, beneath the knobs.
    for (auto* k : allKnobs())
        if (k->isVisible())
            Analog::LookAndFeel::drawKnobScale (g, k->slider, k->slider.getBounds().translated (k->getX(), k->getY()).toFloat(), k->getAlpha());

    // Footer: maker's line, power switch with lamp
    {
        const bool power = processor.apvts.getRawParameterValue (ParamID::bypass)->load() < 0.5f;
        const auto footer = Rectangle<float> (60.0f, (float) topHeight - 34.0f, (float) width - 120.0f, 20.0f);
        setInk (g, Palette::silkscreen.withAlpha (0.8f));
        g.setFont (font (11.0f).withExtraKerningFactor (0.28f));
        g.drawText ("HOMEBREW AUDIO  -  STUDIO ECHO UNIT  -  SER. NO. 0001", footer, Justification::centredLeft);
        g.setFont (font (12.0f).withExtraKerningFactor (0.2f));
        g.drawText ("POWER", Rectangle<float> (footer.getRight() - 170.0f, footer.getY(), 60.0f, footer.getHeight()), Justification::centredRight);
        drawJewel (g, { footer.getRight() - 4.0f, footer.getCentreY() }, 6.5f, Palette::lampRed, power ? 1.0f : 0.0f);
    }

    if (isAdvanced())
    {
        setInk (g, Palette::silkscreen.withAlpha (0.6f));
        g.setFont (font (11.0f).withExtraKerningFactor (0.28f));
        g.drawText ("VS-1X  EXPANDER", Rectangle<float> (60.0f, (float) (topHeight + bottomHeight) - 32.0f, (float) width - 120.0f, 16.0f),
                    Justification::centredRight);
    }

    // Software toolbar
    const auto bar = Rectangle<float> (0.0f, (float) (getHeight() - toolbarHeight), (float) width, (float) toolbarHeight);
    g.setColour (Palette::barBack);
    g.fillRect (bar);
    g.setColour (juce::Colours::black);
    g.fillRect (bar.withHeight (1.0f));
    g.setColour (Palette::barDim);
    g.setFont (uiFont (12.5f));
    g.drawText ("PRESET", bar.withX (14.0f).withWidth (52.0f), Justification::centredLeft);
    g.drawText ("COMPARE", bar.withX (502.0f).withWidth (66.0f), Justification::centredLeft);
}

void Faceplate::paintOverChildren (juce::Graphics& g)
{
    using namespace juce;
    // One studio light over the whole unit: soft vignette and a slightly warm cast,
    // so the rendered parts and the live-drawn parts read as one photograph.
    const auto units = getLocalBounds().withTrimmedBottom (toolbarHeight).toFloat();
    ColourGradient vignette (juce::Colours::transparentBlack, units.getCentreX(), units.getY() + units.getHeight() * 0.42f,
                             juce::Colours::black.withAlpha (0.30f), units.getX(), units.getY(), true);
    vignette.addColour (0.6, juce::Colours::transparentBlack);
    g.setGradientFill (vignette);
    g.fillRect (units);
    g.setColour (juce::Colour (0xffff9a3c).withAlpha (0.035f));
    g.fillRect (units);
}

void Faceplate::resized()
{
    groupLabels.clear();

    inputMeter.setBounds (inputWell);
    scope.setBounds (scopeWell);
    outputMeter.setBounds (outputWell);

    const int innerX = 56, innerW = 888;

    // ---- Main controls ----
    {
        auto row = juce::Rectangle<int> (innerX, 300, innerW, 208);
        auto labels = row.removeFromTop (16);
        row.removeFromTop (2);
        const int x0 = row.getX();

        modeKnob.setBounds (row.removeFromLeft (112));
        const int delayStart = row.getX();
        auto timeArea = row.removeFromLeft (136);
        timeKnob.setBounds (timeArea);
        divisionKnob.setBounds (timeArea);
        syncToggle.setBounds (row.removeFromLeft (58).withSizeKeepingCentre (74, 120));
        feedbackKnob.setBounds (row.removeFromLeft (112));
        const int delayEnd = row.getX();
        driveKnob.setBounds (row.removeFromLeft (112));
        driveTypeKnob.setBounds (row.removeFromLeft (120));
        const int satEnd = row.getX();
        duckKnob.setBounds (row.removeFromLeft (122));
        mixKnob.setBounds (row);

        const int y = labels.getY(), lh = labels.getHeight();
        groupLabels.push_back ({ { x0, y, delayStart - x0, lh }, "SELECT" });
        groupLabels.push_back ({ { delayStart, y, delayEnd - delayStart, lh }, "DELAY" });
        groupLabels.push_back ({ { delayEnd, y, satEnd - delayEnd, lh }, "SATURATION" });
        groupLabels.push_back ({ { satEnd, y, row.getRight() - satEnd, lh }, "BLEND" });
    }

    powerSwitch.setBounds (838, topHeight - 50, 64, 52);

    // ---- Expander ----
    const bool advanced = isAdvanced();
    for (auto* c : std::initializer_list<juce::Component*> { &hpfKnob, &lpfKnob, &widthKnob, &offsetKnob, &modRateKnob,
                                                             &modDepthKnob, &pitchKnob, &threshKnob, &releaseKnob,
                                                             &outputKnob, &reverseToggle, &freezeToggle })
        c->setVisible (advanced);

    if (advanced)
    {
        auto row = juce::Rectangle<int> (innerX, topHeight + 18, innerW, 160);
        auto labels = row.removeFromTop (16);
        row.removeFromTop (2);
        const int y = labels.getY(), lh = labels.getHeight();
        const int kw = 74, tw = 60;

        auto group = [&] (std::initializer_list<juce::Component*> comps, std::initializer_list<int> widths, const juce::String& title)
        {
            const int start = row.getX();
            auto w = widths.begin();
            for (auto* c : comps)
                c->setBounds (row.removeFromLeft (*w++));
            groupLabels.push_back ({ { start, y, row.getX() - start, lh }, title });
            row.removeFromLeft (4);
        };

        group ({ &hpfKnob, &lpfKnob }, { kw, kw }, "FILTER");
        group ({ &widthKnob, &offsetKnob }, { kw, kw }, "STEREO");
        group ({ &modRateKnob, &modDepthKnob }, { kw, kw }, "WOBBLE");
        group ({ &pitchKnob }, { kw }, "SHIFT");
        group ({ &threshKnob, &releaseKnob }, { kw, kw }, "DUCK");
        group ({ &reverseToggle, &freezeToggle }, { tw, tw }, "FX");
        outputKnob.setBounds (row);
        groupLabels.push_back ({ { row.getX(), y, row.getWidth(), lh }, "OUT" });

        for (auto* t : { &reverseToggle, &freezeToggle })
            t->setBounds (t->getBounds().withSizeKeepingCentre (tw + 12, 120));
    }

    // ---- Toolbar ----
    auto bar = juce::Rectangle<int> (0, getHeight() - toolbarHeight, width, toolbarHeight).reduced (10, 7);
    bar.removeFromLeft (56);
    prevButton.setBounds (bar.removeFromLeft (28));
    bar.removeFromLeft (4);
    presetBox.setBounds (bar.removeFromLeft (236));
    bar.removeFromLeft (4);
    nextButton.setBounds (bar.removeFromLeft (28));
    bar.removeFromLeft (10);
    saveButton.setBounds (bar.removeFromLeft (56));
    bar.removeFromLeft (4);
    deleteButton.setBounds (bar.removeFromLeft (62));
    bar.removeFromLeft (84);
    slotA.setBounds (bar.removeFromLeft (30));
    bar.removeFromLeft (3);
    slotB.setBounds (bar.removeFromLeft (30));
    bar.removeFromLeft (4);
    copyButton.setBounds (bar.removeFromLeft (56));

    sizeBox.setBounds (bar.removeFromRight (78));
    bar.removeFromRight (14);
    advancedButton.setBounds (bar.removeFromRight (86));
    bar.removeFromRight (3);
    simpleButton.setBounds (bar.removeFromRight (66));

    repaint();
}

//==============================================================================
VoxSlapEditor::VoxSlapEditor (VoxSlapProcessor& p)
    : AudioProcessorEditor (p), faceplate (p)
{
    addAndMakeVisible (faceplate);
    faceplate.onLayoutChanged = [this] { applyLayout(); };
    applyLayout();
    startTimerHz (60);
}

VoxSlapEditor::~VoxSlapEditor()
{
    stopTimer();
}

void VoxSlapEditor::applyLayout()
{
    const float s = faceplate.getScale();
    faceplate.setBounds (0, 0, Faceplate::width, faceplate.getBaseHeight());
    faceplate.setTransform (juce::AffineTransform::scale (s));
    setSize (juce::roundToInt (Faceplate::width * s), juce::roundToInt ((float) faceplate.getBaseHeight() * s));
    faceplate.resized();
}

void VoxSlapEditor::timerCallback()
{
    faceplate.tick (1.0f / 60.0f);
}
