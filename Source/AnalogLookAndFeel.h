#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include "BinaryData.h"

namespace Analog
{
    namespace Palette
    {
        const juce::Colour silkscreen { 0xffeee3c6 };
        const juce::Colour engraved   { 0xff1f1c19 };
        const juce::Colour amber      { 0xffffb84a };
        const juce::Colour lampRed    { 0xffff3b22 };
        const juce::Colour phosphor   { 0xff74ffa6 };
        const juce::Colour crtBack    { 0xff050c08 };
        const juce::Colour vuFace     { 0xfff2dca6 };

        // Software toolbar (outside the "hardware")
        const juce::Colour barBack    { 0xff1c1c1e };
        const juce::Colour barControl { 0xff2c2c2f };
        const juce::Colour barOutline { 0xff3c3c40 };
        const juce::Colour barText    { 0xffd8d8dc };
        const juce::Colour barDim     { 0xff8a8a90 };
    }

    // Knob angles: 0 = 12 o'clock, clockwise positive (JUCE rotary convention).
    inline juce::Point<float> polar (juce::Point<float> c, float r, float a)
    {
        return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
    }

    //==========================================================================
    // Embedded art shared by every editor instance: fonts and sprite sheets.
    // Kept alive by the LookAndFeel (SharedResourcePointer), so lookups elsewhere are cheap.
    struct Art
    {
        Art()
        {
            using namespace BinaryData;
            // Michroma: Microgramma/Eurostile-style extended face, as printed on 1970s studio and hi-fi gear.
            silk     = juce::Typeface::createSystemTypefaceFor (MichromaRegular_ttf, MichromaRegular_ttfSize);
            silkBold = silk;
            ui       = juce::Typeface::createSystemTypefaceFor (BarlowMedium_ttf, BarlowMedium_ttfSize);
            vfd      = juce::Typeface::createSystemTypefaceFor (ShareTechMonoRegular_ttf, ShareTechMonoRegular_ttfSize);

            auto load = [] (const char* d, int n) { return juce::ImageCache::getFromMemory (d, n); };
            alu     = load (knob_alu_png, knob_alu_pngSize);
            black   = load (knob_black_png, knob_black_pngSize);
            chicken = load (knob_chicken_png, knob_chicken_pngSize);
            toggle  = load (toggle_png, toggle_pngSize);
            power   = load (power_png, power_pngSize);
        }

        juce::Typeface::Ptr silk, silkBold, ui, vfd;
        juce::Image alu, black, chicken, toggle, power;
    };

    inline Art& art() { return *juce::SharedResourcePointer<Art>(); }

    inline juce::Font font (float size, bool bold = false)
    {
        // Extended face: set a little smaller than a condensed one would be.
        return juce::Font (juce::FontOptions (bold ? art().silkBold : art().silk).withHeight (size * (bold ? 0.92f : 0.84f)))
                   .withExtraKerningFactor (0.04f);
    }
    inline juce::Font uiFont (float size) { return juce::Font (juce::FontOptions (art().ui).withHeight (size)); }
    inline juce::Font mono (float size)   { return juce::Font (juce::FontOptions (art().vfd).withHeight (size * 1.12f)); }

    // Silkscreen ink.
    inline void setInk (juce::Graphics& g, juce::Colour c, juce::Point<int> = {})
    {
        g.setColour (c); // clean, unworn print
    }

    inline juce::Point<int> panelOrigin (const juce::Component& c)
    {
        juce::Point<int> p;
        for (auto* comp = &c; comp != nullptr && comp->getProperties()["isFaceplate"].isVoid(); comp = comp->getParentComponent())
            p += comp->getPosition();
        return p;
    }

    // Faceted jewel indicator lamp in a chrome bezel (UA 175 / Neve style).
    inline void drawJewel (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, float on)
    {
        const float br = r * 1.35f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (c.x - br + 0.8f, c.y - br + 1.6f, br * 2.0f, br * 2.0f);
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
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.62f, r * 0.5f, r * 0.36f);
    }

    inline void drawSheetFrame (juce::Graphics& g, const juce::Image& sheet, int col, int row, int cols, int rows,
                                juce::Rectangle<float> dest)
    {
        const int fw = sheet.getWidth() / cols, fh = sheet.getHeight() / rows;
        g.drawImage (sheet, juce::roundToInt (dest.getX()), juce::roundToInt (dest.getY()),
                     juce::roundToInt (dest.getWidth()), juce::roundToInt (dest.getHeight()),
                     col * fw, row * fh, fw, fh);
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        static constexpr float imagePad = 1.45f;

        LookAndFeel()
        {
            using namespace Palette;
            setColour (juce::ComboBox::textColourId, barText);
            setColour (juce::ComboBox::arrowColourId, barDim);
            setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff232326));
            setColour (juce::PopupMenu::textColourId, barText);
            setColour (juce::PopupMenu::headerTextColourId, barDim);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a3a40));
            setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
            setColour (juce::TextButton::textColourOffId, barText);
            setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff232326));
            setColour (juce::AlertWindow::textColourId, barText);
            setColour (juce::AlertWindow::outlineColourId, barOutline);
            setColour (juce::TextEditor::backgroundColourId, barControl);
            setColour (juce::TextEditor::textColourId, juce::Colours::white);
            setColour (juce::TextEditor::outlineColourId, barOutline);
            setColour (juce::TextEditor::focusedOutlineColourId, amber.withAlpha (0.7f));
            setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0202022));
            setColour (juce::TooltipWindow::textColourId, barText);
            setColour (juce::TooltipWindow::outlineColourId, barOutline);
        }

        //======================================================================
        // Knobs. Slider properties: "knob" = alu | black | chicken; "labels" = "a|b|c".
        // Only the knob body is drawn here; the printed scale belongs to the panel (drawKnobScale),
        // so turning one knob can never paint over the labels of its neighbours.
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider& slider) override
        {
            using namespace juce;
            g.setImageResamplingQuality (Graphics::highResamplingQuality);

            const auto& props = slider.getProperties();
            const String style = props.getWithDefault ("knob", "black").toString();
            const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            const float R = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto c = bounds.getCentre();
            // The knob body is drawn at its animated position (see KnobControl::tick).
            pos = (float) props.getWithDefault ("displayPos", pos);
            const float angle = startAngle + pos * (endAngle - startAngle);
            const bool chicken = style == "chicken";
            const float knobR = R * (chicken ? 0.48f : 0.52f);

            // One of 120 Blender-rendered frames, 3 degrees apart, clockwise from 12 o'clock.
            const auto& sheet = style == "alu" ? art().alu : chicken ? art().chicken : art().black;
            constexpr int frames = 120, cols = 12, rows = 10;
            const float turn = MathConstants<float>::twoPi;
            float a = std::fmod (angle, turn);
            if (a < 0.0f) a += turn;
            const int frame = roundToInt (a / turn * (float) frames) % frames;
            drawSheetFrame (g, sheet, frame % cols, frame / cols, cols, rows,
                            Rectangle<float> (knobR * 2.0f * imagePad, knobR * 2.0f * imagePad).withCentre (c));
        }

        // Printed scale around a knob (dots + labels), drawn by the panel underneath the knobs.
        static void drawKnobScale (juce::Graphics& g, const juce::Slider& slider, juce::Rectangle<float> bounds, float alpha)
        {
            using namespace juce;
            const auto& props = slider.getProperties();
            const bool chicken = props.getWithDefault ("knob", "black").toString() == "chicken";
            StringArray labels = StringArray::fromTokens (props.getWithDefault ("labels", "0|1|2|3|4|5|6|7|8|9|10").toString(), "|", "");
            const auto rp = slider.getRotaryParameters();
            const float startAngle = rp.startAngleRadians, endAngle = rp.endAngleRadians;

            const float R = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto c = bounds.getCentre();
            const int n = labels.size();
            const float dotR = R * 0.68f, labelR = R * 0.84f;
            const float fontSize = jlimit (9.5f, 12.5f, R * 0.2f);

            g.setColour (Palette::silkscreen.withMultipliedAlpha (alpha));
            g.setFont (font (fontSize));
            for (int i = 0; i < n; ++i)
            {
                const float a = startAngle + (n > 1 ? (float) i / (float) (n - 1) : 0.5f) * (endAngle - startAngle);
                const auto p = polar (c, dotR, a);
                g.fillEllipse (p.x - 1.7f, p.y - 1.7f, 3.4f, 3.4f);

                if (! chicken && i < n - 1 && n <= 11)
                {
                    const auto pm = polar (c, dotR, a + 0.5f * (endAngle - startAngle) / (float) (n - 1));
                    g.fillEllipse (pm.x - 1.0f, pm.y - 1.0f, 2.0f, 2.0f);
                }

                if (labels[i].isNotEmpty())
                {
                    const auto lp = polar (c, labelR, a);
                    const float s = std::sin (a);
                    const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), labels[i]) + 2.0f;
                    Rectangle<float> box (tw, fontSize + 4.0f);
                    Justification just = Justification::centred;
                    if (chicken)         box.setCentre (polar (c, R * 0.92f, a));
                    else if (s < -0.35f) { box.setPosition (lp.x - tw + 4.0f, lp.y - box.getHeight() * 0.5f); just = Justification::centredRight; }
                    else if (s > 0.35f)  { box.setPosition (lp.x - 4.0f, lp.y - box.getHeight() * 0.5f); just = Justification::centredLeft; }
                    else                 box.setCentre (lp);
                    g.drawText (labels[i], box, just, false);
                }
            }
        }

        //======================================================================
        // Bat-handle toggles rendered in Blender. Property "anim" (0 = on .. 1 = off) picks one of
        // five frames so the lever visibly flips; "sheet" = "power" uses the horizontal switch.
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
        {
            using namespace juce;
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            const auto& props = b.getProperties();
            const float anim = (float) props.getWithDefault ("anim", b.getToggleState() ? 0.0 : 1.0);
            const bool horizontal = props.getWithDefault ("sheet", "toggle").toString() == "power";
            const int frame = jlimit (0, 4, roundToInt (anim * 4.0f));
            auto area = b.getLocalBounds().toFloat();

            if (horizontal)
            {
                const float size = 13.0f / 0.9f * 2.6f * 2.0f;
                drawSheetFrame (g, art().power, frame, 0, 5, 1, Rectangle<float> (size, size).withCentre (area.getCentre()));
                return;
            }

            setInk (g, Palette::silkscreen.withAlpha (highlighted ? 1.0f : 0.94f), panelOrigin (b));
            g.setFont (font (13.0f));
            g.drawText (b.getButtonText(), area.removeFromTop (20.0f), Justification::centred);

            const auto c = area.getCentre();
            const float lever = jmin (30.0f, area.getHeight() * 0.36f);
            g.setFont (font (9.0f));
            g.drawText ("ON", Rectangle<float> (c.x + 12.0f, c.y - lever - 2.0f, 30.0f, 12.0f), Justification::centredLeft);
            g.drawText ("OFF", Rectangle<float> (c.x + 12.0f, c.y + lever - 10.0f, 30.0f, 12.0f), Justification::centredLeft);

            // The ring is 0.9 of 2.6 half-width units in the render; show it at 15 px radius.
            const float size = 15.0f / 0.9f * 2.6f * 2.0f;
            drawSheetFrame (g, art().toggle, frame, 0, 5, 1, Rectangle<float> (size, size).withCentre (c));
        }

        //======================================================================
        // Toolbar: flat, quiet controls like a UAD plug-in's software bar.
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                   bool highlighted, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            const bool on = b.getToggleState();
            auto fill = on ? juce::Colour (0xff46464c) : Palette::barControl;
            if (highlighted) fill = fill.brighter (0.12f);
            if (down) fill = fill.darker (0.2f);
            g.setColour (fill);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (on ? Palette::amber.withAlpha (0.75f) : Palette::barOutline);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
        {
            g.setColour ((b.getToggleState() ? juce::Colours::white : Palette::barText).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.35f));
            g.setFont (uiFont (13.5f));
            g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (14.0f); }
        juce::Font getPopupMenuFont() override { return uiFont (14.5f); }
        juce::Font getAlertWindowMessageFont() override { return uiFont (14.0f); }
        juce::Font getAlertWindowTitleFont() override { return uiFont (16.0f); }
        juce::Font getTextButtonFont (juce::TextButton&, int) override { return uiFont (13.5f); }

        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
        {
            auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
            g.setColour (box.isMouseOver (true) ? Palette::barControl.brighter (0.08f) : Palette::barControl);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (Palette::barOutline);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);

            juce::Path arrow;
            const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
            arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
            arrow.lineTo (ax, ay + 2.5f);
            arrow.lineTo (ax + 4.0f, ay - 2.0f);
            g.setColour (Palette::barDim);
            g.strokePath (arrow, juce::PathStrokeType (1.5f));
        }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
            label.setFont (getComboBoxFont (box));
        }

        void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
        {
            g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
            g.setColour (Palette::barOutline);
            g.drawRect (0, 0, w, h, 1);
        }

    private:
        juce::SharedResourcePointer<Art> artHolder;
    };
}
