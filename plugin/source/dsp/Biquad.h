#pragma once
// UBMaster — Biquad (RBJ cookbook, TDF-II) + magnitude evaluation for the UI curves.
#include <cmath>
#include <complex>
#include <algorithm>

namespace ubm
{
constexpr double kPi = 3.14159265358979323846;

struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

inline Coeffs norm (double b0, double b1, double b2, double a0, double a1, double a2)
{
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

inline double clampF (double f, double sr) { return std::clamp (f, 5.0, sr * 0.49); }

inline Coeffs peak (double sr, double f, double q, double gDb)
{
    const double A = std::pow (10.0, gDb / 40.0), w = 2 * kPi * clampF (f, sr) / sr, al = std::sin (w) / (2 * q), c = std::cos (w);
    return norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
}
inline Coeffs lowShelf (double sr, double f, double q, double gDb)
{
    const double A = std::pow (10.0, gDb / 40.0), w = 2 * kPi * clampF (f, sr) / sr, al = std::sin (w) / (2 * q), c = std::cos (w), s = 2 * std::sqrt (A) * al;
    return norm (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
                 (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s);
}
inline Coeffs highShelf (double sr, double f, double q, double gDb)
{
    const double A = std::pow (10.0, gDb / 40.0), w = 2 * kPi * clampF (f, sr) / sr, al = std::sin (w) / (2 * q), c = std::cos (w), s = 2 * std::sqrt (A) * al;
    return norm (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
                 (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s);
}
inline Coeffs highpass (double sr, double f, double q)
{
    const double w = 2 * kPi * clampF (f, sr) / sr, al = std::sin (w) / (2 * q), c = std::cos (w);
    return norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
}
inline Coeffs lowpass (double sr, double f, double q)
{
    const double w = 2 * kPi * clampF (f, sr) / sr, al = std::sin (w) / (2 * q), c = std::cos (w);
    return norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
}

/** |H(e^jw)| in dB */
inline double magDb (const Coeffs& k, double f, double sr)
{
    const double w = 2 * kPi * f / sr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
    const auto h = (k.b0 + k.b1 * z1 + k.b2 * z2) / (1.0 + k.a1 * z1 + k.a2 * z2);
    return 20.0 * std::log10 (std::max (1e-12, std::abs (h)));
}

struct Biquad
{
    Coeffs c;
    double z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process (float xin)
    {
        const double x = xin, y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return (float) y;
    }
};

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g, float floorDb = -120.0f) { return g > 1e-9f ? std::max (floorDb, 20.0f * std::log10 (g)) : floorDb; }
inline float onePoleCoef (double ms, double sr) { return (float) (1.0 - std::exp (-1.0 / std::max (1e-6, ms * 0.001 * sr))); }
}
