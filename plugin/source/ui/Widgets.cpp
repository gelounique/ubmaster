#include "Widgets.h"

using namespace theme;

// ------------------------------------------------------------------------------------------------ Analyzer
Analyzer::Analyzer (UBMasterProcessor& p) : proc (p)
{
    win.resize (N); buf.resize (2 * N);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N);
    db.assign (N / 2 + 1, -120.0f);
}

void Analyzer::update()
{
    const int w = proc.scopeWrite.load (std::memory_order_acquire);
    float peak = 0;
    for (int i = 0; i < N; ++i)
    {
        const int k = (w - N + i) & (UBMasterProcessor::kScope - 1);
        const float m = 0.5f * (proc.scopeL[(size_t) k].load (std::memory_order_relaxed) + proc.scopeR[(size_t) k].load (std::memory_order_relaxed));
        peak = std::max (peak, std::abs (m));
        buf[(size_t) i] = m * win[(size_t) i];
    }
    std::fill (buf.begin() + N, buf.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (buf.data(), true);
    active = peak > 1e-5f;
    const float norm = 2.0f / (N * 0.5f);
    for (int k = 0; k <= N / 2; ++k)
    {
        const float d = juce::Decibels::gainToDecibels (buf[(size_t) k] * norm, -120.0f);
        float& s = db[(size_t) k];
        s = d > s ? s + (d - s) * 0.6f : s + (d - s) * 0.18f;   // fast up, slow down
    }
}

float Analyzer::dbAt (float hz) const
{
    const double sr = proc.getSampleRateSafe();
    const float binHz = (float) (sr / N);
    // average over ±1/24 octave (more bins at the top -> smooth)
    const float fb = hz / binHz;
    if (fb < 24.0f)   // low end: fewer bins than the smoothing width -> interpolate in dB between bins
    {
        const int k = juce::jlimit (1, N / 2 - 1, (int) fb); const float fr = juce::jlimit (0.0f, 1.0f, fb - (float) k);
        return db[(size_t) k] * (1 - fr) + db[(size_t) k + 1] * fr + 3.0f * std::log2 (hz / 1000.0f);
    }
    const float lo = hz * 0.971f, hi = hz * 1.029f;
    int k0 = (int) std::floor (lo / binHz), k1 = (int) std::ceil (hi / binHz);
    k0 = juce::jlimit (1, N / 2, k0); k1 = juce::jlimit (k0, N / 2, k1);
    float e = 0; int n = 0;
    for (int k = k0; k <= k1; ++k) { e += std::pow (10.0f, db[(size_t) k] * 0.1f); ++n; }
    // tilt +3 dB/oct around 1 kHz so music reads "flat" (pink-noise compensation)
    return 10.0f * std::log10 (e / (float) n + 1e-14f) + 3.0f * std::log2 (hz / 1000.0f);
}

float Analyzer::levelAt (float hz) const { return juce::jlimit (0.0f, 1.0f, (dbAt (hz) + 78.0f) / 66.0f); }

// ------------------------------------------------------------------------------------------------ FreqAxis
void FreqAxis::drawGrid (juce::Graphics& g, bool labels) const
{
    const int fs[] { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    for (int fi : fs)
    {
        const float f = (float) fi;
        g.setColour (juce::Colours::white.withAlpha (fi == 100 || fi == 1000 || fi == 10000 ? 0.07f : 0.035f));
        g.drawVerticalLine ((int) x (f), r.getY(), r.getBottom());
        if (labels)
        {
            g.setColour (faint); g.setFont (mono (10.0f));
            g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), juce::Rectangle<float> (x (f) + 3, r.getBottom() - 14, 40, 12), juce::Justification::left);
        }
    }
}

// ------------------------------------------------------------------------------------------------ Knob
Knob::Knob (UBMasterProcessor& p, const juce::String& id, juce::String label) : lbl (std::move (label))
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);
    slider.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
    if (auto* prm = p.apvts.getParameter (id)) slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

void Knob::resized()
{
    auto b = getLocalBounds();
    b.removeFromBottom (34);
    const int s = std::min (b.getWidth(), b.getHeight());
    slider.setBounds (b.withSizeKeepingCentre (s, s));
}

void Knob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto t = b.removeFromBottom (34);
    const bool dimmed = (bool) slider.getProperties().getWithDefault ("ubDim", false);
    g.setColour (dimmed ? dim : text);
    g.setFont (mono (12.5f, true));
    g.drawText (slider.getTextFromValue (slider.getValue()), t.removeFromTop (17), juce::Justification::centred);
    g.setColour (dim);
    g.setFont (sans (11.0f, 600));
    g.drawText (lbl.toUpperCase(), t, juce::Justification::centredTop);
}

// ------------------------------------------------------------------------------------------------ Segmented
Segmented::Segmented (UBMasterProcessor& p, const juce::String& id, juce::StringArray l) : proc (p), labels (std::move (l))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setParamId (id);
}

void Segmented::setParamId (const juce::String& id)
{
    att.reset();
    if (auto* prm = proc.apvts.getParameter (id))
    {
        att = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { idx = juce::roundToInt (v); repaint(); });
        att->sendInitialUpdate();
    }
}

void Segmented::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked() || att == nullptr) return;
    att->setValueAsCompleteGesture ((float) segAt (e.x));
}

void Segmented::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float rad = b.getHeight() * 0.5f;
    g.setColour (panel3.withAlpha (0.7f)); g.fillRoundedRectangle (b, rad);
    g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawRoundedRectangle (b.reduced (0.5f), rad, 1.0f);
    const float w = b.getWidth() / labels.size();
    for (int i = 0; i < labels.size(); ++i)
    {
        auto s = juce::Rectangle<float> (b.getX() + w * i, b.getY(), w, b.getHeight()).reduced (3.0f);
        if (i == idx)
        {
            g.setColour (tiffany.withAlpha (0.16f)); g.fillRoundedRectangle (s.expanded (1.5f), s.getHeight() * 0.5f + 1.5f);
            g.setColour (tiffany); g.fillRoundedRectangle (s, s.getHeight() * 0.5f);
        }
        else if (i == hover) { g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (s, s.getHeight() * 0.5f); }
        g.setColour (i == idx ? bg : (i == hover ? text : dim));
        g.setFont (mono (11.5f, true));
        g.drawText (labels[i], s, juce::Justification::centred);
    }
}

// ------------------------------------------------------------------------------------------------ PowerSwitch
PowerSwitch::PowerSwitch (UBMasterProcessor& p, const juce::String& id, juce::String label) : lbl (std::move (label))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    if (auto* prm = p.apvts.getParameter (id))
    {
        att = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { on = v > 0.5f; repaint(); if (onChange) onChange(); });
        att->sendInitialUpdate();
    }
}

void PowerSwitch::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && att) att->setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

void PowerSwitch::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    if (lbl.isNotEmpty())
    {
        g.setColour (on ? text : dim); g.setFont (mono (11.0f, true));
        g.drawText (lbl, b.removeFromLeft (b.getWidth() - 40), juce::Justification::centredRight);
        b.removeFromLeft (6);
    }
    auto sw = b.withSizeKeepingCentre (34, 20);
    g.setColour (on ? tiffany : panel3); g.fillRoundedRectangle (sw, 10.0f);
    if (on) { g.setColour (tiffany.withAlpha (0.25f)); g.drawRoundedRectangle (sw.expanded (2.0f), 12.0f, 2.0f); }
    const float kx = on ? sw.getRight() - 18 : sw.getX() + 2;
    g.setColour (on ? bg : dim); g.fillEllipse (kx, sw.getY() + 2, 16, 16);
}

// ------------------------------------------------------------------------------------------------ PillButton
void PillButton::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float r = b.getHeight() * 0.5f;
    const bool hover = isMouseOver();
    if (prim || sel)
    {
        g.setColour (tiffany.withAlpha (hover ? 0.32f : 0.18f)); g.fillRoundedRectangle (b.expanded (1.0f), r + 1);
        g.setColour (tiffany.withAlpha (isEnabled() ? 1.0f : 0.4f)); g.fillRoundedRectangle (b.reduced (1.5f), r - 1.5f);
        g.setColour (bg);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (hover ? 0.10f : 0.05f)); g.fillRoundedRectangle (b, r);
        g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawRoundedRectangle (b, r, 1.0f);
        g.setColour (hover ? text : text.withAlpha (0.82f));
    }
    g.setFont (sans (fontSize, 600));
    g.drawText (title, b, juce::Justification::centred);
}
