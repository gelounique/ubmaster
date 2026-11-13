#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

#ifndef U8
 #define U8(s) juce::String (juce::CharPointer_UTF8 (s))
#endif

namespace theme
{
    // UBMaster — UNIQUEBABY dark, Apple-style. One accent only: Tiffany blue.
    const juce::Colour bg      { 0xff000000 };
    const juce::Colour panel   { 0xff111113 };   // graphite
    const juce::Colour panel2  { 0xff18181b };
    const juce::Colour panel3  { 0xff232327 };
    const juce::Colour line    { 0x14ffffff };
    const juce::Colour text    { 0xfff5f5f7 };
    const juce::Colour dim     { 0xff8e8e95 };
    const juce::Colour faint   { 0xff4a4a50 };
    const juce::Colour tiffany { 0xff81d8d0 };
    const juce::Colour warn    { 0xfff5f5f7 };   // warnings stay white (no 2nd accent)

    constexpr float radius = 22.0f;

    juce::Font sans (float size, int weight = 400);   // Instrument Sans 400/600/700
    juce::Font mono (float size, bool medium = false); // JetBrains Mono
    juce::Image logo();                               // white UB

    /** soft Tiffany glow behind a path */
    void glowPath (juce::Graphics&, const juce::Path&, float strokeW, juce::Colour c, float spread = 6.0f, float alpha = 0.35f);
    void card (juce::Graphics&, juce::Rectangle<float>, float r = radius, juce::Colour fill = panel);

    enum class Icon { EQ, Tape, Transient, Stereo, Exciter, Limiter };
    juce::Path icon (Icon, juce::Rectangle<float> box);   // stroked line icons

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
        void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
        void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                bool hasSubMenu, const juce::String& text, const juce::String& shortcut, const juce::Drawable* icon, const juce::Colour* textColour) override;
        juce::Font getPopupMenuFont() override { return mono (13.0f, true); }
        void getIdealPopupMenuItemSize (const juce::String&, bool isSeparator, int standardHeight, int& w, int& h) override;
        juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
        void fillTextEditorBackground (juce::Graphics&, int, int, juce::TextEditor&) override {}
        void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override {}
    };
}
