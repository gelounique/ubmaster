// Renders the UBMaster editor to PNG while synthetic music plays through the processor
// usage: UBMasterSnapshot out.png [module 0-5] [scale]
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <random>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String out = argc > 1 ? argv[1] : "/tmp/ubmaster.png";
    const int module = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 1.0f;

    UBMasterProcessor proc;
    const double sr = 48000;
    proc.prepareToPlay (sr, 800);
    proc.setParam (ids::limGain, 7.5f);
    proc.setParam (ids::eqBand (1, "gain"), 1.5f); proc.setParam (ids::eqBand (3, "gain"), -1.8f); proc.setParam (ids::eqBand (4, "gain"), 2.2f);
    proc.setParam (ids::eqBand (3, "freq"), 3200.0f); proc.setParam (ids::eqBand (0, "freq"), 28.0f);
    proc.setParam (ids::stHigh, 135.0f); proc.setParam (ids::stLow, 60.0f); proc.setParam (ids::stMid, 110.0f);
    proc.setParam (ids::trAmount, 45.0f);
    if (module == 0) proc.setParam (ids::eqMode, 1), proc.setParam (ids::eqBand (4, "tgt"), 2);

    std::unique_ptr<UBMasterEditor> ed (dynamic_cast<UBMasterEditor*> (proc.createEditor()));
    ed->setSize (UBMasterMain::W, UBMasterMain::H);
    auto& m = ed->getMain();
    m.select (module);

    std::mt19937 rng (3); std::normal_distribution<float> nd (0, 1);
    juce::AudioBuffer<float> b (2, 800); juce::MidiBuffer midi;
    long long n = 0;
    for (int f = 0; f < 60 * 14; ++f)   // 14 s of "music", 60 fps
    {
        for (int i = 0; i < 800; ++i, ++n)
        {
            const double t = n / sr, tb = std::fmod (t, 0.5), th = std::fmod (t, 0.125);
            const float kick = (float) (std::exp (-tb * 16) * std::sin (2 * M_PI * (48 + 110 * std::exp (-tb * 28)) * tb));
            const float bass = 0.3f * (float) std::sin (2 * M_PI * 55 * t) * (float) (0.7 + 0.3 * std::sin (t * 2));
            const float ch = 0.09f * (float) (std::sin (2 * M_PI * 220 * t) + std::sin (2 * M_PI * 277.2 * t) + std::sin (2 * M_PI * 329.6 * t) + std::sin (2 * M_PI * 440 * t));
            const float hat = (float) std::exp (-th * 90) * nd (rng) * 0.14f;
            const float air = nd (rng) * 0.015f;
            b.setSample (0, i, 0.32f * (kick + bass + ch * 1.15f + hat + air + 0.04f * (float) std::sin (2 * M_PI * 1760 * t)));
            b.setSample (1, i, 0.32f * (kick + bass + ch * 0.85f - hat * 0.7f + air));
        }
        proc.processBlock (b, midi);
        m.tick (1.0f / 60.0f);
    }
    auto img = m.createComponentSnapshot (m.getLocalBounds(), true, scale);
    juce::File f (out); f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png; png.writeImageToStream (img, os);
    std::printf ("wrote %s (%dx%d)  I=%.1f S=%.1f TP=%.1f GR=%.1f\n", out.toRawUTF8(), img.getWidth(), img.getHeight(),
                 proc.meter.integrated.load(), proc.meter.shortTerm.load(), proc.meter.truePeakDb.load(), proc.limiter.grDb.load());
    ed.reset();
    return 0;
}
