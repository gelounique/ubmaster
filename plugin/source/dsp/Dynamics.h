#pragma once
// UBMaster — Stereo-Tool (3-band width), True-Peak interpolator, Lookahead True-Peak Limiter.
#include "Biquad.h"
#include <array>
#include <atomic>
#include <vector>

namespace ubm
{
// ---------------------------------------------------------------------------------------------------------------
/** 3-band stereo width on the side signal. Bands are complementary (low + mid + high == side exactly),
    so 100 / 100 / 100 % is bit-identical bypass. */
class StereoTool
{
public:
    void prepare (double sampleRate) { sr = sampleRate; reset(); curLo = curHi = -1; for (auto& w : wS) w = 1.0f; }
    void reset() { lpLo.reset(); lpHi.reset(); }

    void process (float* L, float* R, int n, bool on, float wl, float wm, float wh, float xLo, float xHi)
    {
        xHi = std::max (xHi, xLo * 1.5f);
        if (std::abs (xLo - curLo) > 0.5f || std::abs (xHi - curHi) > 0.5f)
        {
            curLo = xLo; curHi = xHi;
            lpLo.c = lowpass (sr, xLo, 0.71); lpHi.c = lowpass (sr, xHi, 0.71);
        }
        const float t[3] { on ? wl : 1.0f, on ? wm : 1.0f, on ? wh : 1.0f };
        const float k = onePoleCoef (30.0, sr);
        for (int i = 0; i < n; ++i)
        {
            for (int b = 0; b < 3; ++b) wS[b] += (t[b] - wS[b]) * k;
            const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]);
            const float sl = lpLo.process (s);
            const float sh = s - lpHi.process (s);
            const float sm = s - sl - sh;
            const float s2 = wS[0] * sl + wS[1] * sm + wS[2] * sh;
            L[i] = m + s2; R[i] = m - s2;
        }
    }
private:
    double sr = 48000.0;
    Biquad lpLo, lpHi;
    float curLo = -1, curHi = -1, wS[3] { 1, 1, 1 };
};

// ---------------------------------------------------------------------------------------------------------------
/** 8x polyphase Kaiser-windowed-sinc interpolator (true-peak estimate, finer than the BS.1770 minimum of 4x so that
    inter-sample peaks near Nyquist are not missed). Delay: kTaps/2 samples. */
class TruePeak
{
public:
    static constexpr int kTaps = 64, kPhases = 8;
    TruePeak()
    {
        for (int p = 0; p < kPhases; ++p)
            for (int t = 0; t < kTaps; ++t)
            {
                const double x = (double) (t - kTaps / 2 + 1) - (double) p / kPhases;   // offset from centre
                const double sinc = std::abs (x) < 1e-9 ? 1.0 : std::sin (kPi * x) / (kPi * x);
                const double u = x / (kTaps / 2.0);                                         // Kaiser window, beta 7
                const double w = std::abs (u) >= 1.0 ? 0.0 : bessel0 (9.0 * std::sqrt (1.0 - u * u)) / bessel0 (9.0);
                h[p][t] = (float) (sinc * w);
            }
        for (int p = 0; p < kPhases; ++p)   // unity DC gain per phase
        {
            float sum = 0; for (int t = 0; t < kTaps; ++t) sum += h[p][t];
            for (int t = 0; t < kTaps; ++t) h[p][t] /= sum;
        }
        reset();
    }
    static double bessel0 (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; }
    void reset() { for (auto& b : buf) b.fill (0.0f); idx = 0; }
    /** push one stereo sample, returns max |x| over the 8 interpolated points (L & R) */
    inline float push (float l, float r)
    {
        buf[0][(size_t) idx] = buf[0][(size_t) idx + kTaps] = l;
        buf[1][(size_t) idx] = buf[1][(size_t) idx + kTaps] = r;
        idx = (idx + 1) % kTaps;
        float m = 0.0f;
        for (int c = 0; c < 2; ++c)
        {
            const float* x = buf[(size_t) c].data() + idx;   // oldest .. newest
            for (int p = 0; p < kPhases; ++p)
            {
                float acc = 0.0f;
                for (int t = 0; t < kTaps; ++t) acc += h[p][t] * x[kTaps - 1 - t];
                m = std::max (m, std::abs (acc));
            }
        }
        return m;
    }
private:
    float h[kPhases][kTaps] {};
    std::array<std::array<float, kTaps * 2>, 2> buf {};
    int idx = 0;
};

// ---------------------------------------------------------------------------------------------------------------
/** Lookahead true-peak brickwall limiter: gain -> min-hold over the lookahead -> release -> box smoothing.
    The box smoother never lets the gain rise above the requirement at the peak -> no overs. */
class Limiter
{
public:
    std::atomic<float> grDb { 0.0f };   // current gain reduction (<= 0)

    int getLatency() const { return delay; }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        L = std::max (16, (int) std::round (0.0015 * sr));        // 1.5 ms lookahead
        hold = L + TruePeak::kTaps / 2 + 3;
        delay = L + TruePeak::kTaps / 2;
        holdBuf.assign ((size_t) hold, 1.0f);
        dq.assign ((size_t) hold + 1, {});
        boxBuf.assign ((size_t) L, 1.0f);
        for (auto& d : dly) d.assign ((size_t) delay + 1, 0.0f);
        reset();
    }
    void reset()
    {
        tp.reset();
        std::fill (holdBuf.begin(), holdBuf.end(), 1.0f); std::fill (boxBuf.begin(), boxBuf.end(), 1.0f);
        for (auto& d : dly) std::fill (d.begin(), d.end(), 0.0f);
        hIdx = bIdx = dIdx = 0; boxSum = (double) L; rel = 1.0f; dqHead = dqTail = 0; tick = 0;
    }

    void process (float* l, float* r, int n, bool on, float inGainDb, float ceilingDb, float releaseMs)
    {
        const float ceil = dbToGain (ceilingDb);
        const float relK = onePoleCoef (std::max (1.0f, releaseMs), sr);
        const float gTarget = on ? dbToGain (inGainDb) : 1.0f;
        const float k = onePoleCoef (20.0, sr);
        float minG = 1.0f;
        for (int i = 0; i < n; ++i)
        {
            inG += (gTarget - inG) * k;
            onS += ((on ? 1.0f : 0.0f) - onS) * k;
            const float xl = l[i] * inG, xr = r[i] * inG;
            const float pk = std::max (tp.push (xl, xr), std::max (std::abs (xl), std::abs (xr)));
            const float req = pk > ceil ? ceil / pk : 1.0f;
            const float h = holdMin (req);
            rel = h < rel ? h : rel + (h - rel) * relK;
            boxSum += (double) rel - (double) boxBuf[(size_t) bIdx];
            boxBuf[(size_t) bIdx] = rel; bIdx = (bIdx + 1) % L;
            float g = (float) (boxSum / L);
            g = std::min (g, 1.0f);
            const float gain = 1.0f + (g - 1.0f) * onS;
            // delay line
            dly[0][(size_t) dIdx] = xl; dly[1][(size_t) dIdx] = xr;
            const int rIdx = (dIdx + 1) % (delay + 1);
            dIdx = rIdx;
            float yl = dly[0][(size_t) rIdx] * gain, yr = dly[1][(size_t) rIdx] * gain;
            if (onS > 0.999f) { yl = std::clamp (yl, -ceil, ceil); yr = std::clamp (yr, -ceil, ceil); }   // safety, should not engage
            l[i] = yl; r[i] = yr;
            minG = std::min (minG, gain);
        }
        // display value: instant attack, ~25 dB/s recovery (so short peaks stay visible in the UI)
        const float blk = gainToDb (minG, -60.0f);
        grDisp = std::min (blk, grDisp + 25.0f * (float) n / (float) sr);
        grDb.store (std::min (0.0f, grDisp), std::memory_order_relaxed);
    }

private:
    /** sliding-window minimum over the last `hold` samples (monotonic deque, O(1) amortised) */
    inline float holdMin (float v)
    {
        // deque stored in fixed ring: values + positions
        const int cap = hold + 1;
        while (dqTail != dqHead)
        {
            const int last = (dqTail - 1 + cap) % cap;
            if (dq[(size_t) last].v >= v) dqTail = last; else break;
        }
        dq[(size_t) dqTail] = { v, tick }; dqTail = (dqTail + 1) % cap;
        while (dq[(size_t) dqHead].t <= tick - hold) dqHead = (dqHead + 1) % cap;
        ++tick;
        return dq[(size_t) dqHead].v;
    }
    struct DQ { float v = 1.0f; long long t = 0; };

    double sr = 48000.0;
    int L = 72, hold = 83, delay = 80, hIdx = 0, bIdx = 0, dIdx = 0, dqHead = 0, dqTail = 0;
    long long tick = 0;
    double boxSum = 72.0;
    float rel = 1.0f, inG = 1.0f, onS = 1.0f, grDisp = 0.0f;
    TruePeak tp;
    std::vector<float> holdBuf, boxBuf, dly[2];
    std::vector<DQ> dq;
};
}
