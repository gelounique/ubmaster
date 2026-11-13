#pragma once
// UBMaster — adaptive spectral transient processor (Spiff-style).
// STFT (sqrt-Hann, 75 % overlap). Per bin a short "now" power is compared with an adaptive reference that follows the
// recent spectrum; the excess (in dB, above a sensitivity threshold) is a transient. The detection is smoothed into 48
// log-spaced bands (~1/5 octave), turned into a cut or boost per band with fast attack / musical release and applied to
// both channels (linked -> stable stereo image). Freq range limits where it works. Delta = hear only what changes.
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"
#include <array>
#include <atomic>
#include <vector>

namespace ubm
{
class TransientShaper
{
public:
    static constexpr int kBands = 48;
    std::array<std::atomic<float>, kBands> displayDb {};     // current gain per band (dB) for the UI
    static float bandCentre (int b) { return 20.0f * std::pow (1000.0f, (b + 0.5f) / (float) kBands); }   // 20 Hz .. 20 kHz

    int getLatency() const { return N; }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        order = sr > 64000.0 ? 10 : 9;            // 512 @ 44.1/48k, 1024 @ 88.2/96k -> ~10 ms frames, ~2.7 ms hop
        N = 1 << order; hop = N / 4;
        fft = std::make_unique<juce::dsp::FFT> (order);
        win.resize ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = std::sqrt (0.5f - 0.5f * std::cos (2.0f * (float) kPi * (float) i / (float) N));
        for (int c = 0; c < 2; ++c) { in[c].assign ((size_t) N, 0.0f); out[c].assign ((size_t) N, 0.0f); spec[c].assign ((size_t) N * 2, 0.0f); }
        const int bins = N / 2 + 1;
        ref.assign ((size_t) bins, 0.0f);
        binBand.resize ((size_t) bins); binFrac.resize ((size_t) bins);
        for (int k = 0; k < bins; ++k)
        {
            const float f = std::max (1.0f, (float) (k * sr / N));
            const float lp = std::log (f / 20.0f) / std::log (1000.0f) * kBands - 0.5f;
            const float p = std::clamp (lp, 0.0f, (float) kBands - 1.001f);
            binBand[(size_t) k] = (int) p; binFrac[(size_t) k] = p - (float) (int) p;
        }
        for (auto& g : gEnv) g = 0.0f;
        for (auto& g : dispHold) g = 0.0f;
        for (auto& d : displayDb) d.store (0.0f);
        pos = 0; hopCount = 0; mixS = 0.0f; primed = 0;
    }

    struct Params { bool on = false; bool boost = false; float amount = 0.5f, sensitivity = 0.5f, speedMs = 40.0f, lo = 20, hi = 20000, mix = 1.0f; bool delta = false; };

    void process (float* L, float* R, int n, const Params& p)
    {
        prm = p;
        float* io[2] { L, R };
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                in[c][(size_t) pos] = io[c][i];
                io[c][i] = out[c][(size_t) pos];
                out[c][(size_t) pos] = 0.0f;
            }
            pos = (pos + 1) % N;
            if (++hopCount == hop) { hopCount = 0; frame(); }
        }
    }

private:
    void frame()
    {
        const int bins = N / 2 + 1;
        for (int c = 0; c < 2; ++c)
        {
            auto& s = spec[c];
            for (int k = 0; k < N; ++k) s[(size_t) k] = in[c][(size_t) ((pos + k) % N)] * win[(size_t) k];
            std::fill (s.begin() + N, s.end(), 0.0f);
            fft->performRealOnlyForwardTransform (s.data(), true);
        }

        // --- detection ---
        const float hopSec = (float) hop / (float) sr;
        const float refCoef = 1.0f - std::exp (-hopSec / (std::max (5.0f, prm.speedMs) * 0.001f * 1.6f));
        const float relCoef = 1.0f - std::exp (-hopSec / (std::max (5.0f, prm.speedMs) * 0.001f));
        const float thrDb = 14.0f - 11.0f * prm.sensitivity;        // sensitivity 0..1 -> 14 .. 3 dB above reference
        std::array<float, kBands> acc {}, wsum {};
        for (int k = 1; k < bins; ++k)
        {
            const float re0 = spec[0][(size_t) (2 * k)], im0 = spec[0][(size_t) (2 * k + 1)];
            const float re1 = spec[1][(size_t) (2 * k)], im1 = spec[1][(size_t) (2 * k + 1)];
            const float P = re0 * re0 + im0 * im0 + re1 * re1 + im1 * im1 + 1e-14f;
            float& r = ref[(size_t) k];
            const float excess = primed > 4 ? 10.0f * std::log10 (P / (r + 1e-12f)) : 0.0f;
            r += (P - r) * (P > r ? refCoef : refCoef * 0.6f);
            const float e = std::max (0.0f, excess - thrDb);
            const float w = std::sqrt (P);                            // energy-weighted band average
            const int b = binBand[(size_t) k]; const float fr = binFrac[(size_t) k];
            acc[(size_t) b] += e * w * (1 - fr); wsum[(size_t) b] += w * (1 - fr);
            acc[(size_t) b + 1] += e * w * fr;   wsum[(size_t) b + 1] += w * fr;
        }
        if (primed <= 4) ++primed;

        const float on = prm.on ? 1.0f : 0.0f;
        const float sign = prm.boost ? 1.0f : -1.0f;
        const float maxDb = prm.boost ? 12.0f : 24.0f;
        std::array<float, kBands> gLin {};
        for (int b = 0; b < kBands; ++b)
        {
            const float f = bandCentre (b);
            float w = 1.0f;                                           // soft 1/3-oct edges on the frequency range
            if (f < prm.lo) w = std::max (0.0f, 1.0f - std::log2 (prm.lo / f) * 3.0f);
            if (f > prm.hi) w = std::min (w, std::max (0.0f, 1.0f - std::log2 (f / prm.hi) * 3.0f));
            const float e = wsum[(size_t) b] > 0 ? acc[(size_t) b] / wsum[(size_t) b] : 0.0f;
            const float target = on * sign * std::min (maxDb, e * prm.amount * 1.1f) * w;
            float& g = gEnv[(size_t) b];
            if (std::abs (target) > std::abs (g) && target * g >= 0.0f) g += (target - g) * 0.85f;   // fast attack
            else g += (target - g) * relCoef;                                                       // release
            const float shown = g * mixS;
            float& dh = dispHold[(size_t) b];
            dh = std::abs (shown) > std::abs (dh) ? shown : dh * 0.985f;   // hold + ~0.5 s decay for the UI
            displayDb[(size_t) b].store (dh, std::memory_order_relaxed);
            gLin[(size_t) b] = dbToGain (g) - 1.0f;
        }
        const float mixT = on * prm.mix;
        mixS += (mixT - mixS) * 0.25f;

        // --- apply ---
        for (int k = 0; k < bins; ++k)
        {
            const int b = binBand[(size_t) k]; const float fr = binFrac[(size_t) k];
            const float d = (gLin[(size_t) b] * (1 - fr) + gLin[(size_t) std::min (b + 1, kBands - 1)] * fr) * mixS;
            const float G = prm.delta ? d : 1.0f + d;
            for (int c = 0; c < 2; ++c) { spec[c][(size_t) (2 * k)] *= G; spec[c][(size_t) (2 * k + 1)] *= G; }
        }
        for (int c = 0; c < 2; ++c)
        {
            auto& s = spec[c];
            fft->performRealOnlyInverseTransform (s.data());
            for (int k = 0; k < N; ++k) out[c][(size_t) ((pos + k) % N)] += s[(size_t) k] * win[(size_t) k] * 0.5f;   // sqrt-Hann^2 @ 75 % = 2
        }
    }

    double sr = 48000.0;
    int order = 9, N = 512, hop = 128, pos = 0, hopCount = 0, primed = 0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> win, in[2], out[2], spec[2], ref, binFrac;
    std::vector<int> binBand;
    std::array<float, kBands> gEnv {}, dispHold {};
    float mixS = 0.0f;
    Params prm;
};
}
