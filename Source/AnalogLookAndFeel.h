#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

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

            const auto& sheet = style == "alu" ? aluSheet : style == "chicken" ? chickenSheet : blackSheet;
            drawKnobFrame (g, sheet, angle, Rectangle<float> (knobR * 2.0f * imagePad, knobR * 2.0f * imagePad).withCentre (c));
        }

        // Knob sprite sheets: 120 Blender-rendered frames, 3 degrees apart, clockwise from 12 o'clock.
        static void drawKnobFrame (juce::Graphics& g, const juce::Image& sheet, float angle, juce::Rectangle<float> dest)
        {
            constexpr int frames = 120, cols = 12;
            const float turn = juce::MathConstants<float>::twoPi;
            float a = std::fmod (angle, turn);
            if (a < 0.0f) a += turn;
            const int frame = juce::roundToInt (a / turn * (float) frames) % frames;
            const int fw = sheet.getWidth() / cols;
            const int fh = sheet.getHeight() / ((frames + cols - 1) / cols);
            g.drawImage (sheet, juce::roundToInt (dest.getX()), juce::roundToInt (dest.getY()),
                         juce::roundToInt (dest.getWidth()), juce::roundToInt (dest.getHeight()),
                         (frame % cols) * fw, (frame / cols) * fh, fw, fh);
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

            // Rendered switch: frame 0 = on (lever up), 1 = off. The ring is 0.9 of 2.6 half-width units.
            const float size = 15.0f / 0.9f * 2.6f * 2.0f;
            const int fw = toggleSheet.getWidth() / 2;
            g.drawImage (toggleSheet, roundToInt (c.x - size * 0.5f), roundToInt (c.y - size * 0.5f), roundToInt (size), roundToInt (size),
                         on ? 0 : fw, 0, fw, toggleSheet.getHeight());
        }

        //======================================================================
        // Square cream push keys (Neve EQL/PHASE style); glow from behind when engaged.
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                   bool highlighted, bool down) override
        {
            using namespace juce;
            const int frame = down ? 2 : b.getToggleState() ? 1 : 0;
            const int fw = keySheet.getWidth(), fh = keySheet.getHeight() / 3, sy = frame * fh;

            // The surround fills 120 of the 140 rendered rows; scale so it matches the button height.
            const auto r = b.getLocalBounds().toFloat();
            const float scale = r.getHeight() / ((float) fh * 120.0f / 140.0f);
            const float dh = (float) fh * scale;
            const float y = r.getCentreY() - dh * 0.5f;
            const int slice = fw / 5;
            const float ds = (float) slice * scale;
            const float extra = (float) fw * scale * (8.0f / 300.0f); // transparent margin around the surround
            const float x0 = r.getX() - extra, x1 = r.getRight() + extra;

            g.setOpacity (b.isEnabled() ? 1.0f : 0.55f);
            g.drawImage (keySheet, roundToInt (x0), roundToInt (y), roundToInt (ds), roundToInt (dh), 0, sy, slice, fh);
            g.drawImage (keySheet, roundToInt (x0 + ds), roundToInt (y), roundToInt (x1 - x0 - 2.0f * ds), roundToInt (dh),
                         slice, sy, fw - 2 * slice, fh);
            g.drawImage (keySheet, roundToInt (x1 - ds), roundToInt (y), roundToInt (ds), roundToInt (dh), fw - slice, sy, slice, fh);
            g.setOpacity (1.0f);

            if (highlighted && ! down)
            {
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.fillRoundedRectangle (r.reduced (3.0f), 2.0f);
            }
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
        static juce::Image load (const char* data, int size) { return juce::ImageCache::getFromMemory (data, size); }

        juce::Image aluSheet     = load (BinaryData::knob_alu_png, BinaryData::knob_alu_pngSize);
        juce::Image blackSheet   = load (BinaryData::knob_black_png, BinaryData::knob_black_pngSize);
        juce::Image chickenSheet = load (BinaryData::knob_chicken_png, BinaryData::knob_chicken_pngSize);
        juce::Image toggleSheet  = load (BinaryData::toggle_png, BinaryData::toggle_pngSize);
        juce::Image keySheet     = load (BinaryData::key_png, BinaryData::key_pngSize);
    };
}
