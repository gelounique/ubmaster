#include "Theme.h"
#include "BinaryData.h"

namespace theme
{
static juce::Typeface::Ptr tf (const void* d, size_t n) { return juce::Typeface::createSystemTypefaceFor (d, n); }
static juce::Typeface::Ptr sansTf (int w)
{
    static auto r = tf (BinaryData::InstrumentSansRegular_ttf, BinaryData::InstrumentSansRegular_ttfSize);
    static auto s = tf (BinaryData::InstrumentSansSemiBold_ttf, BinaryData::InstrumentSansSemiBold_ttfSize);
    static auto b = tf (BinaryData::InstrumentSansBold_ttf, BinaryData::InstrumentSansBold_ttfSize);
    return w >= 700 ? b : (w >= 600 ? s : r);
}
static juce::Typeface::Ptr monoTf (bool m)
{
    static auto r = tf (BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
    static auto md = tf (BinaryData::JetBrainsMonoMedium_ttf, BinaryData::JetBrainsMonoMedium_ttfSize);
    return m ? md : r;
}
juce::Font sans (float size, int weight) { return juce::Font (juce::FontOptions (sansTf (weight)).withHeight (size)); }
juce::Font mono (float size, bool medium) { return juce::Font (juce::FontOptions (monoTf (medium)).withHeight (size)); }

juce::Image logo()
{
    static juce::Image img = juce::ImageCache::getFromMemory (BinaryData::ublogo_png, BinaryData::ublogo_pngSize);
    return img;
}

void glowPath (juce::Graphics& g, const juce::Path& p, float w, juce::Colour c, float spread, float alpha)
{
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (c.withAlpha (alpha * 0.18f * (4 - i)));
        g.strokePath (p, juce::PathStrokeType (w + spread * (float) i, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void card (juce::Graphics& g, juce::Rectangle<float> r, float rad, juce::Colour fill)
{
    juce::ColourGradient gr (fill.brighter (0.06f), r.getX(), r.getY(), fill, r.getX(), r.getBottom(), false);
    g.setGradientFill (gr);
    g.fillRoundedRectangle (r, rad);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
}

juce::Path icon (Icon i, juce::Rectangle<float> b)
{
    juce::Path p;
    const float x = b.getX(), y = b.getY(), w = b.getWidth(), h = b.getHeight();
    auto P = [&] (float fx, float fy) { return juce::Point<float> (x + fx * w, y + fy * h); };
    auto dot = [&] (float fx, float fy, float r) { p.addEllipse (P (fx, fy).x - r * w, P (fx, fy).y - r * w, 2 * r * w, 2 * r * w); };
    switch (i)
    {
        case Icon::EQ:
            p.startNewSubPath (P (0.02f, 0.62f));
            p.cubicTo (P (0.18f, 0.62f), P (0.20f, 0.30f), P (0.33f, 0.30f));
            p.cubicTo (P (0.46f, 0.30f), P (0.50f, 0.74f), P (0.66f, 0.74f));
            p.cubicTo (P (0.80f, 0.74f), P (0.84f, 0.44f), P (0.98f, 0.44f));
            dot (0.33f, 0.30f, 0.07f); dot (0.66f, 0.74f, 0.07f); dot (0.90f, 0.46f, 0.05f);
            break;
        case Icon::Tape:
            dot (0.27f, 0.42f, 0.21f); dot (0.73f, 0.42f, 0.21f);
            dot (0.27f, 0.42f, 0.05f); dot (0.73f, 0.42f, 0.05f);
            p.startNewSubPath (P (0.27f, 0.84f)); p.lineTo (P (0.73f, 0.84f));
            p.startNewSubPath (P (0.08f, 0.50f)); p.quadraticTo (P (0.10f, 0.84f), P (0.27f, 0.84f));
            p.startNewSubPath (P (0.92f, 0.50f)); p.quadraticTo (P (0.90f, 0.84f), P (0.73f, 0.84f));
            break;
        case Icon::Transient:
            p.startNewSubPath (P (0.0f, 0.62f)); p.lineTo (P (0.26f, 0.62f)); p.lineTo (P (0.32f, 0.08f)); p.lineTo (P (0.40f, 0.86f));
            p.lineTo (P (0.48f, 0.42f)); p.lineTo (P (0.56f, 0.72f)); p.lineTo (P (0.64f, 0.55f)); p.lineTo (P (0.72f, 0.66f)); p.lineTo (P (1.0f, 0.62f));
            break;
        case Icon::Stereo:
            dot (0.36f, 0.5f, 0.27f); dot (0.64f, 0.5f, 0.27f);
            break;
        case Icon::Exciter:
        {
            auto star = [&] (float cx, float cy, float r) {
                const auto c = P (cx, cy); const float R = r * w, k = R * 0.22f;
                p.startNewSubPath (c.x, c.y - R);
                p.quadraticTo (c.x + k, c.y - k, c.x + R, c.y); p.quadraticTo (c.x + k, c.y + k, c.x, c.y + R);
                p.quadraticTo (c.x - k, c.y + k, c.x - R, c.y); p.quadraticTo (c.x - k, c.y - k, c.x, c.y - R); p.closeSubPath(); };
            star (0.42f, 0.56f, 0.38f); star (0.82f, 0.18f, 0.15f);
            break;
        }
        case Icon::Limiter:
            p.startNewSubPath (P (0.0f, 0.22f)); p.lineTo (P (1.0f, 0.22f));
            p.startNewSubPath (P (0.0f, 0.70f)); p.lineTo (P (0.12f, 0.36f)); p.lineTo (P (0.20f, 0.30f)); p.lineTo (P (0.34f, 0.30f));
            p.lineTo (P (0.44f, 0.84f)); p.lineTo (P (0.56f, 0.30f)); p.lineTo (P (0.70f, 0.30f)); p.lineTo (P (0.80f, 0.62f)); p.lineTo (P (0.90f, 0.40f)); p.lineTo (P (1.0f, 0.48f));
            break;
    }
    return p;
}

LookAndFeel::LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, panel2);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, tiffany);
    setColour (juce::PopupMenu::highlightedTextColourId, bg);
    setColour (juce::TooltipWindow::backgroundColourId, panel2);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, tiffany.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, tiffany);
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName()) return sansTf (f.isBold() ? 700 : 400);
    return juce::LookAndFeel_V4::getTypefaceForFont (f);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (panel2);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRect (0, 0, w, h, 1);
}

void LookAndFeel::getIdealPopupMenuItemSize (const juce::String& t, bool sep, int, int& w, int& h)
{
    if (sep) { w = 50; h = 9; return; }
    h = 30;
    w = (int) juce::GlyphArrangement::getStringWidth (getPopupMenuFont(), t) + 48;
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                     bool, const juce::String& t, const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator) { g.setColour (line); g.fillRect (area.reduced (8, 4).withHeight (1)); return; }
    auto r = area.reduced (4, 2);
    if (isHighlighted && isActive) { g.setColour (tiffany); g.fillRoundedRectangle (r.toFloat(), 8.0f); }
    g.setColour (isHighlighted && isActive ? bg : text.withAlpha (isActive ? 0.9f : 0.3f));
    g.setFont (getPopupMenuFont());
    g.drawText (t, r.withTrimmedLeft (12), juce::Justification::centredLeft);
    if (isTicked) { g.setColour (isHighlighted ? bg : tiffany); g.fillEllipse ((float) r.getRight() - 16.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f); }
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const bool enabled = s.isEnabled() && ! (bool) s.getProperties().getWithDefault ("ubDim", false);
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const bool bipolar = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float zero = bipolar ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
    const float ar = r - 3.0f;

    juce::Path track; track.addCentredArc (c.x, c.y, ar, ar, 0, a0, a1, true);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float va = a0 + pos * (a1 - a0), za = a0 + zero * (a1 - a0);
    if (std::abs (va - za) > 0.01f)
    {
        juce::Path val; val.addCentredArc (c.x, c.y, ar, ar, 0, std::min (va, za), std::max (va, za), true);
        if (enabled) glowPath (g, val, 3.0f, tiffany, 4.0f, 0.5f);
        else { g.setColour (faint); g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
    }
    // knob face
    const float kr = ar - 9.0f;
    juce::ColourGradient body (juce::Colour (0xff222226), c.x, c.y - kr, juce::Colour (0xff0b0b0d), c.x, c.y + kr, false);
    g.setGradientFill (body); g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
    g.setColour (juce::Colours::white.withAlpha (s.isMouseOverOrDragging() ? 0.16f : 0.08f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);
    // pointer
    const auto p0 = c.getPointOnCircumference (kr * 0.45f, va), p1 = c.getPointOnCircumference (kr * 0.86f, va);
    g.setColour (enabled ? text : dim);
    g.drawLine ({ p0, p1 }, 2.2f);
}
}
