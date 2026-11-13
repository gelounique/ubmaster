#include "Halo.h"

using namespace theme;

float LoudnessHalo::angleFor (float lufs)
{
    const float t = juce::jlimit (0.0f, 1.0f, (lufs + 36.0f) / 32.0f);
    return juce::degreesToRadians (-135.0f + 270.0f * t);
}

void LoudnessHalo::tick (float dt)
{
    phase += dt * 0.08f;
    for (int i = 0; i < kBars; ++i)
    {
        // bars run around the ring: low freqs at the top going clockwise, mirrored so the ring is symmetric
        const float t = (float) i / kBars;
        const float m = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
        const float hz = 30.0f * std::pow (16000.0f / 30.0f, m);
        float v = an.hasSignal() ? an.levelAt (hz) : 0.0f;
        const float idle = 0.05f + 0.035f * std::sin (phase * 6.0f + t * 31.0f) + 0.02f * std::sin (phase * 11.0f - t * 57.0f);
        v = std::max (v, idle);
        bars[(size_t) i] += (v - bars[(size_t) i]) * (v > bars[(size_t) i] ? 0.55f : 0.14f);
    }
    const float st = proc.meter.shortTerm.load();
    shown = shown < -90 ? st : shown + (st - shown) * 0.25f;
    const float target = proc.get (ids::target);
    pulse = std::abs (shown - target) <= 0.5f ? std::min (1.0f, pulse + dt * 3) : std::max (0.0f, pulse - dt * 2);
    repaint();
}

void LoudnessHalo::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre().translated (0, -4);
    const float R = std::min (b.getWidth(), b.getHeight()) * 0.5f - 36.0f;   // gauge radius
    const float base = R * 0.66f, len = R * 0.24f;

    // ambient glow
    {
        juce::ColourGradient amb (tiffany.withAlpha (0.10f + 0.06f * pulse), c.x, c.y, tiffany.withAlpha (0.0f), c.x + R * 1.1f, c.y, true);
        g.setGradientFill (amb); g.fillEllipse (c.x - R * 1.1f, c.y - R * 1.1f, R * 2.2f, R * 2.2f);
    }
    // vinyl grooves
    for (int k = 0; k < 7; ++k)
    {
        const float rr = base * (0.62f + 0.055f * k);
        g.setColour (juce::Colours::white.withAlpha (0.025f + 0.008f * (k % 2)));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 1.0f);
    }
    // spectrum bars (glow pass + crisp pass)
    juce::Path bp;
    for (int i = 0; i < kBars; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / kBars;
        const float v = bars[(size_t) i];
        const auto p0 = c.getPointOnCircumference (base, a), p1 = c.getPointOnCircumference (base + 3.0f + len * v, a);
        bp.startNewSubPath (p0); bp.lineTo (p1);
    }
    g.setColour (tiffany.withAlpha (0.10f)); g.strokePath (bp, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (tiffany.withAlpha (0.20f)); g.strokePath (bp, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    for (int i = 0; i < kBars; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / kBars;
        const float v = bars[(size_t) i];
        const auto p0 = c.getPointOnCircumference (base, a), p1 = c.getPointOnCircumference (base + 3.0f + len * v, a);
        g.setColour (tiffany.interpolatedWith (juce::Colours::white, juce::jlimit (0.0f, 1.0f, (v - 0.55f) * 1.6f)).withAlpha (0.45f + 0.55f * v));
        g.drawLine ({ p0, p1 }, 1.5f);
    }

    // loudness gauge
    const float a0 = juce::degreesToRadians (-135.0f), a1 = juce::degreesToRadians (135.0f);
    {
        juce::Path tr; tr.addCentredArc (c.x, c.y, R, R, 0, a0, a1, true);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.strokePath (tr, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (int l = -36; l <= -4; l += 2)   // ticks
        {
            const float a = angleFor ((float) l);
            const bool major = l % 6 == 0;
            g.setColour (juce::Colours::white.withAlpha (major ? 0.22f : 0.09f));
            g.drawLine ({ c.getPointOnCircumference (R + 6, a), c.getPointOnCircumference (R + (major ? 13.0f : 9.0f), a) }, 1.0f);
            if (major && l > -36 && l < -4)
            {
                g.setFont (mono (9.5f)); g.setColour (faint);
                const auto tp = c.getPointOnCircumference (R + 25, a);
                g.drawText (juce::String (l), juce::Rectangle<float> (30, 12).withCentre (tp), juce::Justification::centred);
            }
        }
        if (shown > -36.0f)
        {
            juce::Path v; v.addCentredArc (c.x, c.y, R, R, 0, a0, angleFor (shown), true);
            glowPath (g, v, 2.5f, tiffany, 5.0f, 0.55f);
        }
    }
    // target marker
    {
        const float tgt = proc.get (ids::target);
        const auto mp = c.getPointOnCircumference (R, angleFor (tgt));
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (tiffany.withAlpha ((0.10f + 0.06f * pulse) * (5 - i)));
            const float rr = 4.0f + i * 3.5f;
            g.fillEllipse (mp.x - rr, mp.y - rr, rr * 2, rr * 2);
        }
        g.setColour (juce::Colours::white); g.fillEllipse (mp.x - 4.5f, mp.y - 4.5f, 9, 9);
    }

    // logo
    auto lg = logo();
    const float lw = base * 0.78f, lh = lw * (float) lg.getHeight() / (float) lg.getWidth();
    auto lr = juce::Rectangle<float> (lw, lh).withCentre (c.translated (0, -base * 0.30f));
    g.setOpacity (1.0f);
    g.drawImage (lg, lr, juce::RectanglePlacement::centred);

    // big LUFS
    const bool sig = shown > -70.0f;
    g.setColour (text);
    g.setFont (mono (base * 0.30f, true));
    auto nr = juce::Rectangle<float> (base * 1.6f, base * 0.34f).withCentre (c.translated (0, base * 0.20f));
    g.drawText (sig ? juce::String (shown, 1) : juce::String::fromUTF8 ("–"), nr, juce::Justification::centred);
    g.setColour (dim); g.setFont (mono (10.5f, true));
    const float tgt = proc.get (ids::target);
    juce::String sub = "LUFS  SHORT-TERM";
    g.drawText (sub, nr.translated (0, nr.getHeight() * 0.72f).withHeight (14), juce::Justification::centred);
    if (sig)
    {
        const float d = shown - tgt;
        g.setColour (std::abs (d) <= 0.5f ? tiffany : text.withAlpha (0.7f));
        g.setFont (mono (11.0f, true));
        const juce::String s = std::abs (d) <= 0.5f ? juce::String::fromUTF8 ("● AUF ZIEL") : (d < 0 ? juce::String::fromUTF8 ("▲ ") + juce::String (-d, 1) + " LU" : juce::String::fromUTF8 ("▼ ") + juce::String (d, 1) + " LU");
        g.drawText (s, nr.translated (0, nr.getHeight() * 0.72f + 16).withHeight (14), juce::Justification::centred);
    }
}
