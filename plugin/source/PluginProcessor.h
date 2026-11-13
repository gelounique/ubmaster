#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/EQ.h"
#include "dsp/Saturation.h"
#include "dsp/Transient.h"
#include "dsp/Dynamics.h"
#include "dsp/Loudness.h"

namespace ids
{
    // global
    inline const juce::String mode { "mode" }, target { "target" };
    // modules
    inline const juce::String eqOn { "eq_on" }, eqMode { "eq_mode" };
    inline juce::String eqBand (int b, const char* what) { return "eq" + juce::String (b + 1) + "_" + what; }   // on freq gain q tgt
    inline const juce::String tapeOn { "tape_on" }, tapeDrive { "tape_drive" }, tapeLo { "tape_lo" }, tapeHi { "tape_hi" }, tapeChar { "tape_char" }, tapeMix { "tape_mix" };
    inline const juce::String trOn { "tr_on" }, trMode { "tr_mode" }, trAmount { "tr_amount" }, trSens { "tr_sens" }, trSpeed { "tr_speed" },
                              trLo { "tr_lo" }, trHi { "tr_hi" }, trMix { "tr_mix" }, trDelta { "tr_delta" };
    inline const juce::String stOn { "st_on" }, stLow { "st_low" }, stMid { "st_mid" }, stHigh { "st_high" }, stXLo { "st_xlo" }, stXHi { "st_xhi" };
    inline const juce::String exOn { "ex_on" }, exAmount { "ex_amount" }, exFreq { "ex_freq" }, exChar { "ex_char" };
    inline const juce::String limOn { "lim_on" }, limGain { "lim_gain" }, limCeil { "lim_ceiling" }, limRelease { "lim_release" };
}

/** Streaming presets (target LUFS, recommended true-peak ceiling) */
struct Platform { const char* name; float lufs; float ceiling; const char* note; };
inline const std::array<Platform, 5>& platforms()
{
    static const std::array<Platform, 5> p { {
        { "Spotify",     -14.0f, -1.0f, "Spotify normalisiert auf -14 LUFS. Bist du lauter, nimm -2 dBTP Ceiling." },
        { "Apple Music", -16.0f, -1.0f, "Apple Music (Sound Check) spielt auf -16 LUFS ab und dreht lautere Tracks runter." },
        { "YouTube",     -14.0f, -1.0f, "YouTube dreht nur runter: alles über -14 LUFS wird leiser abgespielt." },
        { "SoundCloud",  -14.0f, -1.0f, "SoundCloud normalisiert nicht überall – -14 LUFS / -1 dBTP ist der sichere Wert." },
        { "Custom",      -14.0f, -1.0f, "Eigener Zielwert – z. B. -9 LUFS für Club, -23 LUFS für Broadcast." },
    } };
    return p;
}

class UBMasterProcessor : public juce::AudioProcessor
{
public:
    UBMasterProcessor();
    ~UBMasterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "UBMaster"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- UI / helpers ----
    juce::AudioProcessorValueTreeState apvts;
    ubm::TransientShaper transient;
    ubm::Limiter limiter;
    ubm::LoudnessMeter meter;

    float get (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    void setParam (const juce::String& id, float plainValue);
    void selectPlatform (int index);       // sets mode + target
    float matchTargetLoudness();           // moves limiter gain so integrated (or short-term) hits target; returns change in dB
    double getSampleRateSafe() const { return currentSR; }

    // scope / spectrum tap (output, lock-free, display only)
    static constexpr int kScope = 8192;
    std::array<std::atomic<float>, kScope> scopeL {}, scopeR {};
    std::atomic<int> scopeWrite { 0 };
    std::atomic<float> correlation { 1.0f };
    std::atomic<float> inPeakDb { -100.0f };

private:
    void processStereo (float* L, float* R, int n);
    double currentSR = 48000.0;
    int maxBlock = 512;
    juce::AudioBuffer<float> tmp;
    ubm::MasterEQ eq;
    ubm::TapeSim tape;
    ubm::StereoTool stereo;
    ubm::Exciter exciter;
    double corrLR = 0, corrLL = 1e-9, corrRR = 1e-9;

    // raw parameter pointers (no string work on the audio thread)
    struct Raw
    {
        std::atomic<float> *eqOn {}, *eqMode {}, *band[ubm::kEqBands][5] {};
        std::atomic<float> *tapeOn {}, *tapeDrive {}, *tapeLo {}, *tapeHi {}, *tapeChar {}, *tapeMix {};
        std::atomic<float> *trOn {}, *trMode {}, *trAmount {}, *trSens {}, *trSpeed {}, *trLo {}, *trHi {}, *trMix {}, *trDelta {};
        std::atomic<float> *stOn {}, *stLow {}, *stMid {}, *stHigh {}, *stXLo {}, *stXHi {};
        std::atomic<float> *exOn {}, *exAmount {}, *exFreq {}, *exChar {};
        std::atomic<float> *limOn {}, *limGain {}, *limCeil {}, *limRelease {};
    } raw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UBMasterProcessor)
};
