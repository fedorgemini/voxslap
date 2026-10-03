#include "PluginEditor.h"
#include "Textures.h"

using namespace Analog;

static const juce::Identifier advancedProp { "advanced" };

//==============================================================================
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

KnobControl::KnobControl (juce::AudioProcessorValueTreeState& state, const char* paramID,
                          const juce::String& t, KnobStyle style, const juce::String& scaleLabels)
    : title (t)
{
    using namespace juce;
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (style == KnobStyle::chicken ? 140 : 240);
    slider.addMouseListener (this, false);
    // Printed scale labels may extend past the knob's own box, like silkscreen on a panel.
    slider.setPaintingIsUnclipped (true);
    setPaintingIsUnclipped (true);

    auto* param = state.getParameter (paramID);
    String labels = scaleLabels;

    if (labels == "auto")
    {
        labels.clear();
        const auto& range = param->getNormalisableRange();
        // Small knobs only have room for the ends and the centre.
        const bool sparse = style == KnobStyle::black;
        for (int i = 0; i < 5; ++i)
            labels << (i > 0 ? "|" : "") << (sparse && (i % 2 == 1) ? String() : scaleNumber (range.convertFrom0to1 ((float) i / 4.0f)));
    }
    else if (auto* choice = dynamic_cast<AudioParameterChoice*> (param); choice != nullptr && labels.isEmpty())
    {
        labels = choice->choices.joinIntoString ("|").toUpperCase();
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
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
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
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.setFont (font (12.0f));
    auto titleArea = getLocalBounds().removeFromTop (20);
    g.drawText (title, titleArea.translated (0, 1), juce::Justification::centred);
    g.setColour (Palette::silkscreen);
    g.drawText (title, titleArea, juce::Justification::centred);
}

void KnobControl::paintOverChildren (juce::Graphics& g)
{
    if (! slider.isMouseOverOrDragging())
        return;

    const auto text = slider.getTextFromValue (slider.getValue());
    g.setFont (mono (11.5f));
    const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text) + 14.0f;
    auto plate = juce::Rectangle<float> (tw, 16.0f).withCentre ({ (float) getWidth() * 0.5f, (float) getHeight() - 8.0f });
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.fillRoundedRectangle (plate, 3.0f);
    g.setColour (Palette::amber);
    g.drawText (text, plate, juce::Justification::centred);
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
    const auto face = drawBezel (g, getLocalBounds().toFloat(), 10.0f);

    // Warm incandescent backlight from below
    ColourGradient light (juce::Colour (0xfffff3cf), face.getCentreX(), face.getBottom() + face.getHeight() * 0.1f,
                          juce::Colour (0xffb98c48), face.getX() - face.getWidth() * 0.1f, face.getY(), true);
    light.addColour (0.55, Palette::vuFace);
    g.setGradientFill (light);
    g.fillRect (face);

    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (face.toNearestInt());

    const float arcTop = face.getY() + 46.0f;
    const Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.42f);
    const float radius = pivot.y - arcTop;
    const float maxAngle = std::asin (jmin (0.95f, (face.getWidth() * 0.5f - 18.0f) / radius));
    auto angleFor = [maxAngle] (float pos) { return -maxAngle + 2.0f * maxAngle * pos; };
    auto posForDb = [] (float db) { return std::pow (10.0f, db / 20.0f) / 1.4125f; };
    const Colour ink (0xff221a12), red (0xffc92a1c);

    Path blackArc, redArc;
    blackArc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, angleFor (posForDb (-20.0f)), angleFor (posForDb (0.0f)), true);
    redArc.addCentredArc (pivot.x, pivot.y, radius + 1.5f, radius + 1.5f, 0.0f, angleFor (posForDb (0.0f)), angleFor (1.0f), true);
    g.setColour (ink);
    g.strokePath (blackArc, PathStrokeType (3.5f));
    g.setColour (red);
    g.strokePath (redArc, PathStrokeType (7.0f));

    g.setFont (font (11.0f));
    for (float db : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
    {
        const float a = angleFor (posForDb (db));
        g.setColour (db > 0.0f ? red : ink);
        g.drawLine (Line<float> (polar (pivot, radius + 1.0f, a), polar (pivot, radius + 9.0f, a)), 1.5f);
        if (db == -20.0f || db == -10.0f || db == -5.0f || db == -3.0f || db == 0.0f || db == 3.0f)
        {
            const auto text = (db > 0.0f ? "+" : "") + String ((int) db);
            g.drawText (text, Rectangle<float> (28.0f, 12.0f).withCentre (polar (pivot, radius + 19.0f, a)), Justification::centred);
        }
    }

    // Percentage sub-scale under the arc
    g.setFont (font (8.0f));
    g.setColour (ink.withAlpha (0.75f));
    for (int pct : { 0, 20, 40, 60, 80, 100 })
    {
        const float a = angleFor ((float) pct / 100.0f / 1.4125f);
        g.drawLine (Line<float> (polar (pivot, radius - 2.0f, a), polar (pivot, radius - 7.0f, a)), 1.0f);
        if (pct % 50 == 0)
            g.drawText (String (pct) + (pct == 100 ? "%" : ""), Rectangle<float> (30.0f, 10.0f).withCentre (polar (pivot, radius - 14.0f, a)), Justification::centred);
    }

    g.setColour (ink);
    g.setFont (font (20.0f));
    g.drawText ("VU", Rectangle<float> (face.getX(), face.getBottom() - 66.0f, face.getWidth(), 22.0f), Justification::centred);
    g.setFont (font (9.0f));
    g.setColour (ink.withAlpha (0.7f));
    g.drawText (label, Rectangle<float> (face.getX(), face.getBottom() - 44.0f, face.getWidth(), 12.0f), Justification::centred);

    // Needle and its shadow on the face
    const float a = angleFor (needle);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine (Line<float> (pivot.translated (3.0f, 5.0f), polar (pivot, radius + 12.0f, a).translated (3.0f, 5.0f)), 2.2f);
    g.setColour (juce::Colour (0xff120d0a));
    g.drawLine (Line<float> (pivot, polar (pivot, radius + 12.0f, a)), 1.4f);

    // Pivot cover
    auto dome = Rectangle<float> (face.getWidth() * 0.42f, 22.0f).withCentre ({ face.getCentreX(), face.getBottom() });
    g.setGradientFill (ColourGradient (juce::Colour (0xff3a3632), dome.getCentreX(), dome.getY(),
                                       juce::Colour (0xff0c0b0a), dome.getCentreX(), dome.getBottom(), false));
    g.fillEllipse (dome);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawEllipse (dome.reduced (1.0f), 0.8f);

    // Shadows cast by the bezel, then the glass
    g.setGradientFill (ColourGradient (juce::Colours::black.withAlpha (0.5f), 0.0f, face.getY(),
                                       juce::Colours::transparentBlack, 0.0f, face.getY() + 20.0f, false));
    g.fillRect (face.withHeight (20.0f));
    g.setGradientFill (ColourGradient (juce::Colours::black.withAlpha (0.28f), face.getX(), 0.0f,
                                       juce::Colours::transparentBlack, face.getX() + 12.0f, 0.0f, false));
    g.fillRect (face.withWidth (12.0f));
    g.setGradientFill (ColourGradient (juce::Colours::black.withAlpha (0.28f), face.getRight(), 0.0f,
                                       juce::Colours::transparentBlack, face.getRight() - 12.0f, 0.0f, false));
    g.fillRect (face.withTrimmedLeft (face.getWidth() - 12.0f));

    Path glare;
    glare.startNewSubPath (face.getX(), face.getY());
    glare.lineTo (face.getX() + face.getWidth() * 0.55f, face.getY());
    glare.lineTo (face.getX() + face.getWidth() * 0.25f, face.getBottom());
    glare.lineTo (face.getX(), face.getBottom());
    glare.closeSubPath();
    g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.16f), face.getX(), face.getY(),
                                       juce::Colours::transparentWhite, face.getX() + face.getWidth() * 0.4f, face.getCentreY(), false));
    g.fillPath (glare);

    drawJewel (g, { face.getRight() - 13.0f, face.getY() + 15.0f }, 3.5f, Palette::lampRed, peakLamp);
}

//==============================================================================
void EchoScope::pushHistory (float dry, float wet)
{
    dryHistory[(size_t) writeIndex] = dry;
    wetHistory[(size_t) writeIndex] = wet;
    writeIndex = (writeIndex + 1) % historySize;
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

void EchoScope::paint (juce::Graphics& g)
{
    using namespace juce;
    ++frameCounter;
    const auto inner = drawBezel (g, getLocalBounds().toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff0b0b0a));
    g.fillRect (inner);

    const auto screen = inner.reduced (5.0f);
    Path tube;
    tube.addRoundedRectangle (screen, 18.0f);

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (tube);

        g.setGradientFill (ColourGradient (juce::Colour (0xff0f2318), screen.getCentreX(), screen.getCentreY(),
                                           Palette::crtBack, screen.getX(), screen.getY(), true));
        g.fillRect (screen);

        // Graticule
        g.setColour (Palette::phosphor.withAlpha (0.09f));
        for (int i = 1; i < 10; ++i)
            g.drawVerticalLine ((int) (screen.getX() + screen.getWidth() * (float) i / 10.0f), screen.getY(), screen.getBottom());
        for (int i = 1; i < 6; ++i)
            g.drawHorizontalLine ((int) (screen.getY() + screen.getHeight() * (float) i / 6.0f), screen.getX(), screen.getRight());

        auto content = screen.reduced (14.0f, 8.0f);
        auto top = content.removeFromTop (16.0f);
        auto mapArea = content.removeFromBottom (52.0f);
        content.removeFromBottom (4.0f);

        g.setFont (mono (11.5f));
        g.setColour (Palette::phosphor.withAlpha (0.6f));
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

        // Tube curvature, scan lines and glass
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

void EchoScope::paintHistory (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace juce;
    const float midY = area.getCentreY();
    const float halfH = area.getHeight() * 0.5f;

    g.setColour (Palette::phosphor.withAlpha (0.2f));
    g.drawHorizontalLine ((int) midY, area.getX(), area.getRight());
    for (int i = 0; i <= 50; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / 50.0f;
        g.drawVerticalLine ((int) x, midY - (i % 5 == 0 ? 4.0f : 2.0f), midY + (i % 5 == 0 ? 4.0f : 2.0f));
    }

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
            p.lineTo (area.getX() + area.getWidth() * (float) i / (float) (historySize - 1),
                      midY - toHeight (data[(size_t) ((writeIndex + i) % historySize)]));
        for (int i = historySize - 1; i >= 0; --i)
            p.lineTo (area.getX() + area.getWidth() * (float) i / (float) (historySize - 1),
                      midY + toHeight (data[(size_t) ((writeIndex + i) % historySize)]));
        p.closeSubPath();
        return p;
    };

    const auto dryShape = buildShape (dryHistory);
    const auto wetShape = buildShape (wetHistory);

    g.setColour (Palette::phosphor.withAlpha (0.10f));
    g.fillPath (dryShape);
    g.setColour (Palette::phosphor.withAlpha (0.3f));
    g.strokePath (dryShape, PathStrokeType (0.8f));

    g.setColour (Palette::phosphor.withAlpha (0.22f));
    g.fillPath (wetShape);
    glowStroke (g, wetShape, Palette::phosphor, 1.2f);

    g.setFont (mono (10.0f));
    g.setColour (Palette::phosphor.withAlpha (0.45f));
    g.drawText ("VOICE", area.withWidth (60.0f).withHeight (14.0f), Justification::centredLeft);
    g.setColour (Palette::phosphor);
    g.drawText ("ECHO", area.withTrimmedLeft (52.0f).withWidth (60.0f).withHeight (14.0f), Justification::centredLeft);
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
    area.removeFromLeft (4.0f);
    const float midY = area.getCentreY();
    const float halfH = area.getHeight() * 0.5f - 6.0f;

    g.setFont (mono (10.0f));
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
        g.setColour (Palette::phosphor.withAlpha (0.15f));
        g.strokePath (mark, PathStrokeType (5.0f));
        g.setColour (Palette::phosphor.brighter (0.3f));
        g.fillPath (mark);
    }

    auto status = area.removeFromRight (70.0f).removeFromTop (14.0f);
    if (info.freeze && (frameCounter / 15) % 2 == 0)
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

//==============================================================================
VoxSlapEditor::VoxSlapEditor (VoxSlapProcessor& p)
    : AudioProcessorEditor (p), processor (p),
      modeKnob      (p.apvts, ParamID::mode,        "MODE",          KnobStyle::chicken),
      timeKnob      (p.apvts, ParamID::timeMs,      "TIME",      KnobStyle::aluminium, "auto"),
      divisionKnob  (p.apvts, ParamID::division,    "TIME",    KnobStyle::aluminium, "1/32||1/16|||1/8|||1/4||1/2"),
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
      threshKnob    (p.apvts, ParamID::duckThresh,  "THRESHOLD",     KnobStyle::black, "-60||-30||0"),
      releaseKnob   (p.apvts, ParamID::duckRelease, "RELEASE",       KnobStyle::black, "auto"),
      outputKnob    (p.apvts, ParamID::output,      "OUTPUT",        KnobStyle::black, "-24|||+12")
{
    setLookAndFeel (&lnf);
    buildTextures();

    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("- preset -");
    presetBox.onChange = [this]
    {
        if (const int id = presetBox.getSelectedId(); id > 0)
            processor.presets.loadByIndex (id - 1);
    };

    for (auto* b : { &prevButton, &nextButton, &saveButton, &deleteButton, &simpleButton, &advancedButton })
        addAndMakeVisible (*b);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { savePresetDialog(); };
    deleteButton.onClick = [this] { deletePresetDialog(); };
    simpleButton.onClick = [this] { setAdvanced (false); };
    advancedButton.onClick = [this] { setAdvanced (true); };
    saveButton.setTooltip ("Save current settings as a user preset");
    deleteButton.setTooltip ("Move this user preset to the trash");

    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);
    addAndMakeVisible (scope);

    for (auto* k : { &modeKnob, &timeKnob, &divisionKnob, &feedbackKnob, &driveKnob, &driveTypeKnob, &duckKnob, &mixKnob,
                     &hpfKnob, &lpfKnob, &widthKnob, &offsetKnob, &modRateKnob, &modDepthKnob,
                     &pitchKnob, &threshKnob, &releaseKnob, &outputKnob })
        addAndMakeVisible (*k);

    duckKnob.slider.setTooltip ("UNDER: echoes play under the voice.  AFTER: echoes hide while you sing and come out in the gaps.");
    driveKnob.slider.setTooltip ("Distortion applied to the repeats only - the dry voice stays clean");

    for (auto* t : { &syncToggle, &reverseToggle, &freezeToggle })
        addAndMakeVisible (*t);
    syncAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ParamID::sync, syncToggle);
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

void VoxSlapEditor::buildTextures()
{
    const int unitW = editorWidth - 2 * cheekWidth;
    woodTexture     = tex::wood (cheekWidth * 2, topUnitHeight + bottomUnitHeight, 11);
    burgundyTexture = tex::paintedMetal (unitW, topUnitHeight, Palette::burgundy, 23);
    blackTexture    = tex::paintedMetal (unitW, bottomUnitHeight, Palette::blackPanel, 37, 1.6f);
    plateTexture    = tex::brushedMetal ((unitW - 36) * 2, 60 * 2, Palette::aluminium, 51);
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

    const int h = topUnitHeight + (shouldBeAdvanced ? bottomUnitHeight : 0);
    if (getHeight() != h || getWidth() != editorWidth)
        setSize (editorWidth, h);
    else
        resized();
    repaint();
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
    window->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));

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

    timeKnob.setVisible (! sync);
    divisionKnob.setVisible (sync);
    offsetKnob.setAlpha (mode == 2 ? 1.0f : 0.45f);

    if (processor.presets.getCurrentName() != shownPresetName
        || processor.presets.getUserPresetNames().size() != shownPresetCount)
        refreshPresetList();

    if (isAdvanced() != advancedButton.getToggleState())
        setAdvanced (isAdvanced());
}

//==============================================================================
void VoxSlapEditor::paintUnit (juce::Graphics& g, juce::Rectangle<int> unit, const juce::Image& texture)
{
    using namespace juce;
    const auto r = unit.toFloat();
    g.drawImage (texture, r);

    // Overhead studio light: brighter towards the top.
    g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.07f), r.getCentreX(), r.getY(),
                                       juce::Colours::black.withAlpha (0.22f), r.getCentreX(), r.getBottom(), false));
    g.fillRect (r);

    // Folded edge bevel
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawLine (r.getX(), r.getY() + 0.5f, r.getRight(), r.getY() + 0.5f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawLine (r.getX(), r.getBottom() - 1.0f, r.getRight(), r.getBottom() - 1.0f, 2.0f);

    const float inset = 13.0f;
    drawScrew (g, { r.getX() + inset, r.getY() + inset }, 5.5f, 0.35f);
    drawScrew (g, { r.getRight() - inset, r.getY() + inset }, 5.5f, 1.1f);
    drawScrew (g, { r.getX() + inset, r.getBottom() - inset }, 5.5f, 0.8f);
    drawScrew (g, { r.getRight() - inset, r.getBottom() - inset }, 5.5f, 0.1f);
}

void VoxSlapEditor::paintGroupLabel (juce::Graphics& g, juce::Rectangle<int> span, const juce::String& title)
{
    using namespace juce;
    const auto f = font (10.5f);
    const float tw = GlyphArrangement::getStringWidth (f, title) + 14.0f;
    const auto r = span.toFloat().reduced (10.0f, 0.0f);
    const float y = r.getCentreY();
    const float cx = r.getCentreX();

    g.setColour (Palette::silkscreen.withAlpha (0.8f));
    g.drawLine (r.getX(), y, cx - tw * 0.5f, y, 1.0f);
    g.drawLine (cx + tw * 0.5f, y, r.getRight(), y, 1.0f);
    g.drawLine (r.getX(), y, r.getX(), y + 5.0f, 1.0f);
    g.drawLine (r.getRight(), y, r.getRight(), y + 5.0f, 1.0f);
    g.setFont (f);
    g.drawText (title, Rectangle<float> (tw, 14.0f).withCentre ({ cx, y }), Justification::centred);
}

void VoxSlapEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (juce::Colours::black);

    // Wooden side cheeks
    const int h = getHeight();
    for (int side = 0; side < 2; ++side)
    {
        const Rectangle<int> cheek (side == 0 ? 0 : getWidth() - cheekWidth, 0, cheekWidth, h);
        g.drawImage (woodTexture, cheek.getX(), 0, cheekWidth, h, side * cheekWidth, 0, cheekWidth, h);
        const auto cf = cheek.toFloat();
        ColourGradient varnish (juce::Colours::black.withAlpha (0.45f), cf.getX(), 0.0f,
                                juce::Colours::black.withAlpha (0.45f), cf.getRight(), 0.0f, false);
        varnish.addColour (0.42, juce::Colours::white.withAlpha (0.10f));
        varnish.addColour (0.55, juce::Colours::transparentWhite);
        g.setGradientFill (varnish);
        g.fillRect (cf);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRect (side == 0 ? cf.withTrimmedLeft (cf.getWidth() - 3.0f) : cf.withWidth (3.0f));
    }

    paintUnit (g, topUnit, burgundyTexture);
    if (isAdvanced())
        paintUnit (g, bottomUnit, blackTexture);

    // Brushed aluminium name plate
    {
        const auto plate = namePlate.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (plate.expanded (2.0f).translated (0.0f, 1.5f), 4.0f);
        g.drawImage (plateTexture, plate);
        ColourGradient sheen (juce::Colours::white.withAlpha (0.12f), plate.getX(), plate.getY(),
                              juce::Colours::black.withAlpha (0.18f), plate.getX(), plate.getBottom(), false);
        sheen.addColour (0.45, juce::Colours::white.withAlpha (0.20f));
        g.setGradientFill (sheen);
        g.fillRect (plate);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawLine (plate.getX(), plate.getY() + 0.5f, plate.getRight(), plate.getY() + 0.5f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRect (plate, 1.0f);

        drawScrew (g, { plate.getX() + 13.0f, plate.getCentreY() }, 5.0f, 0.6f);
        drawScrew (g, { plate.getRight() - 13.0f, plate.getCentreY() }, 5.0f, 1.3f);

        // Engraved wordmark: dark fill with a light lower edge
        auto text = [&] (const String& s, Rectangle<float> r, const Font& f)
        {
            g.setFont (f);
            g.setColour (juce::Colours::white.withAlpha (0.65f));
            g.drawText (s, r.translated (0.0f, 1.0f), Justification::centredLeft);
            g.setColour (Palette::engraved);
            g.drawText (s, r, Justification::centredLeft);
        };
        const float lx = plate.getX() + 32.0f;
        text ("VOXSLAP", { lx, plate.getY() + 6.0f, 240.0f, 30.0f }, font (27.0f).withExtraKerningFactor (0.2f));
        text ("VOCAL SLAPBACK ECHO   MODEL VS-1", { lx + 1.0f, plate.getY() + 36.0f, 260.0f, 14.0f },
              font (9.5f).withExtraKerningFactor (0.18f));
    }

    for (auto& [span, title] : groupLabels)
        paintGroupLabel (g, span, title);

    // Footer silkscreen + power jewel
    {
        const auto footer = Rectangle<float> ((float) topUnit.getX() + 34.0f, (float) topUnit.getBottom() - 30.0f,
                                              (float) topUnit.getWidth() - 68.0f, 20.0f);
        g.setFont (font (9.5f).withExtraKerningFactor (0.25f));
        g.setColour (Palette::silkscreen.withAlpha (0.75f));
        g.drawText ("HOMEBREW AUDIO  -  STUDIO ECHO UNIT  -  SER. NO. 0001", footer, Justification::centredLeft);
        g.drawText ("POWER", footer.withTrimmedRight (34.0f), Justification::centredRight);
        drawJewel (g, { footer.getRight() - 12.0f, footer.getCentreY() }, 6.5f, Palette::lampRed, 1.0f);
    }

    if (isAdvanced())
    {
        g.setFont (font (9.0f).withExtraKerningFactor (0.25f));
        g.setColour (Palette::silkscreen.withAlpha (0.55f));
        g.drawText ("VS-1X  EXPANDER", bottomUnit.toFloat().withTrimmedRight (34.0f).withTrimmedBottom (6.0f).removeFromBottom (14.0f),
                    Justification::centredRight);
    }
}

void VoxSlapEditor::resized()
{
    groupLabels.clear();
    topUnit = { cheekWidth, 0, editorWidth - 2 * cheekWidth, topUnitHeight };
    bottomUnit = { cheekWidth, topUnitHeight, editorWidth - 2 * cheekWidth, bottomUnitHeight };

    // ---- Name plate ----
    namePlate = { topUnit.getX() + 18, 16, topUnit.getWidth() - 36, 60 };
    {
        auto p = namePlate.reduced (30, 15);
        p.removeFromLeft (262);
        prevButton.setBounds (p.removeFromLeft (30));
        p.removeFromLeft (6);
        presetBox.setBounds (p.removeFromLeft (220));
        p.removeFromLeft (6);
        nextButton.setBounds (p.removeFromLeft (30));
        p.removeFromLeft (12);
        saveButton.setBounds (p.removeFromLeft (58));
        p.removeFromLeft (6);
        deleteButton.setBounds (p.removeFromLeft (48));

        advancedButton.setBounds (p.removeFromRight (96));
        p.removeFromRight (4);
        simpleButton.setBounds (p.removeFromRight (74));
    }

    auto inner = topUnit.reduced (30, 0);

    // ---- Meter bridge ----
    {
        auto m = juce::Rectangle<int> (inner.getX(), 92, inner.getWidth(), 196);
        inputMeter.setBounds (m.removeFromLeft (214));
        outputMeter.setBounds (m.removeFromRight (214));
        scope.setBounds (m.reduced (14, 0));
    }

    // ---- Main controls ----
    {
        auto row = juce::Rectangle<int> (inner.getX(), 300, inner.getWidth(), 210);
        auto labels = row.removeFromTop (16);
        row.removeFromTop (2);

        auto take = [&] (int w) { labels.removeFromLeft (0); return row.removeFromLeft (w); };
        const int x0 = row.getX();

        modeKnob.setBounds (take (112));
        const int delayStart = row.getX();
        auto timeArea = take (136);
        timeKnob.setBounds (timeArea);
        divisionKnob.setBounds (timeArea);
        syncToggle.setBounds (take (58).withSizeKeepingCentre (58, 120));
        feedbackKnob.setBounds (take (112));
        const int delayEnd = row.getX();
        driveKnob.setBounds (take (112));
        driveTypeKnob.setBounds (take (120));
        const int satEnd = row.getX();
        duckKnob.setBounds (take (122));
        mixKnob.setBounds (row);

        const int y = labels.getY(), lh = labels.getHeight();
        groupLabels.push_back ({ { x0, y, delayStart - x0, lh }, "SELECT" });
        groupLabels.push_back ({ { delayStart, y, delayEnd - delayStart, lh }, "DELAY" });
        groupLabels.push_back ({ { delayEnd, y, satEnd - delayEnd, lh }, "SATURATION" });
        groupLabels.push_back ({ { satEnd, y, row.getRight() - satEnd, lh }, "BLEND" });
    }

    if (! isAdvanced())
        return;

    // ---- Expander unit ----
    {
        auto row = juce::Rectangle<int> (inner.getX(), bottomUnit.getY() + 18, inner.getWidth(), 160);
        auto labels = row.removeFromTop (16);
        row.removeFromTop (2);
        const int y = labels.getY(), lh = labels.getHeight();
        const int kw = 74, tw = 54;

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
        group ({ &reverseToggle, &freezeToggle }, { tw + 6, tw + 6 }, "FX");
        outputKnob.setBounds (row);
        groupLabels.push_back ({ { row.getX(), y, row.getWidth(), lh }, "OUT" });

        for (auto* t : { &reverseToggle, &freezeToggle })
            t->setBounds (t->getBounds().withSizeKeepingCentre (tw + 6, 120));
    }
}
