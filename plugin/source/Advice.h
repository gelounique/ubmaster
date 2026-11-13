#pragma once
// UBMaster — concrete recommendations to hit the loudness target (pure logic, unit-tested in tests/dsp_test.cpp)
#include <juce_core/juce_core.h>
#include <cmath>

#ifndef U8
 #define U8(s) juce::String (juce::CharPointer_UTF8 (s))
#endif

namespace ubm
{
struct AdviceInput
{
    juce::String platform; bool custom = false; bool spotify = false;
    float target = -14, integrated = -100, shortTerm = -100, seconds = 0, truePeak = -100, ceiling = -1, lra = 0, grDb = 0, limGain = 0;
    bool limOn = true;
};

struct Advice
{
    juce::String headline;
    juce::StringArray tips;
    float delta = 0;          // dB to add to the limiter gain to reach the target
    bool onTarget = false, hasSignal = false;
};

inline juce::String fmtDb (float v, int dec = 1) { return (v > 0.049f ? "+" : (v < -0.049f ? U8 ("−") : "")) + juce::String (std::abs (v), dec); }

inline Advice makeAdvice (const AdviceInput& in)
{
    Advice a;
    if (in.shortTerm <= -70.0f && in.integrated <= -70.0f)
    {
        a.headline = U8 ("Spiel deinen Track ab – UBMaster misst live mit.");
        a.tips.add (U8 ("Für einen sauberen Integrated-Wert den ganzen Song (oder den lautesten Teil) laufen lassen."));
        a.tips.add ("Ziel: " + juce::String (in.target, 1) + U8 (" LUFS · True Peak max. ") + juce::String (in.ceiling, 1) + " dBTP.");
        return a;
    }
    a.hasSignal = true;
    const bool useI = in.seconds >= 3.0f && in.integrated > -70.0f;
    const float level = useI ? in.integrated : in.shortTerm;
    const float diff = level - in.target;
    a.delta = -diff;
    const juce::String lvl = juce::String (level, 1) + " LUFS" + (useI ? "" : " (Short-term)");

    if (std::abs (diff) <= 0.5f)
    {
        a.onTarget = true;
        a.headline = juce::String::fromUTF8 ("Ziel erreicht – ") + lvl + juce::String::fromUTF8 (" ✓");
        a.tips.add (U8 ("Passt für ") + in.platform + ". Jetzt nur noch mit Ohren feintunen, nicht mehr mit dem Gain.");
    }
    else if (diff < 0)
    {
        const float need = -diff;
        a.headline = "Noch " + juce::String (need, 1) + " dB zu leise (" + lvl + ")";
        if (! in.limOn) a.tips.add ("Limiter einschalten, dann Gain um +" + juce::String (need, 1) + juce::String::fromUTF8 (" dB erhöhen."));
        else a.tips.add ("Limiter-Gain um +" + juce::String (need, 1) + " dB auf " + juce::String (std::min (24.0f, in.limGain + need), 1)
                         + juce::String::fromUTF8 (" dB stellen – oder „Auf Ziel“ klicken."));
        if (-in.grDb + need > 6.0f)
            a.tips.add (juce::String::fromUTF8 ("Über ~6 dB Gain Reduction wird’s gequetscht: hol 1–2 dB über Tape-Drive oder Exciter, "
                                                "oder zähm Bass-Peaks mit dem Low Shelf."));
    }
    else
    {
        a.headline = juce::String (diff, 1) + " dB lauter als das Ziel (" + lvl + ")";
        if (in.custom) a.tips.add ("Limiter-Gain um " + juce::String (diff, 1) + " dB senken, um genau auf " + juce::String (in.target, 1) + " LUFS zu kommen.");
        else a.tips.add (in.platform + " dreht dich um " + juce::String (diff, 1) + " dB runter. Gain um " + juce::String (diff, 1)
                         + juce::String::fromUTF8 (" dB senken – gleiche Lautheit beim Hörer, aber mehr Punch."));
    }

    // true peak
    if (in.truePeak > in.ceiling + 0.1f && in.truePeak > -60.0f)
        a.tips.add ("True Peak " + juce::String (in.truePeak, 1) + U8 (" dBTP liegt über ") + juce::String (in.ceiling, 1)
                    + (in.limOn ? juce::String::fromUTF8 (" dBTP – Ceiling prüfen bzw. Messung zurücksetzen.") : juce::String (" dBTP – Limiter einschalten.")));
    else if (in.ceiling > -1.0f && ! in.custom)
        a.tips.add (juce::String::fromUTF8 ("Ceiling auf −1,0 dBTP stellen – sonst können beim Encoding (AAC/Ogg) Übersteuerungen entstehen."));
    if (in.spotify && level > -14.0f && in.ceiling > -2.0f)
        a.tips.add (juce::String::fromUTF8 ("Spotify: Master lauter als −14 LUFS → Ceiling −2,0 dBTP empfohlen."));

    // dynamics
    if (-in.grDb > 8.0f)
        a.tips.add (juce::String::fromUTF8 ("Limiter arbeitet hart (") + fmtDb (in.grDb) + juce::String::fromUTF8 (" dB GR): Release auf 150–300 ms oder Gain runter."));
    if (in.seconds >= 10.0f && in.lra > 0.0f && in.lra < 3.0f)
        a.tips.add ("LRA nur " + juce::String (in.lra, 1) + U8 (" LU – sehr komprimiert. Weniger Gain oder Transient auf BOOST für mehr Punch."));

    while (a.tips.size() > 3) a.tips.remove (a.tips.size() - 1);
    return a;
}
}
