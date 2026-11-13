#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Theme.h"
#include "../PluginProcessor.h"

/** Output spectrum (FFT 4096, Hann, 1/12-oct smoothing for display). Updated from the editor timer. */
class Analyzer
{
public:
    explicit Analyzer (UBMasterProcessor& p);
    void update();                                   // call ~30-60 Hz
    float dbAt (float hz) const;                     // smoothed dB at frequency (≈ -100 .. +6)
    float levelAt (float hz) const;                  // 0..1 normalised for drawing
    bool hasSignal() const { return active; }
private:
    UBMasterProcessor& proc;
    static constexpr int order = 12, N = 1 << order;
    juce::dsp::FFT fft { order };
    std::vector<float> win, buf, db;
    bool active = false;
};

/** Rotary knob with label + value, attached to a parameter. */
class Knob : public juce::Component
{
public:
    Knob (UBMasterProcessor& p, const juce::String& paramId, juce::String label);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String lbl;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

/** Segmented control for a choice / bool parameter */
class Segmented : public juce::Component
{
public:
    Segmented (UBMasterProcessor& p, const juce::String& paramId, juce::StringArray labels);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent& e) override { hover = segAt (e.x); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void setParamId (const juce::String& id);
private:
    int segAt (int x) const { return juce::jlimit (0, labels.size() - 1, x * labels.size() / juce::jmax (1, getWidth())); }
    UBMasterProcessor& proc;
    juce::StringArray labels;
    std::unique_ptr<juce::ParameterAttachment> att;
    int idx = 0, hover = -1;
};

/** Small on/off pill (iOS-style switch) bound to a bool parameter */
class PowerSwitch : public juce::Component
{
public:
    PowerSwitch (UBMasterProcessor& p, const juce::String& paramId, juce::String label = {});
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool isOn() const { return on; }
    std::function<void()> onChange;
private:
    std::unique_ptr<juce::ParameterAttachment> att;
    bool on = false; float anim = 0;
    juce::String lbl;
};

/** Rounded action button */
class PillButton : public juce::Component
{
public:
    explicit PillButton (juce::String t, bool primary = false) : title (std::move (t)), prim (primary) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && isEnabled() && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void setSelected (bool s) { if (s != sel) { sel = s; repaint(); } }
    void setTitle (const juce::String& t) { if (t != title) { title = t; repaint(); } }
    std::function<void()> onClick;
    float fontSize = 12.5f;
private:
    juce::String title;
    bool prim = false, sel = false;
};

/** frequency <-> x on a log axis 20 Hz .. 20 kHz */
struct FreqAxis
{
    juce::Rectangle<float> r;
    float x (float hz) const { return r.getX() + r.getWidth() * std::log (hz / 20.0f) / std::log (1000.0f); }
    float hz (float px) const { return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (px - r.getX()) / r.getWidth())); }
    void drawGrid (juce::Graphics& g, bool labels = true) const;
};
