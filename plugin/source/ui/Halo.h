#pragma once
#include "Widgets.h"

/** "Loudness Halo": radial spectrum ring of fine bars (vinyl grooves) in Tiffany, UB logo in the middle, big LUFS below,
    an outer loudness gauge (-36 .. -4 LUFS over 270°) with a glowing target marker. */
class LoudnessHalo : public juce::Component
{
public:
    LoudnessHalo (UBMasterProcessor& p, Analyzer& a) : proc (p), an (a) {}
    void tick (float dt);
    void paint (juce::Graphics&) override;
    static float angleFor (float lufs);   // radians, 0 = up, clockwise
private:
    UBMasterProcessor& proc;
    Analyzer& an;
    static constexpr int kBars = 180;
    std::array<float, kBars> bars {};
    float phase = 0, shown = -100, pulse = 0;
};
