#pragma once
#include "Widgets.h"

/** Base: rounded graphite panel with icon, title, one-line description and power switch */
class ModulePanel : public juce::Component
{
public:
    ModulePanel (UBMasterProcessor& p, theme::Icon ic, juce::String title, juce::String desc, const juce::String& powerId);
    void paint (juce::Graphics&) override;
    void resized() override;
    virtual void layoutBody (juce::Rectangle<int>) = 0;
    virtual void tick() {}
protected:
    UBMasterProcessor& proc;
    PowerSwitch power;
    juce::OwnedArray<Knob> knobs;
    Knob& addKnob (const juce::String& id, const juce::String& label);
    void layoutKnobs (juce::Rectangle<int> row, int maxW = 104);
private:
    theme::Icon icon; juce::String title, desc;
};

/** spectrum + highlighted range with draggable edges (lo/hi parameters) */
class RangeView : public juce::Component
{
public:
    RangeView (UBMasterProcessor& p, Analyzer& a, const juce::String& loId, const juce::String& hiId, juce::String caption);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    std::function<void (juce::Graphics&, const FreqAxis&)> overlay;
    float lo = 20, hi = 20000;
    bool showSpectrum = true;
private:
    FreqAxis axis() const { return { getLocalBounds().toFloat().reduced (2.0f, 6.0f) }; }
    UBMasterProcessor& proc; Analyzer& an;
    std::unique_ptr<juce::ParameterAttachment> aLo, aHi;
    juce::String caption;
    int drag = 0;   // 1 lo, 2 hi, 3 both
    float dragHz0 = 0, lo0 = 0, hi0 = 0;
};

/** ---------------------------------------------------------------------------------------- EQ */
class EqGraph : public juce::Component, private juce::AudioProcessorValueTreeState::Listener
{
public:
    EqGraph (UBMasterProcessor& p, Analyzer& a);
    ~EqGraph() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    std::function<void (int)> onSelect;
    int selected = 2;
private:
    void parameterChanged (const juce::String&, float) override { dirty = true; }
    FreqAxis axis() const { return { getLocalBounds().toFloat().reduced (4.0f, 8.0f) }; }
    float yFor (float db) const;
    float dbFor (float y) const;
    juce::Point<float> nodePos (int b) const;
    int nodeAt (juce::Point<float>) const;
    UBMasterProcessor& proc; Analyzer& an;
    int drag = -1;
    bool dirty = true;
    juce::Path curveA, curveB;
    void rebuild();
};

class EqPanel : public ModulePanel
{
public:
    EqPanel (UBMasterProcessor& p, Analyzer& a);
    void layoutBody (juce::Rectangle<int>) override;
    void tick() override { graph.repaint(); }
    void select (int band);
    void paintOverChildren (juce::Graphics&) override;
private:
    Segmented mode;
    EqGraph graph;
    juce::OwnedArray<PillButton> bandBtns;
    juce::OwnedArray<Knob> bandKnobs;          // 3 per band
    juce::OwnedArray<Segmented> targets;       // 1 per band
    juce::OwnedArray<PowerSwitch> bandOn;      // 1 per band
    int sel = 2;
    juce::Rectangle<int> rowArea;
};

/** ---------------------------------------------------------------------------------------- TAPE */
class TapePanel : public ModulePanel
{
public:
    TapePanel (UBMasterProcessor& p, Analyzer& a);
    void layoutBody (juce::Rectangle<int>) override;
    void tick() override { range.repaint(); }
private:
    RangeView range;
};

/** ---------------------------------------------------------------------------------------- TRANSIENT */
class TransientPanel : public ModulePanel
{
public:
    TransientPanel (UBMasterProcessor& p, Analyzer& a);
    void layoutBody (juce::Rectangle<int>) override;
    void tick() override { range.repaint(); }
private:
    Segmented mode;
    PowerSwitch delta;
    RangeView range;
};

/** ---------------------------------------------------------------------------------------- STEREO */
class Vectorscope : public juce::Component
{
public:
    explicit Vectorscope (UBMasterProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    UBMasterProcessor& proc;
};

class WidthView : public juce::Component
{
public:
    explicit WidthView (UBMasterProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    UBMasterProcessor& proc;
};

class StereoPanel : public ModulePanel
{
public:
    StereoPanel (UBMasterProcessor& p);
    void layoutBody (juce::Rectangle<int>) override;
    void tick() override { scope.repaint(); widths.repaint(); }
private:
    Vectorscope scope;
    WidthView widths;
};

/** ---------------------------------------------------------------------------------------- EXCITER */
class ExciterPanel : public ModulePanel
{
public:
    ExciterPanel (UBMasterProcessor& p, Analyzer& a);
    void layoutBody (juce::Rectangle<int>) override;
    void tick() override { view.repaint(); }
private:
    struct View : juce::Component
    {
        View (UBMasterProcessor& p, Analyzer& a) : proc (p), an (a) {}
        void paint (juce::Graphics&) override;
        UBMasterProcessor& proc; Analyzer& an;
    } view;
};

/** ---------------------------------------------------------------------------------------- LIMITER */
class LimiterPanel : public ModulePanel
{
public:
    explicit LimiterPanel (UBMasterProcessor& p);
    void layoutBody (juce::Rectangle<int>) override;
    void tick() override;
private:
    struct Graph : juce::Component
    {
        explicit Graph (UBMasterProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
        UBMasterProcessor& proc;
        std::array<float, 240> gr {}, lvl {};
        int w = 0;
    } graph;
};
