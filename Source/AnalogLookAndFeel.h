#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

namespace Analog
{
    namespace Palette
    {
        const juce::Colour burgundy   { 0xff4f1f1d };
        const juce::Colour blackPanel { 0xff1b1a19 };
        const juce::Colour aluminium  { 0xffb9b8b2 };
        const juce::Colour silkscreen { 0xffeee3c6 };
        const juce::Colour engraved   { 0xff1f1c19 };
        const juce::Colour amber      { 0xffffb84a };
        const juce::Colour lampRed    { 0xffff3b22 };
        const juce::Colour knobRed    { 0xffb3201f };
        const juce::Colour phosphor   { 0xff74ffa6 };
        const juce::Colour crtBack    { 0xff050c08 };
        const juce::Colour vuFace     { 0xfff2dca6 };
    }

    // Knob angles: 0 = 12 o'clock, clockwise positive (JUCE rotary convention).
    inline juce::Point<float> polar (juce::Point<float> c, float r, float a)
    {
        return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
    }

    inline juce::Font font (float size, bool bold = true)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain))
                   .withExtraKerningFactor (0.08f);
    }

    inline juce::Font mono (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold));
    }

    inline void softShadow (juce::Graphics& g, juce::Point<float> c, float r, float alpha = 0.55f, juce::Point<float> offset = { 3.0f, 5.0f })
    {
        const auto sc = c + offset;
        juce::ColourGradient grad (juce::Colours::black.withAlpha (alpha), sc.x, sc.y,
                                   juce::Colours::transparentBlack, sc.x + r * 1.35f, sc.y, true);
        grad.addColour (0.62, juce::Colours::black.withAlpha (alpha * 0.8f));
        g.setGradientFill (grad);
        g.fillEllipse (sc.x - r * 1.35f, sc.y - r * 1.35f, r * 2.7f, r * 2.7f);
    }

    // Chrome Phillips-head panel screw.
    inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
    {
        softShadow (g, c, r, 0.5f, { 0.8f, 1.5f });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f3ef), c.x - r, c.y - r,
                                                 juce::Colour (0xff6f6c66), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 0.7f);

        for (float a : { angle, angle + juce::MathConstants<float>::halfPi })
        {
            const auto p1 = polar (c, r * 0.68f, a), p2 = polar (c, r * 0.68f, a + juce::MathConstants<float>::pi);
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawLine (p1.x + 0.6f, p1.y + 0.6f, p2.x + 0.6f, p2.y + 0.6f, r * 0.22f);
            g.setColour (juce::Colour (0xff2c2a27));
            g.drawLine (p1.x, p1.y, p2.x, p2.y, r * 0.22f);
        }
    }

    // Faceted jewel indicator lamp in a chrome bezel (UA 175 / Neve style).
    inline void drawJewel (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, float on)
    {
        const float br = r * 1.35f;
        softShadow (g, c, br, 0.5f, { 1.0f, 2.0f });
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff1f0ec), c.x - br, c.y - br,
                                                 juce::Colour (0xff5b5852), c.x + br, c.y + br, false));
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);

        if (on > 0.01f)
        {
            juce::ColourGradient glow (colour.withAlpha (0.5f * on), c.x, c.y, colour.withAlpha (0.0f), c.x + r * 4.0f, c.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (c.x - r * 4.0f, c.y - r * 4.0f, r * 8.0f, r * 8.0f);
        }

        const auto lit = colour.darker (2.2f).interpolatedWith (colour.brighter (0.25f), on);
        g.setGradientFill (juce::ColourGradient (lit.brighter (0.9f * on + 0.15f), c.x - r * 0.25f, c.y - r * 0.3f,
                                                 lit.darker (0.8f), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

        // Facets
        juce::Path facets;
        for (int i = 0; i < 6; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 6.0f;
            facets.startNewSubPath (c);
            facets.lineTo (polar (c, r * 0.92f, a));
        }
        g.setColour (juce::Colours::black.withAlpha (0.08f));
        g.strokePath (facets, juce::PathStrokeType (0.6f));
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.62f, r * 0.5f, r * 0.36f);
    }

    // A rectangular recess with lit bottom wall and shaded top wall, for meters and the scope.
    inline juce::Rectangle<float> drawBezel (juce::Graphics& g, juce::Rectangle<float> outer, float depth)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (outer.translated (1.5f, 3.0f), 6.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4744), 0.0f, outer.getY(),
                                                 juce::Colour (0xff141312), 0.0f, outer.getBottom(), false));
        g.fillRoundedRectangle (outer, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawLine (outer.getX() + 6.0f, outer.getY() + 0.8f, outer.getRight() - 6.0f, outer.getY() + 0.8f, 1.0f);

        const auto lip = outer.reduced (5.0f);
        const auto inner = lip.reduced (depth);
        auto wall = [&] (juce::Point<float> a, juce::Point<float> b, juce::Point<float> c, juce::Point<float> d, juce::Colour col)
        {
            juce::Path p;
            p.startNewSubPath (a); p.lineTo (b); p.lineTo (c); p.lineTo (d); p.closeSubPath();
            g.setColour (col);
            g.fillPath (p);
        };
        wall (lip.getTopLeft(), lip.getTopRight(), inner.getTopRight(), inner.getTopLeft(), juce::Colour (0xff0b0a0a));
        wall (lip.getBottomLeft(), lip.getBottomRight(), inner.getBottomRight(), inner.getBottomLeft(), juce::Colour (0xff57534d));
        wall (lip.getTopLeft(), lip.getBottomLeft(), inner.getBottomLeft(), inner.getTopLeft(), juce::Colour (0xff24221f));
        wall (lip.getTopRight(), lip.getBottomRight(), inner.getBottomRight(), inner.getTopRight(), juce::Colour (0xff302d2a));
        return inner;
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            using namespace Palette;
            setColour (juce::ComboBox::textColourId, amber);
            setColour (juce::ComboBox::arrowColourId, amber);
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff17130f));
            setColour (juce::PopupMenu::textColourId, amber);
            setColour (juce::PopupMenu::headerTextColourId, silkscreen.withAlpha (0.6f));
            setColour (juce::PopupMenu::highlightedBackgroundColourId, amber.withAlpha (0.22f));
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::TextButton::textColourOffId, engraved);
            setColour (juce::TextButton::textColourOnId, engraved);
            setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff2a2421));
            setColour (juce::AlertWindow::textColourId, silkscreen);
            setColour (juce::AlertWindow::outlineColourId, juce::Colours::black);
            setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff120e0b));
            setColour (juce::TextEditor::textColourId, amber);
            setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff5a524a));
            setColour (juce::TextEditor::focusedOutlineColourId, amber.withAlpha (0.6f));
            setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff17130f));
            setColour (juce::TooltipWindow::textColourId, silkscreen);
        }

        //======================================================================
        // Knobs. Slider properties: "knob" = alu | black | chicken; "labels" = "a|b|c";
        // "ink" = colour string for the printed scale.
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider& slider) override
        {
            using namespace juce;
            g.setImageResamplingQuality (Graphics::highResamplingQuality);

            const auto& props = slider.getProperties();
            const String style = props.getWithDefault ("knob", "black").toString();
            const Colour ink = Colour::fromString (props.getWithDefault ("ink", Palette::silkscreen.toString()).toString());
            StringArray labels = StringArray::fromTokens (props.getWithDefault ("labels", "0|1|2|3|4|5|6|7|8|9|10").toString(), "|", "");

            const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            const float R = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto c = bounds.getCentre();
            const float angle = startAngle + pos * (endAngle - startAngle);
            const bool chicken = style == "chicken";
            const float knobR = R * (chicken ? 0.48f : 0.52f);

            // Printed scale: dots + labels
            const int n = labels.size();
            const float dotR = R * 0.68f, labelR = R * 0.84f;
            const float fontSize = jlimit (8.0f, 11.0f, R * 0.17f);
            g.setFont (font (fontSize));
            for (int i = 0; i < n; ++i)
            {
                const float a = startAngle + (n > 1 ? (float) i / (float) (n - 1) : 0.5f) * (endAngle - startAngle);
                const auto p = polar (c, dotR, a);
                g.setColour (ink);
                g.fillEllipse (p.x - 1.7f, p.y - 1.7f, 3.4f, 3.4f);

                if (! chicken && i < n - 1 && n <= 11)
                {
                    const float am = a + 0.5f * (endAngle - startAngle) / (float) (n - 1);
                    const auto pm = polar (c, dotR, am);
                    g.setColour (ink.withAlpha (0.7f));
                    g.fillEllipse (pm.x - 1.0f, pm.y - 1.0f, 2.0f, 2.0f);
                }

                if (labels[i].isNotEmpty())
                {
                    const auto lp = polar (c, labelR, a);
                    const float s = std::sin (a);
                    const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), labels[i]) + 2.0f;
                    Rectangle<float> box (tw, fontSize + 2.0f);
                    Justification just = Justification::centred;
                    if (chicken)         box.setCentre (polar (c, R * 0.92f, a));
                    else if (s < -0.35f)      { box.setPosition (lp.x - tw + 4.0f, lp.y - box.getHeight() * 0.5f); just = Justification::centredRight; }
                    else if (s > 0.35f)  { box.setPosition (lp.x - 4.0f, lp.y - box.getHeight() * 0.5f); just = Justification::centredLeft; }
                    else                 box.setCentre (lp);
                    g.setColour (ink);
                    g.drawText (labels[i], box, just, false);
                }
            }

            if (chicken)
            {
                drawChickenHead (g, c, knobR, angle);
                return;
            }

            const auto img = getKnobImage (style, knobR);
            const float imgSize = knobR * 2.0f * imagePad;
            g.drawImage (img, Rectangle<float> (imgSize, imgSize).withCentre (c));

            if (style == "alu")
            {
                const auto p1 = polar (c, knobR * 0.18f, angle), p2 = polar (c, knobR * 0.78f, angle);
                g.setColour (juce::Colours::white.withAlpha (0.6f));
                g.drawLine (p1.x + 0.7f, p1.y + 0.9f, p2.x + 0.7f, p2.y + 0.9f, 1.2f);
                g.setColour (juce::Colour (0xff1c1a18));
                g.drawLine (p1.x, p1.y, p2.x, p2.y, jmax (1.6f, knobR * 0.045f));
            }
            else
            {
                const auto p1 = polar (c, knobR * 0.2f, angle), p2 = polar (c, knobR * 0.98f, angle);
                g.setColour (juce::Colour (0xfff2ede0));
                g.drawLine (p1.x, p1.y, p2.x, p2.y, jmax (2.0f, knobR * 0.075f));
            }
        }

        // Red bakelite pointer knob (Neve-style), used for stepped selectors.
        void drawChickenHead (juce::Graphics& g, juce::Point<float> c, float r, float angle)
        {
            using namespace juce;
            Path body;
            body.startNewSubPath (-0.17f * r, -1.08f * r);
            body.quadraticTo (0.0f, -1.16f * r, 0.17f * r, -1.08f * r);
            body.lineTo (0.46f * r, 0.0f);
            body.lineTo (0.30f * r, 0.78f * r);
            body.quadraticTo (0.0f, 0.9f * r, -0.30f * r, 0.78f * r);
            body.lineTo (-0.46f * r, 0.0f);
            body.closeSubPath();
            body.addEllipse (-0.66f * r, -0.66f * r, 1.32f * r, 1.32f * r);
            body.setUsingNonZeroWinding (true);

            const auto t = AffineTransform::rotation (angle).translated (c);
            Path shaped (body);
            shaped.applyTransform (t);

            DropShadow (juce::Colours::black.withAlpha (0.6f), (int) (r * 0.35f), { (int) (r * 0.12f) + 1, (int) (r * 0.2f) + 2 })
                .drawForPath (g, shaped);

            g.setGradientFill (ColourGradient (Palette::knobRed.brighter (0.35f), c.x - r, c.y - r,
                                               Palette::knobRed.darker (0.9f), c.x + r, c.y + r, false));
            g.fillPath (shaped);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.strokePath (shaped, PathStrokeType (0.9f));

            // Glossy dome on the hub
            g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.38f), c.x - r * 0.3f, c.y - r * 0.38f,
                                               juce::Colours::transparentWhite, c.x + r * 0.2f, c.y + r * 0.2f, true));
            g.fillEllipse (c.x - r * 0.62f, c.y - r * 0.62f, r * 1.24f, r * 1.24f);

            // Grey centre insert and pointer line
            const float ir = r * 0.36f;
            g.setGradientFill (ColourGradient (juce::Colour (0xff9a958c), c.x - ir, c.y - ir,
                                               juce::Colour (0xff3b3833), c.x + ir, c.y + ir, false));
            g.fillEllipse (c.x - ir, c.y - ir, ir * 2.0f, ir * 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawEllipse (c.x - ir, c.y - ir, ir * 2.0f, ir * 2.0f, 0.8f);

            const auto p1 = polar (c, r * 0.45f, angle), p2 = polar (c, r * 1.02f, angle);
            g.setColour (juce::Colour (0xfff5efe2));
            g.drawLine (p1.x, p1.y, p2.x, p2.y, jmax (1.6f, r * 0.07f));
        }

        //======================================================================
        // Chrome bat-handle toggle. Properties: "ink".
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
        {
            using namespace juce;
            const Colour ink = Colour::fromString (b.getProperties().getWithDefault ("ink", Palette::silkscreen.toString()).toString());
            auto area = b.getLocalBounds().toFloat();
            const bool on = b.getToggleState();

            g.setColour (ink.withAlpha (highlighted ? 1.0f : 0.92f));
            g.setFont (font (11.5f));
            g.drawText (b.getButtonText(), area.removeFromTop (20.0f), Justification::centred);

            const auto c = area.getCentre();
            const float lever = jmin (30.0f, area.getHeight() * 0.36f);

            g.setFont (font (9.0f));
            g.setColour (ink.withAlpha (0.85f));
            g.drawText ("ON", Rectangle<float> (c.x + 14.0f, c.y - lever - 2.0f, 30.0f, 12.0f), Justification::centredLeft);
            g.drawText ("OFF", Rectangle<float> (c.x + 14.0f, c.y + lever - 10.0f, 30.0f, 12.0f), Justification::centredLeft);

            // Mounting ring + hex nut
            softShadow (g, c, 15.0f, 0.5f, { 1.0f, 2.5f });
            g.setGradientFill (ColourGradient (juce::Colour (0xfff7f6f2), c.x - 15.0f, c.y - 15.0f,
                                               juce::Colour (0xff55524c), c.x + 15.0f, c.y + 15.0f, false));
            g.fillEllipse (c.x - 15.0f, c.y - 15.0f, 30.0f, 30.0f);
            Path hex;
            for (int i = 0; i < 6; ++i)
            {
                const auto p = polar (c, 10.5f, MathConstants<float>::pi / 3.0f * (float) i + 0.52f);
                i == 0 ? hex.startNewSubPath (p) : hex.lineTo (p);
            }
            hex.closeSubPath();
            g.setGradientFill (ColourGradient (juce::Colour (0xffe9e7e1), c.x + 8.0f, c.y - 10.0f,
                                               juce::Colour (0xff6a665f), c.x - 8.0f, c.y + 10.0f, false));
            g.fillPath (hex);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.strokePath (hex, PathStrokeType (0.7f));
            g.setColour (juce::Colour (0xff141210));
            g.fillEllipse (c.x - 4.5f, c.y - 4.5f, 9.0f, 9.0f);

            // Lever with shadow
            const float tipY = on ? c.y - lever : c.y + lever;
            Path bat;
            bat.startNewSubPath (c.x - 3.0f, c.y);
            bat.lineTo (c.x - 4.2f, tipY);
            bat.lineTo (c.x + 4.2f, tipY);
            bat.lineTo (c.x + 3.0f, c.y);
            bat.closeSubPath();

            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillPath (bat, AffineTransform::translation (3.0f, on ? 3.0f : 6.0f));
            g.fillEllipse (c.x - 6.0f + 3.0f, tipY - 6.0f + (on ? 3.0f : 6.0f), 12.0f, 12.0f);

            ColourGradient chrome (juce::Colour (0xff7b7871), c.x - 4.5f, c.y, juce::Colour (0xff5d5a54), c.x + 4.5f, c.y, false);
            chrome.addColour (0.35, juce::Colour (0xfffbfaf7));
            chrome.addColour (0.6, juce::Colour (0xffb9b6af));
            g.setGradientFill (chrome);
            g.fillPath (bat);

            g.setGradientFill (ColourGradient (juce::Colours::white, c.x - 2.5f, tipY - 3.0f,
                                               juce::Colour (0xff5f5c56), c.x + 5.0f, tipY + 5.0f, true));
            g.fillEllipse (c.x - 6.0f, tipY - 6.0f, 12.0f, 12.0f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawEllipse (c.x - 6.0f, tipY - 6.0f, 12.0f, 12.0f, 0.6f);
        }

        //======================================================================
        // Square cream push keys (Neve EQL/PHASE style); glow from behind when engaged.
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                   bool highlighted, bool down) override
        {
            using namespace juce;
            auto r = b.getLocalBounds().toFloat().reduced (1.5f);
            const bool on = b.getToggleState();

            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillRoundedRectangle (r.expanded (1.5f), 3.0f);

            if (! down)
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 2.5f);
            }
            if (down) r.translate (0.0f, 1.0f);

            Colour top = on ? juce::Colour (0xfffff0c8) : juce::Colour (0xffeee8d8);
            Colour bottom = on ? juce::Colour (0xfff2b65a) : juce::Colour (0xffbfb8a3);
            if (highlighted) { top = top.brighter (0.05f); bottom = bottom.brighter (0.05f); }
            g.setGradientFill (ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 2.5f);

            if (on)
            {
                g.setGradientFill (ColourGradient (Palette::amber.withAlpha (0.55f), r.getCentreX(), r.getCentreY(),
                                                   Palette::amber.withAlpha (0.0f), r.getRight(), r.getBottom(), true));
                g.fillRoundedRectangle (r, 2.5f);
            }

            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.drawLine (r.getX() + 2.0f, r.getY() + 0.8f, r.getRight() - 2.0f, r.getY() + 0.8f, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawRoundedRectangle (r, 2.5f, 0.8f);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
        {
            g.setColour (Palette::engraved.withMultipliedAlpha (b.isEnabled() ? 0.9f : 0.3f));
            g.setFont (font (juce::jmin (12.0f, (float) b.getHeight() * 0.45f)));
            g.drawText (b.getButtonText(), b.getLocalBounds().translated (0, down ? 1 : 0), juce::Justification::centred);
        }

        //======================================================================
        // Preset window: smoked glass with an amber vacuum-fluorescent readout.
        juce::Font getComboBoxFont (juce::ComboBox&) override { return mono (15.0f); }
        juce::Font getPopupMenuFont() override { return mono (14.0f); }

        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
        {
            using namespace juce;
            auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
            g.setColour (juce::Colour (0xff0d0b09));
            g.fillRoundedRectangle (r, 3.0f);
            auto glass = r.reduced (2.5f);
            g.setGradientFill (ColourGradient (juce::Colour (0xff1d1510), 0.0f, glass.getY(),
                                               juce::Colour (0xff0b0806), 0.0f, glass.getBottom(), false));
            g.fillRoundedRectangle (glass, 2.0f);
            g.setGradientFill (ColourGradient (juce::Colours::black.withAlpha (0.6f), 0.0f, glass.getY(),
                                               juce::Colours::transparentBlack, 0.0f, glass.getY() + 7.0f, false));
            g.fillRect (glass.withHeight (7.0f));
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRect (glass.reduced (2.0f).withHeight (glass.getHeight() * 0.4f));
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            g.drawLine (r.getX() + 3.0f, r.getBottom() - 0.6f, r.getRight() - 3.0f, r.getBottom() - 0.6f, 1.0f);

            Path arrow;
            const float ax = (float) w - 15.0f, ay = (float) h * 0.5f;
            arrow.addTriangle (ax - 5.0f, ay - 2.5f, ax + 5.0f, ay - 2.5f, ax, ay + 3.5f);
            g.setColour (Palette::amber.withAlpha (0.85f));
            g.fillPath (arrow);
        }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
            label.setFont (getComboBoxFont (box));
        }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
            g.setColour (juce::Colour (0xff5a5048));
            g.drawRect (0, 0, w, h, 1);
        }

        static constexpr float imagePad = 1.45f;

    private:
        // Knob bodies are rendered once per size: the lighting is fixed, so only the pointer moves.
        juce::Image getKnobImage (const juce::String& style, float knobR)
        {
            const int key = juce::roundToInt (knobR * 4.0f);
            auto id = style + juce::String (key);
            if (auto it = knobCache.find (id); it != knobCache.end())
                return it->second;

            const float scale = 2.0f;
            const float r = knobR * scale;
            const int size = (int) std::ceil (r * 2.0f * imagePad);
            juce::Image img (juce::Image::ARGB, size, size, true);
            juce::Graphics g (img);
            const juce::Point<float> c ((float) size * 0.5f, (float) size * 0.5f);

            softShadow (g, c, r, 0.6f, { r * 0.1f, r * 0.17f });

            if (style == "alu") renderAluminium (g, c, r);
            else                renderBlack (g, c, r);

            knobCache[id] = img;
            return img;
        }

        static void knurl (juce::Graphics& g, juce::Point<float> c, float rOuter, float innerFrac, int ridges,
                           float base, float range, juce::Colour tint)
        {
            using namespace juce;
            const float light = -MathConstants<float>::pi * 0.25f;
            for (int i = 0; i < ridges; ++i)
            {
                const float a0 = MathConstants<float>::twoPi * (float) i / (float) ridges;
                const float a1 = MathConstants<float>::twoPi * (float) (i + 1) / (float) ridges;
                float l = base + range * std::cos (0.5f * (a0 + a1) - light) + ((i % 2) ? 0.07f : -0.07f);
                l = jlimit (0.0f, 1.0f, l);
                Path seg;
                seg.addPieSegment (c.x - rOuter, c.y - rOuter, rOuter * 2.0f, rOuter * 2.0f, a0, a1 + 0.004f, innerFrac);
                g.setColour (Colour::fromFloatRGBA (l * tint.getFloatRed(), l * tint.getFloatGreen(), l * tint.getFloatBlue(), 1.0f));
                g.fillPath (seg);
            }
        }

        static void renderAluminium (juce::Graphics& g, juce::Point<float> c, float r)
        {
            using namespace juce;
            knurl (g, c, r, 0.82f, 96, 0.58f, 0.32f, juce::Colour (0xfff2f2f6));

            // Machined top face: anisotropic "bow-tie" sheen + concentric lathe rings.
            const float fr = r * 0.82f;
            const float light = -MathConstants<float>::pi * 0.25f;
            for (int i = 0; i < 180; ++i)
            {
                const float a0 = MathConstants<float>::twoPi * (float) i / 180.0f;
                const float a1 = MathConstants<float>::twoPi * (float) (i + 1) / 180.0f;
                const float sheen = std::pow (std::abs (std::cos (0.5f * (a0 + a1) - light - MathConstants<float>::halfPi)), 4.0f);
                const float l = 0.6f + 0.33f * sheen;
                Path seg;
                seg.addPieSegment (c.x - fr, c.y - fr, fr * 2.0f, fr * 2.0f, a0, a1 + 0.01f, 0.0f);
                g.setColour (Colour::fromFloatRGBA (l, l, l * 1.01f, 1.0f));
                g.fillPath (seg);
            }
            Random rng (77);
            for (float rr = 2.0f; rr < fr; rr += 1.3f)
            {
                g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (rng.nextFloat() * 0.07f));
                g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 0.8f);
            }

            // Bevel at the edge of the face
            g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.9f), c.x - fr, c.y - fr,
                                               juce::Colours::black.withAlpha (0.6f), c.x + fr, c.y + fr, false));
            g.drawEllipse (c.x - fr, c.y - fr, fr * 2.0f, fr * 2.0f, r * 0.035f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);
        }

        static void renderBlack (juce::Graphics& g, juce::Point<float> c, float r)
        {
            using namespace juce;
            // Knurled skirt
            knurl (g, c, r, 0.80f, 64, 0.16f, 0.12f, juce::Colour (0xfff0f0f0));

            // Flat top of the skirt
            const float sr = r * 0.80f;
            g.setGradientFill (ColourGradient (juce::Colour (0xff3d3b38), c.x - sr, c.y - sr,
                                               juce::Colour (0xff0b0a0a), c.x + sr, c.y + sr, false));
            g.fillEllipse (c.x - sr, c.y - sr, sr * 2.0f, sr * 2.0f);

            // Raised cap with its own shadow and glossy dome
            const float cr = r * 0.62f;
            softShadow (g, c, cr, 0.55f, { r * 0.05f, r * 0.08f });
            g.setGradientFill (ColourGradient (juce::Colour (0xff4b4844), c.x - cr * 0.5f, c.y - cr * 0.6f,
                                               juce::Colour (0xff090808), c.x + cr * 0.7f, c.y + cr * 0.8f, true));
            g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
            g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.28f), c.x - cr * 0.35f, c.y - cr * 0.55f,
                                               juce::Colours::transparentWhite, c.x - cr * 0.35f, c.y + cr * 0.1f, false));
            g.fillEllipse (c.x - cr * 0.75f, c.y - cr * 0.85f, cr * 1.2f, cr * 0.8f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.2f);
        }

        std::map<juce::String, juce::Image> knobCache;
    };
}
