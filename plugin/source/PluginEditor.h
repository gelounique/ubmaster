#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Advice.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include "ui/Halo.h"
#include "ui/Panels.h"

/** Glass navbar: brand, platform pills, target LUFS field */
class NavBar : public juce::Component, private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit NavBar (UBMasterProcessor& p);
    ~NavBar() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
private:
    void parameterChanged (const juce::String&, float) override { juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<NavBar> (this)] { if (sp) sp->refresh(); }); }
    void commitTarget();
    UBMasterProcessor& proc;
    juce::OwnedArray<PillButton> pills;
    juce::TextEditor target;
};

/** One card of the signal chain */
class ModuleCard : public juce::Component
{
public:
    ModuleCard (UBMasterProcessor& p, int index, theme::Icon ic, juce::String name, const juce::String& powerId, std::function<juce::String()> status);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void setSelected (bool s) { if (s != sel) { sel = s; repaint(); } }
    std::function<void (int)> onSelect;
    float glow = 0;
private:
    juce::Rectangle<float> dotArea() const { return { (float) getWidth() - 34.0f, 12.0f, 22.0f, 22.0f }; }
    UBMasterProcessor& proc;
    int idx; theme::Icon icon; juce::String name;
    std::unique_ptr<juce::ParameterAttachment> att;
    bool on = true, sel = false;
    std::function<juce::String()> status;
};

/** Integrated / Short-term / Momentary / True Peak / LRA */
class MeterBar : public juce::Component
{
public:
    explicit MeterBar (UBMasterProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override { reset.setBounds (getWidth() - 96, 16, 78, 26); }
    void tick();
private:
    UBMasterProcessor& proc;
    PillButton reset { juce::String::fromUTF8 ("↺ Reset") };
    float m = -100, s = -100, in = -100, tp = -100, lra = 0, tpHold = -100;
};

class AdviceBox : public juce::Component
{
public:
    explicit AdviceBox (UBMasterProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    UBMasterProcessor& proc;
    PillButton match { juce::String::fromUTF8 ("Auf Ziel bringen"), true };
    ubm::Advice adv;
    juce::String flash; int flashTicks = 0;
};

class UBMasterMain : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1240, H = 860;
    explicit UBMasterMain (UBMasterProcessor& p);
    ~UBMasterMain() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override { repaint (linkArea); setMouseCursor (linkArea.contains (getMouseXYRelative()) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); }
    void select (int module);
    void tick (float dt);                 // public for the snapshot tool
private:
    void timerCallback() override { tick (1.0f / 60.0f); }
    UBMasterProcessor& proc;
    theme::LookAndFeel lnf;
    Analyzer analyzer;
    NavBar nav;
    LoudnessHalo halo;
    juce::OwnedArray<ModulePanel> panels;
    juce::OwnedArray<ModuleCard> cards;
    MeterBar meters;
    AdviceBox advice;
    int current = 0, frame = 0;
    juce::Rectangle<int> linkArea;
};

class UBMasterEditor : public juce::AudioProcessorEditor
{
public:
    explicit UBMasterEditor (UBMasterProcessor&);
    ~UBMasterEditor() override = default;
    void paint (juce::Graphics& g) override { g.fillAll (theme::bg); }
    void resized() override;
    UBMasterMain& getMain() { return main; }
private:
    UBMasterMain main;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UBMasterEditor)
};
