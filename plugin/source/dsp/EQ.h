#pragma once
// UBMaster — Master-EQ: 6 bands (Low Cut, Low Shelf, Bell, Bell, High Shelf, High Cut)
// Modes: Global (stereo-linked) · Mid/Side · Left/Right. In M/S or L/R every band can act on both, A (Mid/Left) or B (Side/Right).
#include "Biquad.h"
#include <array>

namespace ubm
{
enum class BandType { LowCut, LowShelf, Bell, HighShelf, HighCut };
enum EqMode { EqGlobal = 0, EqMidSide = 1, EqLeftRight = 2 };

struct BandSpec { BandType type; const char* name; float defFreq, defGain, defQ; bool defOn; };
constexpr int kEqBands = 6;
inline const std::array<BandSpec, kEqBands>& bandSpecs()
{
    static const std::array<BandSpec, kEqBands> s { {
        { BandType::LowCut,    "LOW CUT",    30.0f,   0.0f, 0.71f, true  },
        { BandType::LowShelf,  "LOW SHELF",  90.0f,   0.0f, 0.71f, true  },
        { BandType::Bell,      "LOW MID",    320.0f,  0.0f, 0.90f, true  },
        { BandType::Bell,      "HIGH MID",   2800.0f, 0.0f, 0.90f, true  },
        { BandType::HighShelf, "HIGH SHELF", 9500.0f, 0.0f, 0.71f, true  },
        { BandType::HighCut,   "HIGH CUT",   20000.0f,0.0f, 0.71f, false },
    } };
    return s;
}

struct BandParams { bool on = true; float freq = 1000, gain = 0, q = 0.71f; int target = 0; };

/** Coefficients of one band (cuts are 24 dB/oct = 2 stages; stage 2 identity for the rest). */
inline void bandCoeffs (BandType t, double sr, float f, float g, float q, Coeffs& s1, Coeffs& s2)
{
    s2 = Coeffs {};
    switch (t)
    {
        case BandType::LowCut:   s1 = highpass (sr, f, 0.5412 * q / 0.71); s2 = highpass (sr, f, 1.3066 * q / 0.71); break;
        case BandType::HighCut:  s1 = lowpass (sr, f, 0.5412 * q / 0.71);  s2 = lowpass (sr, f, 1.3066 * q / 0.71); break;
        case BandType::LowShelf: s1 = lowShelf (sr, f, q, g); break;
        case BandType::HighShelf:s1 = highShelf (sr, f, q, g); break;
        case BandType::Bell:     s1 = peak (sr, f, q, g); break;
    }
}

/** Does band with `target` act on processing channel ch in this mode? */
inline bool bandHits (int mode, int target, int ch) { return mode == EqGlobal || target == 0 || target == ch + 1; }

class MasterEQ
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (int b = 0; b < kEqBands; ++b) { cur[b] = neutral (b); first = true; }
        reset();
    }
    void reset() { for (auto& ch : st) for (auto& b : ch) { b[0].reset(); b[1].reset(); } }

    /** in-place stereo processing. params = targets; smoothed internally per 32-sample slice. */
    void process (float* L, float* R, int n, int mode, const std::array<BandParams, kEqBands>& p)
    {
        if (mode != lastMode) { reset(); lastMode = mode; }
        for (int start = 0; start < n; start += 32)
        {
            const int len = std::min (32, n - start);
            updateCoeffs (mode, p, first ? 1.0f : 0.18f);
            first = false;
            float* a = L + start; float* b = R + start;
            if (mode == EqMidSide)
                for (int i = 0; i < len; ++i) { const float m = 0.5f * (a[i] + b[i]), s = 0.5f * (a[i] - b[i]); a[i] = m; b[i] = s; }
            for (int ch = 0; ch < 2; ++ch)
            {
                float* x = ch == 0 ? a : b;
                for (int bd = 0; bd < kEqBands; ++bd)
                {
                    if (! active[ch][bd]) continue;
                    auto& f = st[ch][bd];
                    const bool two = bandSpecs()[(size_t) bd].type == BandType::LowCut || bandSpecs()[(size_t) bd].type == BandType::HighCut;
                    for (int i = 0; i < len; ++i) x[i] = f[0].process (x[i]);
                    if (two) for (int i = 0; i < len; ++i) x[i] = f[1].process (x[i]);
                }
            }
            if (mode == EqMidSide)
                for (int i = 0; i < len; ++i) { const float m = a[i], s = b[i]; a[i] = m + s; b[i] = m - s; }
        }
    }

private:
    struct Cur { float lf, g, q; };
    Cur neutral (int b) const
    {
        const auto& s = bandSpecs()[(size_t) b];
        if (s.type == BandType::LowCut)  return { std::log (8.0f), 0.0f, s.defQ };
        if (s.type == BandType::HighCut) return { std::log ((float) std::min (24000.0, sr * 0.48)), 0.0f, s.defQ };
        return { std::log (s.defFreq), 0.0f, s.defQ };
    }

    void updateCoeffs (int mode, const std::array<BandParams, kEqBands>& p, float k)
    {
        for (int b = 0; b < kEqBands; ++b)
        {
            const auto& s = bandSpecs()[(size_t) b];
            const bool cut = s.type == BandType::LowCut || s.type == BandType::HighCut;
            Cur t = p[(size_t) b].on ? Cur { std::log (std::max (10.0f, p[(size_t) b].freq)), p[(size_t) b].gain, p[(size_t) b].q } : neutral (b);
            auto& c = cur[b];
            const bool moving = std::abs (c.lf - t.lf) > 1e-4f || std::abs (c.g - t.g) > 1e-3f || std::abs (c.q - t.q) > 1e-4f || dirty[b] || mode != coeffMode[b];
            if (moving)
            {
                c.lf += (t.lf - c.lf) * k; c.g += (t.g - c.g) * k; c.q += (t.q - c.q) * k;
                if (std::abs (c.lf - t.lf) < 1e-4f) c.lf = t.lf;
                if (std::abs (c.g - t.g) < 1e-3f) c.g = t.g;
                if (std::abs (c.q - t.q) < 1e-4f) c.q = t.q;
                Coeffs a, bb;
                bandCoeffs (s.type, sr, std::exp (c.lf), c.g, c.q, a, bb);
                for (int ch = 0; ch < 2; ++ch) { st[ch][b][0].c = a; st[ch][b][1].c = bb; }
                dirty[b] = false; coeffMode[b] = mode;
            }
            // a band is inactive when it is off AND has glided to neutral (cuts) / zero gain (shelves, bells)
            const bool neutralNow = ! p[(size_t) b].on && (cut ? std::abs (c.lf - t.lf) < 1e-3f : std::abs (c.g) < 1e-3f);
            for (int ch = 0; ch < 2; ++ch)
            {
                const bool hit = bandHits (mode, p[(size_t) b].target, ch);
                const bool want = hit && ! neutralNow;
                if (want && ! active[ch][b]) { st[ch][b][0].reset(); st[ch][b][1].reset(); }
                active[ch][b] = want;
            }
        }
    }

    double sr = 48000.0;
    Biquad st[2][kEqBands][2];
    Cur cur[kEqBands] {};
    bool active[2][kEqBands] {};
    bool dirty[kEqBands] { true, true, true, true, true, true };
    int coeffMode[kEqBands] { -1, -1, -1, -1, -1, -1 };
    int lastMode = -1;
    bool first = true;
};
}
