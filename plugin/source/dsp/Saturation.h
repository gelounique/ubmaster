#pragma once
// UBMaster — Tape-Sim (saturation only inside a chosen frequency range) and Exciter.
// Both work on a "distortion residue": the selected band is driven into the curve at 2x oversampling and only the
// difference (= the generated harmonics + compression) is added to the untouched dry signal.
// -> drive 0 / amount 0 is bit-transparent, the dry path keeps its phase (no crossover colouring).
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"

namespace ubm
{
class ResidueOS
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os->initProcessing ((size_t) maxBlock);
        band.setSize (2, maxBlock);
    }
    void reset() { if (os) os->reset(); }
    juce::AudioBuffer<float> band;   // fill with the selected band (base rate), then run()
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    double sr = 48000.0;

    /** shaper(x) -> y at 2x; band buffer is replaced by (shaped - input) at base rate */
    template <typename Fn> void run (int n, Fn&& shaper)
    {
        juce::dsp::AudioBlock<float> blk (band.getArrayOfWritePointers(), 2, (size_t) n);
        auto up = os->processSamplesUp (blk);
        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* d = up.getChannelPointer (ch);
            for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = shaper ((int) ch, d[i]) - d[i];
        }
        os->processSamplesDown (blk);
    }
};

/** Tape: drive 0..24 dB, range lo..hi Hz, character = asymmetry (even harmonics), mix 0..1 */
class TapeSim
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate; res.prepare (sampleRate, maxBlock);
        for (int c = 0; c < 2; ++c) { dc[c] = 0; dcy[c] = 0; }
        reset();
    }
    void reset() { res.reset(); for (auto& c : f) for (auto& b : c) b.reset(); for (int c = 0; c < 2; ++c) dc[c] = dcy[c] = 0; }

    void process (float* L, float* R, int n, bool on, float driveDb, float lo, float hi, float character, float mix)
    {
        hi = std::max (hi, lo * 1.25f);
        if (std::abs (lo - curLo) > 0.5f || std::abs (hi - curHi) > 0.5f)
        {
            curLo = lo; curHi = hi;
            for (int c = 0; c < 2; ++c)
            {
                f[c][0].c = highpass (sr, lo, 0.5412); f[c][1].c = highpass (sr, lo, 1.3066);
                f[c][2].c = lowpass (sr, hi, 0.5412);  f[c][3].c = lowpass (sr, hi, 1.3066);
            }
        }
        const float targetAmt = on ? mix : 0.0f;
        float* io[2] { L, R };
        for (int c = 0; c < 2; ++c)
        {
            auto* b = res.band.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                float x = io[c][i];
                for (auto& s : f[c]) x = s.process (x);
                b[i] = x;
            }
        }
        if (amt < 1e-4f && targetAmt < 1e-4f) { amt = 0; return; }   // fully bypassed (filters keep running = no click on enable)

        const float g = dbToGain (driveDb);
        const float a = 0.45f * character;                         // asymmetry offset
        const float ta = std::tanh (a), norm = 1.0f / (g * (1.0f - ta * ta));
        res.run (n, [&] (int, float x) {
            const float y = (std::tanh (g * x + a) - ta) * norm;   // unity small-signal slope
            return y;
        });
        const float k = onePoleCoef (25.0, sr);
        for (int c = 0; c < 2; ++c)
        {
            const auto* d = res.band.getReadPointer (c);
            float am = amt;
            for (int i = 0; i < n; ++i)
            {
                am += (targetAmt - am) * k;
                // DC blocker on the residue (asymmetry creates DC)
                const float x = d[i];
                const float y = x - dc[c] + 0.9995f * dcy[c];
                dc[c] = x; dcy[c] = y;
                io[c][i] += am * y;
            }
            if (c == 1) amt = am;
        }
    }

    double sr = 48000.0;
private:
    ResidueOS res;
    Biquad f[2][4];
    float curLo = -1, curHi = -1, amt = 0;
    float dc[2] {}, dcy[2] {};
};

/** Exciter: high-pass sidechain from freq, harmonic generator (Tube = even, Tape = odd blend), amount 0..1 */
class Exciter
{
public:
    void prepare (double sampleRate, int maxBlock) { sr = sampleRate; res.prepare (sampleRate, maxBlock); reset(); }
    void reset() { res.reset(); for (auto& c : hp) for (auto& b : c) b.reset(); for (auto& c : post) for (auto& b : c) b.reset(); }

    void process (float* L, float* R, int n, bool on, float amount, float freq, float character)
    {
        if (std::abs (freq - curF) > 0.5f)
        {
            curF = freq;
            for (int c = 0; c < 2; ++c)
            {
                hp[c][0].c = highpass (sr, freq, 0.5412); hp[c][1].c = highpass (sr, freq, 1.3066);
                post[c][0].c = highpass (sr, freq * 1.5, 0.71); post[c][1].c = lowpass (sr, std::min (19500.0, sr * 0.45), 0.71);
            }
        }
        const float target = on ? amount : 0.0f;
        float* io[2] { L, R };
        for (int c = 0; c < 2; ++c)
        {
            auto* b = res.band.getWritePointer (c);
            for (int i = 0; i < n; ++i) { float x = io[c][i]; x = hp[c][0].process (x); b[i] = hp[c][1].process (x); }
        }
        if (amt < 1e-4f && target < 1e-4f) { amt = 0; return; }

        const float drive = 4.0f, even = 1.0f - character;
        res.run (n, [&] (int, float x) {
            const float s = std::tanh (x * drive);
            return (s + 0.6f * even * s * s) / drive;   // odd (tanh) + even (square) harmonics
        });
        const float k = onePoleCoef (25.0, sr);
        for (int c = 0; c < 2; ++c)
        {
            const auto* d = res.band.getReadPointer (c);
            float am = amt;
            for (int i = 0; i < n; ++i)
            {
                am += (target - am) * k;
                float y = post[c][0].process (d[i]);
                y = post[c][1].process (y);
                io[c][i] += am * 2.5f * y;
            }
            if (c == 1) amt = am;
        }
    }
    double sr = 48000.0;
private:
    ResidueOS res;
    Biquad hp[2][2], post[2][2];
    float curF = -1, amt = 0;
};
}
