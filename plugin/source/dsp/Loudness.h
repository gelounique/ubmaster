#pragma once
// UBMaster — ITU-R BS.1770-4 / EBU R128 loudness meter: Momentary (400 ms), Short-term (3 s), Integrated (gated),
// Loudness Range (EBU Tech 3342) and True Peak (4x). Realtime-safe: histograms instead of growing vectors.
#include "Biquad.h"
#include "Dynamics.h"
#include <array>
#include <atomic>

namespace ubm
{
class LoudnessMeter
{
public:
    std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f }, lra { 0.0f }, truePeakDb { -100.0f };
    std::atomic<float> seconds { 0.0f };
    std::atomic<bool> resetRequest { false };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // K-weighting for any sample rate (same formulas as libebur128)
        double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        double K = std::tan (kPi * f0 / sr), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        double a0 = 1.0 + K / Q + K * K;
        Coeffs pre { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        f0 = 38.13547087602444; Q = 0.5003270373238773; K = std::tan (kPi * f0 / sr);
        Coeffs rlb { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / (1.0 + K / Q + K * K), (1.0 - K / Q + K * K) / (1.0 + K / Q + K * K) };
        for (int c = 0; c < 2; ++c) { kw[c][0].c = pre; kw[c][1].c = rlb; }
        subLen = (int) std::round (sr * 0.1);
        resetAll();
    }

    void resetAll()
    {
        for (auto& c : kw) for (auto& b : c) b.reset();
        tp.reset(); subAcc = 0; subN = 0; subCount = 0; ring.fill (0.0); tpMax = 0;
        histI.fill ({}); histS.fill (0);
        momentary = shortTerm = integrated = -100.0f; lra = 0.0f; truePeakDb = -100.0f; seconds = 0.0f;
    }

    void process (const float* L, const float* R, int n)
    {
        if (resetRequest.exchange (false)) resetAll();
        for (int i = 0; i < n; ++i)
        {
            tpMax = std::max (tpMax, tp.push (L[i], R[i]));
            const float l = kw[0][1].process (kw[0][0].process (L[i]));
            const float r = kw[1][1].process (kw[1][0].process (R[i]));
            subAcc += (double) l * l + (double) r * r;
            if (++subN == subLen) closeSubBlock();
        }
        truePeakDb.store (gainToDb (tpMax, -100.0f), std::memory_order_relaxed);
    }

    static float energyToLufs (double e) { return e > 1e-20 ? (float) (-0.691 + 10.0 * std::log10 (e)) : -100.0f; }

private:
    void closeSubBlock()
    {
        ring[(size_t) (subCount % 30)] = subAcc / subN;
        subAcc = 0; subN = 0; ++subCount;
        seconds.store ((float) subCount * 0.1f, std::memory_order_relaxed);

        if (subCount >= 4)
        {
            double e = 0; for (int k = 0; k < 4; ++k) e += ring[(size_t) ((subCount - 1 - k) % 30)];
            e /= 4.0;
            const float m = energyToLufs (e);
            momentary.store (m, std::memory_order_relaxed);
            if (m > -70.0f)   // absolute gate; store 400 ms block (75 % overlap) in 0.1 LU histogram
            {
                const int b = std::clamp ((int) std::floor ((m + 70.0f) * 10.0f), 0, kBins - 1);
                histI[(size_t) b].count++; histI[(size_t) b].energy += e;
            }
            integrated.store (computeIntegrated(), std::memory_order_relaxed);
        }
        else momentary.store (energyToLufs (subAcc), std::memory_order_relaxed);

        const int n = (int) std::min<long long> (subCount, 30);
        double e = 0; for (int k = 0; k < n; ++k) e += ring[(size_t) ((subCount - 1 - k) % 30)];
        e /= n;
        const float s = energyToLufs (e);
        shortTerm.store (s, std::memory_order_relaxed);
        if (subCount >= 30 && s > -70.0f)
        {
            const int b = std::clamp ((int) std::floor ((s + 70.0f) * 10.0f), 0, kBins - 1);
            histS[(size_t) b]++;
            lra.store (computeLRA(), std::memory_order_relaxed);
        }
    }

    float computeIntegrated() const
    {
        double e = 0; long long n = 0;
        for (auto& h : histI) { e += h.energy; n += h.count; }
        if (n == 0) return -100.0f;
        const float rel = energyToLufs (e / (double) n) - 10.0f;
        const int start = std::clamp ((int) std::floor ((rel + 70.0f) * 10.0f), 0, kBins - 1);
        e = 0; n = 0;
        for (int b = start; b < kBins; ++b) { e += histI[(size_t) b].energy; n += histI[(size_t) b].count; }
        return n > 0 ? energyToLufs (e / (double) n) : -100.0f;
    }

    float computeLRA() const
    {
        // relative gate -20 LU below the (power) mean of the short-term values
        double e = 0; long long n = 0;
        for (int b = 0; b < kBins; ++b) if (histS[(size_t) b]) { e += histS[(size_t) b] * std::pow (10.0, ((b + 0.5) / 10.0 - 70.0 + 0.691) / 10.0); n += histS[(size_t) b]; }
        if (n < 2) return 0.0f;
        const float gate = energyToLufs (e / (double) n) - 20.0f;
        const int start = std::clamp ((int) std::floor ((gate + 70.0f) * 10.0f), 0, kBins - 1);
        long long tot = 0; for (int b = start; b < kBins; ++b) tot += histS[(size_t) b];
        if (tot < 2) return 0.0f;
        auto perc = [&] (double p) {
            const long long want = (long long) std::ceil (p * (double) tot);
            long long acc = 0;
            for (int b = start; b < kBins; ++b) { acc += histS[(size_t) b]; if (acc >= want) return (b + 0.5f) / 10.0f - 70.0f; }
            return (kBins - 0.5f) / 10.0f - 70.0f;
        };
        return std::max (0.0f, perc (0.95) - perc (0.10));
    }

    static constexpr int kBins = 800;   // -70 .. +10 LUFS in 0.1 steps
    struct HB { long long count = 0; double energy = 0; };
    double sr = 48000.0;
    Biquad kw[2][2];
    TruePeak tp;
    float tpMax = 0;
    double subAcc = 0; int subN = 0, subLen = 4800; long long subCount = 0;
    std::array<double, 30> ring {};
    std::array<HB, kBins> histI {};
    std::array<long long, kBins> histS {};
};
}
