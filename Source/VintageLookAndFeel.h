#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Vintage
{
    namespace Palette
    {
        const juce::Colour panel      { 0xffd8ceaf };
        const juce::Colour panelDark  { 0xffc4b791 };
        const juce::Colour header     { 0xff2a2622 };
        const juce::Colour headerEdge { 0xff141210 };
        const juce::Colour ink        { 0xff3a2d20 };
        const juce::Colour inkSoft    { 0xff6b5a44 };
        const juce::Colour brass      { 0xffcfa75c };
        const juce::Colour brassDark  { 0xff8a6a32 };
        const juce::Colour amber      { 0xffffb444 };
        const juce::Colour lampRed    { 0xffe5402c };
        const juce::Colour bakelite   { 0xff1d1916 };
        const juce::Colour crtBack    { 0xff08160d };
        const juce::Colour phosphor   { 0xff7dffa0 };
        const juce::Colour vuFace     { 0xfff3e2b0 };
    }

    inline juce::Font font (float size, bool bold = true)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain))
                   .withExtraKerningFactor (0.06f);
    }

    inline juce::Font mono (float size)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold));
    }

    inline void drawScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (c.x - r + 0.8f, c.y - r + 1.2f, r * 2.0f, r * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8e4da), c.x - r, c.y - r,
                                                 juce::Colour (0xff8d877a), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colour (0xff4b463d));
        juce::Path slot;
        slot.addRectangle (-r * 0.8f, -r * 0.13f, r * 1.6f, r * 0.26f);
        g.fillPath (slot, juce::AffineTransform::rotation (angle).translated (c));
    }

    // Small round indicator lamp.
    inline void drawLamp (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, float brightness)
    {
        g.setColour (juce::Colour (0xff1a1714));
        g.fillEllipse (c.x - r - 2.0f, c.y - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f);

        if (brightness > 0.01f)
        {
            juce::ColourGradient glow (colour.withAlpha (0.45f * brightness), c.x, c.y,
                                       colour.withAlpha (0.0f), c.x + r * 3.2f, c.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (c.x - r * 3.2f, c.y - r * 3.2f, r * 6.4f, r * 6.4f);
        }

        const auto lit = colour.darker (1.6f).interpolatedWith (colour.brighter (0.4f), brightness);
        g.setGradientFill (juce::ColourGradient (lit.brighter (0.6f), c.x - r * 0.4f, c.y - r * 0.5f,
                                                 lit.darker (0.5f), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.fillEllipse (c.x - r * 0.5f, c.y - r * 0.6f, r * 0.6f, r * 0.45f);
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel()
        {
            using namespace Palette;
            setColour (juce::ComboBox::backgroundColourId, crtBack);
            setColour (juce::ComboBox::textColourId, amber);
            setColour (juce::ComboBox::outlineColourId, headerEdge);
            setColour (juce::ComboBox::arrowColourId, amber);
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1815));
            setColour (juce::PopupMenu::textColourId, amber);
            setColour (juce::PopupMenu::headerTextColourId, brass);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, amber.withAlpha (0.25f));
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::TextButton::textColourOffId, ink);
            setColour (juce::TextButton::textColourOnId, ink);
            setColour (juce::AlertWindow::backgroundColourId, panel);
            setColour (juce::AlertWindow::textColourId, ink);
            setColour (juce::AlertWindow::outlineColourId, ink);
            setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xfff6efd9));
            setColour (juce::TextEditor::textColourId, ink);
            setColour (juce::TextEditor::outlineColourId, inkSoft);
            setColour (juce::TextEditor::focusedOutlineColourId, brassDark);
        }

        // Bakelite knob on a knurled aluminium skirt, with engraved scale ticks.
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider& slider) override
        {
            using namespace juce;
            const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
            const auto c = bounds.getCentre();
            const float angle = startAngle + pos * (endAngle - startAngle);
            const bool isChoice = slider.getProperties().contains ("stepped");
            const int numTicks = isChoice ? (int) slider.getMaximum() + 1 : 11;

            // Scale ticks
            for (int i = 0; i < numTicks; ++i)
            {
                const float a = startAngle + (float) i / (float) (numTicks - 1) * (endAngle - startAngle);
                const bool major = isChoice || i % 5 == 0;
                const float r1 = r * (major ? 0.80f : 0.84f), r2 = r * 0.97f;
                g.setColour (Palette::ink.withAlpha (major ? 0.9f : 0.55f));
                g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a),
                            c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), major ? 1.8f : 1.1f);
            }

            // Arc showing current value
            Path arc;
            arc.addCentredArc (c.x, c.y, r * 0.88f, r * 0.88f, 0.0f, startAngle, angle, true);
            g.setColour (Palette::lampRed.withAlpha (0.55f));
            g.strokePath (arc, PathStrokeType (2.0f));

            const float skirtR = r * 0.74f;
            const float capR = r * 0.56f;

            // Drop shadow
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillEllipse (c.x - skirtR + 2.0f, c.y - skirtR + 4.0f, skirtR * 2.0f, skirtR * 2.0f);

            // Aluminium skirt with knurling
            g.setGradientFill (ColourGradient (juce::Colour (0xfff0eee8), c.x - skirtR, c.y - skirtR,
                                               juce::Colour (0xff6f6a62), c.x + skirtR, c.y + skirtR, false));
            g.fillEllipse (c.x - skirtR, c.y - skirtR, skirtR * 2.0f, skirtR * 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            for (int i = 0; i < 48; ++i)
            {
                const float a = angle + (float) i * MathConstants<float>::twoPi / 48.0f;
                g.drawLine (c.x + capR * 1.05f * std::sin (a), c.y - capR * 1.05f * std::cos (a),
                            c.x + skirtR * 0.98f * std::sin (a), c.y - skirtR * 0.98f * std::cos (a), 0.8f);
            }

            // Bakelite cap
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillEllipse (c.x - capR - 1.0f, c.y - capR + 1.0f, capR * 2.0f + 2.0f, capR * 2.0f + 2.0f);
            g.setGradientFill (ColourGradient (juce::Colour (0xff4a423b), c.x - capR * 0.6f, c.y - capR * 0.8f,
                                               Palette::bakelite, c.x + capR * 0.5f, c.y + capR * 0.7f, true));
            g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillEllipse (c.x - capR * 0.7f, c.y - capR * 0.85f, capR * 1.0f, capR * 0.6f);

            // Pointer line on the cap
            const float p1 = capR * 0.15f, p2 = skirtR * 0.96f;
            g.setColour (juce::Colour (0xfff4ead0));
            g.drawLine (c.x + p1 * std::sin (angle), c.y - p1 * std::cos (angle),
                        c.x + p2 * std::sin (angle), c.y - p2 * std::cos (angle), jmax (2.0f, r * 0.06f));
        }

        // Bat-handle toggle switch with indicator lamp; the button text is drawn below.
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
        {
            using namespace juce;
            auto area = b.getLocalBounds().toFloat();
            const bool on = b.getToggleState();
            auto textArea = area.removeFromBottom (18.0f);

            const float cx = area.getCentreX();
            drawLamp (g, { cx, area.getY() + 9.0f }, 5.0f, Palette::lampRed, on ? 1.0f : 0.0f);

            const float plateY = area.getY() + 24.0f;
            const float plateH = area.getHeight() - 26.0f;
            const float cy = plateY + plateH * 0.5f;

            // Hex nut
            Path hex;
            const float hr = jmin (14.0f, plateH * 0.3f);
            for (int i = 0; i < 6; ++i)
            {
                const float a = MathConstants<float>::pi / 3.0f * (float) i;
                const Point<float> p (cx + hr * std::cos (a), cy + hr * std::sin (a));
                i == 0 ? hex.startNewSubPath (p) : hex.lineTo (p);
            }
            hex.closeSubPath();
            g.setGradientFill (ColourGradient (juce::Colour (0xffe6e2d8), cx - hr, cy - hr,
                                               juce::Colour (0xff7d776b), cx + hr, cy + hr, false));
            g.fillPath (hex);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.strokePath (hex, PathStrokeType (0.8f));

            // Lever
            const float leverLen = jmin (26.0f, plateH * 0.48f);
            const float tipY = on ? cy - leverLen : cy + leverLen;
            Path lever;
            lever.startNewSubPath (cx - 3.0f, cy);
            lever.lineTo (cx - 4.5f, tipY);
            lever.lineTo (cx + 4.5f, tipY);
            lever.lineTo (cx + 3.0f, cy);
            lever.closeSubPath();
            g.setGradientFill (ColourGradient (juce::Colour (0xfffbfaf6), cx - 5.0f, cy,
                                               juce::Colour (0xff8f897d), cx + 5.0f, cy, false));
            g.fillPath (lever);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.strokePath (lever, PathStrokeType (0.8f));
            g.setColour (juce::Colour (0xffd9d5ca));
            g.fillEllipse (cx - 5.5f, tipY - 5.5f, 11.0f, 11.0f);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawEllipse (cx - 5.5f, tipY - 5.5f, 11.0f, 11.0f, 0.8f);

            g.setColour (Palette::ink.withAlpha (highlighted ? 1.0f : 0.85f));
            g.setFont (font (12.0f));
            g.drawText (b.getButtonText(), textArea, Justification::centred);
        }

        // Cream push-button keys; lit amber when engaged.
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                   bool highlighted, bool down) override
        {
            using namespace juce;
            auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            const bool on = b.getToggleState();
            const bool dark = b.getProperties().contains ("dark");

            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (r.translated (0.0f, down ? 0.5f : 1.5f), 3.0f);

            Colour top, bottom;
            if (on)            { top = Palette::amber.brighter (0.3f); bottom = Palette::amber.darker (0.4f); }
            else if (dark)     { top = juce::Colour (0xff4a443d); bottom = juce::Colour (0xff2b2723); }
            else               { top = juce::Colour (0xfff2ead3); bottom = juce::Colour (0xffc5b993); }
            if (highlighted && ! on) { top = top.brighter (0.08f); bottom = bottom.brighter (0.08f); }
            if (down)          std::swap (top, bottom);

            if (down) r.translate (0.0f, 1.0f);
            g.setGradientFill (ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawRoundedRectangle (r, 3.0f, 1.0f);

            if (on)
            {
                g.setColour (Palette::amber.withAlpha (0.25f));
                g.drawRoundedRectangle (r.expanded (2.0f), 4.0f, 2.0f);
            }
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
        {
            const bool dark = b.getProperties().contains ("dark") && ! b.getToggleState();
            g.setColour ((dark ? Palette::amber : Palette::ink).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.35f));
            g.setFont (font (juce::jmin (13.0f, (float) b.getHeight() * 0.48f)));
            g.drawText (b.getButtonText(), b.getLocalBounds().translated (0, down ? 1 : 0), juce::Justification::centred);
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override { return mono (14.0f); }
        juce::Font getPopupMenuFont() override { return mono (14.0f); }

        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
        {
            using namespace juce;
            auto r = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h);
            g.setColour (Palette::headerEdge);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (Palette::crtBack);
            g.fillRoundedRectangle (r.reduced (2.0f), 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRect (r.reduced (3.0f).removeFromTop ((float) h * 0.4f));

            Path arrow;
            const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
            arrow.addTriangle (ax - 5.0f, ay - 2.5f, ax + 5.0f, ay - 2.5f, ax, ay + 3.5f);
            g.setColour (Palette::amber);
            g.fillPath (arrow);
        }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
            label.setFont (getComboBoxFont (box));
        }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
            g.setColour (Palette::brassDark);
            g.drawRect (0, 0, w, h, 1);
        }
    };
}
