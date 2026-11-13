// UBMaster DSP tests: transparency, latency, true-peak limiting, BS.1770 accuracy, module behaviour, advice, match.
#include "PluginProcessor.h"
#include "Advice.h"
#include <cstdio>
#include <random>

static int fails = 0;
#define CHECK(cond, ...) do { bool ok_ = (cond); std::printf ("%s  ", ok_ ? "[ OK ]" : "[FAIL]"); std::printf (__VA_ARGS__); std::printf ("\n"); if (! ok_) ++fails; } while (0)

static void neutral (UBMasterProcessor& p)
{
    for (auto id : { ids::eqOn, ids::tapeOn, ids::trOn, ids::exOn, ids::limOn }) p.setParam (id, 0.0f);
    p.setParam (ids::stOn, 1.0f);
}

/** synthetic "music": kick every 0.5 s, bass, chords, hats, noise */
static void music (juce::AudioBuffer<float>& b, double sr, float level, uint32_t seed = 1)
{
    std::mt19937 rng (seed); std::normal_distribution<float> nd (0, 1);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / sr, tb = std::fmod (t, 0.5);
        const float kick = (float) (std::exp (-tb * 18) * std::sin (2 * M_PI * (50 + 120 * std::exp (-tb * 30)) * tb)) * 0.9f;
        const float bass = 0.35f * (float) std::sin (2 * M_PI * 55 * t);
        const float ch = 0.12f * (float) (std::sin (2 * M_PI * 220 * t) + std::sin (2 * M_PI * 277.2 * t) + std::sin (2 * M_PI * 329.6 * t));
        const double th = std::fmod (t, 0.125);
        const float hat = (float) std::exp (-th * 120) * nd (rng) * 0.15f;
        const float nz = nd (rng) * 0.02f;
        b.setSample (0, i, level * (kick + bass + ch * 1.1f + hat + nz));
        b.setSample (1, i, level * (kick + bass + ch * 0.9f - hat * 0.6f + nz));
    }
}

/** independent true-peak check: 8x FIR oversampling (JUCE equiripple), skip the first `skip` samples */
static float truePeak8x (juce::AudioBuffer<float>& b, int skip)
{
    juce::dsp::Oversampling<float> os (2, 3, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
    os.initProcessing ((size_t) b.getNumSamples());
    juce::dsp::AudioBlock<float> blk (b);
    auto up = os.processSamplesUp (blk);
    float m = 0; const int lat = (int) os.getLatencyInSamples() * 8 + 64;
    for (size_t c = 0; c < up.getNumChannels(); ++c) for (size_t i = (size_t) (skip * 8 + lat); i < up.getNumSamples(); ++i) m = std::max (m, std::abs (up.getSample ((int) c, (int) i)));
    return juce::Decibels::gainToDecibels (m);
}

static void run (UBMasterProcessor& p, juce::AudioBuffer<float>& b, int block)
{
    juce::MidiBuffer m;
    for (int pos = 0; pos < b.getNumSamples(); pos += block)
    {
        const int n = std::min (block, b.getNumSamples() - pos);
        juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), 2, pos, n);
        p.processBlock (v, m);
    }
}

static float rmsDb (const juce::AudioBuffer<float>& b, int ch, int s0, int n) { return juce::Decibels::gainToDecibels (b.getRMSLevel (ch, s0, n), -150.0f); }

static float sineGainDb (UBMasterProcessor& p, double sr, float hz, float amp = 0.1f)
{
    const int n = (int) sr * 2;
    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i) { const float v = amp * (float) std::sin (2 * M_PI * hz * i / sr); b.setSample (0, i, v); b.setSample (1, i, v); }
    run (p, b, 512);
    return rmsDb (b, 0, n / 2, n / 2) - juce::Decibels::gainToDecibels (amp / std::sqrt (2.0f));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    // ---------------------------------------------------------------- transparency + latency
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        UBMasterProcessor p; p.prepareToPlay (sr, 512); neutral (p);
        const int lat = p.getLatencySamples(), n = (int) sr;
        juce::AudioBuffer<float> in (2, n), b (2, n);
        music (in, sr, 0.3f);
        b.makeCopyOf (in);
        run (p, b, 333);
        float err = 0;
        for (int c = 0; c < 2; ++c) for (int i = lat + 2000; i < n; ++i) err = std::max (err, std::abs (b.getSample (c, i) - in.getSample (c, i - lat)));
        CHECK (err < 2e-4f, "%.1f kHz: neutral chain == input delayed by %d samples (%.2f ms), max err %.2e", sr / 1000, lat, lat * 1000 / sr, err);
        // impulse confirms the reported latency
        UBMasterProcessor q; q.prepareToPlay (sr, 512); neutral (q);
        juce::AudioBuffer<float> imp (2, 8192); imp.clear(); imp.setSample (0, 100, 1.0f); imp.setSample (1, 100, 1.0f);
        run (q, imp, 64);
        int at = -1; float mx = 0; for (int i = 0; i < 8192; ++i) if (std::abs (imp.getSample (0, i)) > mx) { mx = std::abs (imp.getSample (0, i)); at = i; }
        CHECK (at - 100 == lat, "%.1f kHz: impulse peak at +%d (reported %d)", sr / 1000, at - 100, lat);
    }

    // ---------------------------------------------------------------- loudness meter
    {
        UBMasterProcessor p; p.prepareToPlay (48000, 512); neutral (p);
        const int n = 48000 * 10;
        juce::AudioBuffer<float> b (2, n);
        const float a = juce::Decibels::decibelsToGain (-20.0f);
        for (int i = 0; i < n; ++i) { const float v = a * (float) std::sin (2 * M_PI * 1000 * i / 48000.0); b.setSample (0, i, v); b.setSample (1, i, v); }
        run (p, b, 480);
        CHECK (std::abs (p.meter.integrated.load() + 20.0f) < 0.1f, "1 kHz sine -20 dBFS stereo -> integrated %.2f LUFS (expect -20.0)", p.meter.integrated.load());
        CHECK (std::abs (p.meter.shortTerm.load() + 20.0f) < 0.1f, "short-term %.2f LUFS", p.meter.shortTerm.load());
        CHECK (std::abs (p.meter.truePeakDb.load() + 20.0f) < 0.2f, "true peak %.2f dBTP (expect -20)", p.meter.truePeakDb.load());
        CHECK (p.meter.lra.load() < 0.3f, "LRA of a steady sine %.2f LU", p.meter.lra.load());
        // EBU Tech 3341 style: alternating -26 / -36 (+ silence gated) -> LRA ~ 10
        UBMasterProcessor q; q.prepareToPlay (48000, 512); neutral (q);
        juce::AudioBuffer<float> c (2, 48000 * 40);
        for (int i = 0; i < c.getNumSamples(); ++i)
        {
            const float lv = (i / (48000 * 20)) == 0 ? -20.0f : -30.0f;
            const float v = juce::Decibels::decibelsToGain (lv) * (float) std::sin (2 * M_PI * 1000 * i / 48000.0);
            c.setSample (0, i, v); c.setSample (1, i, v);
        }
        run (q, c, 512);
        CHECK (std::abs (q.meter.lra.load() - 10.0f) < 0.6f, "LRA 20 s @-20 + 20 s @-30 -> %.2f LU (expect ~10)", q.meter.lra.load());
        CHECK (std::abs (q.meter.integrated.load() + 22.6f) < 0.5f, "gated integrated %.2f LUFS (expect ~ -22.6)", q.meter.integrated.load());
    }

    // ---------------------------------------------------------------- true-peak estimator vs analytic sine peaks
    for (double f : { 1000.0, 9000.0, 15000.0, 19000.0 })
    {
        const int N = 44100; juce::AudioBuffer<float> b (2, N);
        for (int i = 0; i < N; ++i) { const float v = 0.5f * (float) std::sin (2 * M_PI * f * i / 44100.0 + 0.3); b.setSample (0, i, v); b.setSample (1, i, v); }
        ubm::TruePeak tp; float m = 0; for (int i = 0; i < N; ++i) { const float v = tp.push (b.getSample (0, i), b.getSample (1, i)); if (i > 2000) m = std::max (m, v); }
        const float ref = truePeak8x (b, 2000), sp = juce::Decibels::gainToDecibels (b.getMagnitude (0, 0, N)), mine = juce::Decibels::gainToDecibels (m);
        CHECK (std::abs (mine + 6.02f) < 0.15f, "true-peak %5.0f Hz sine (exact -6.02): UBMaster %.2f · ref 8x FIR %.2f · sample peak %.2f dBFS", f, mine, ref, sp);
    }

    // ---------------------------------------------------------------- limiter: true-peak ceiling holds
    for (float gain : { 6.0f, 12.0f, 20.0f })
    {
        UBMasterProcessor p; p.prepareToPlay (44100, 512); neutral (p);
        p.setParam (ids::limOn, 1); p.setParam (ids::limGain, gain); p.setParam (ids::limCeil, -1.0f); p.setParam (ids::limRelease, 60.0f);
        juce::AudioBuffer<float> b (2, 44100 * 8); music (b, 44100, 0.5f, 3);
        run (p, b, 256);
        p.meter.resetRequest = true;
        juce::AudioBuffer<float> b2 (2, 44100 * 4); music (b2, 44100, 0.5f, 4);
        run (p, b2, 97);
        const float tp8 = truePeak8x (b2, 2000);
        CHECK (tp8 <= -0.95f, "limiter +%.0f dB: independent 8x true peak %.2f dBTP", gain, tp8);
        CHECK (p.meter.truePeakDb.load() <= -0.95f, "limiter +%.0f dB: true peak %.2f dBTP <= -1.0 (+0.05) · GR %.1f dB · %.1f LUFS",
               gain, p.meter.truePeakDb.load(), p.limiter.grDb.load(), p.meter.integrated.load());
    }

    // ---------------------------------------------------------------- EQ
    {
        UBMasterProcessor p; p.prepareToPlay (48000, 512); neutral (p); p.setParam (ids::eqOn, 1);
        p.setParam (ids::eqBand (3, "freq"), 1000); p.setParam (ids::eqBand (3, "gain"), 6.0f); p.setParam (ids::eqBand (3, "q"), 1.0f);
        const float g = sineGainDb (p, 48000, 1000);
        CHECK (std::abs (g - 6.0f) < 0.15f, "EQ bell +6 dB @1k -> %.2f dB", g);
        UBMasterProcessor q; q.prepareToPlay (48000, 512); neutral (q); q.setParam (ids::eqOn, 1);
        q.setParam (ids::eqBand (0, "freq"), 120);
        const float lc = sineGainDb (q, 48000, 30);
        CHECK (lc < -20.0f, "Low cut 120 Hz (24 dB/oct): 30 Hz at %.1f dB", lc);
        // Mid/Side: boost only side -> mono signal unchanged
        UBMasterProcessor m; m.prepareToPlay (48000, 512); neutral (m); m.setParam (ids::eqOn, 1);
        m.setParam (ids::eqMode, 1); m.setParam (ids::eqBand (3, "gain"), 9.0f); m.setParam (ids::eqBand (3, "tgt"), 2);
        const float ms = sineGainDb (m, 48000, 2800);
        CHECK (std::abs (ms) < 0.05f, "M/S EQ: +9 dB on SIDE leaves a mono signal untouched (%.3f dB)", ms);
    }

    // ---------------------------------------------------------------- Tape: saturation only in the chosen range
    {
        auto thd = [] (float lo, float hi) {
            UBMasterProcessor p; p.prepareToPlay (48000, 512); neutral (p);
            p.setParam (ids::tapeOn, 1); p.setParam (ids::tapeDrive, 18.0f); p.setParam (ids::tapeMix, 100.0f); p.setParam (ids::tapeLo, lo); p.setParam (ids::tapeHi, hi);
            const int n = 48000;
            juce::AudioBuffer<float> b (2, n);
            for (int i = 0; i < n; ++i) { const float v = 0.5f * (float) std::sin (2 * M_PI * 100 * i / 48000.0); b.setSample (0, i, v); b.setSample (1, i, v); }
            run (p, b, 512);
            // remove fundamental by projection -> residual energy
            double s = 0, c = 0, e = 0; const int s0 = n / 2;
            for (int i = s0; i < n; ++i) { s += b.getSample (0, i) * std::sin (2 * M_PI * 100 * i / 48000.0); c += b.getSample (0, i) * std::cos (2 * M_PI * 100 * i / 48000.0); }
            s *= 2.0 / (n - s0); c *= 2.0 / (n - s0);
            for (int i = s0; i < n; ++i) { const double r = b.getSample (0, i) - s * std::sin (2 * M_PI * 100 * i / 48000.0) - c * std::cos (2 * M_PI * 100 * i / 48000.0); e += r * r; }
            return (float) (10 * std::log10 (e / (n - s0) / (0.5 * (s * s + c * c))));
        };
        const float inBand = thd (40, 400), outBand = thd (2000, 12000);
        CHECK (inBand > -30.0f && outBand < inBand - 30.0f, "Tape on 100 Hz: range 40-400 Hz THD %.1f dB vs range 2-12 kHz THD %.1f dB", inBand, outBand);
    }

    // ---------------------------------------------------------------- Transient: cut lowers attacks, keeps sustain
    for (int boost = 0; boost < 2; ++boost)
    {
        auto drums = [] (juce::AudioBuffer<float>& b) {
            std::mt19937 r (5); std::normal_distribution<float> nd (0, 1);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double t = std::fmod (i / 48000.0, 0.25);
                const float hit = (float) std::exp (-t * 60) * nd (r) * 0.6f;
                const float pad = 0.15f * (float) std::sin (2 * M_PI * 330 * i / 48000.0);
                b.setSample (0, i, hit + pad); b.setSample (1, i, hit + pad);
            }
        };
        UBMasterProcessor a; a.prepareToPlay (48000, 512); neutral (a);
        UBMasterProcessor p; p.prepareToPlay (48000, 512); neutral (p);
        p.setParam (ids::trOn, 1); p.setParam (ids::trMode, (float) boost); p.setParam (ids::trAmount, 80.0f); p.setParam (ids::trSens, 60.0f);
        juce::AudioBuffer<float> b0 (2, 48000 * 3), b1 (2, 48000 * 3); drums (b0); b1.makeCopyOf (b0);
        run (a, b0, 512); run (p, b1, 512);
        const int s0 = 48000;
        auto attackRms = [&] (juce::AudioBuffer<float>& b, bool attack) {
            double e = 0; int n = 0; const int lat = a.getLatencySamples();
            for (int i = s0; i < b.getNumSamples(); ++i)
            {
                const double t = std::fmod ((i - lat) / 48000.0, 0.25);
                if ((t < 0.02) == attack) { e += b.getSample (0, i) * b.getSample (0, i); ++n; }
            }
            return 10 * std::log10 (e / n);
        };
        const double dA = attackRms (b1, true) - attackRms (b0, true), dS = attackRms (b1, false) - attackRms (b0, false);
        if (boost == 0) CHECK (dA < -2.0 && std::abs (dS) < 1.5, "Transient CUT 80%%: attacks %.1f dB, sustain %.2f dB", dA, dS);
        else CHECK (dA > 1.5 && std::abs (dS) < 1.5, "Transient BOOST 80%%: attacks +%.1f dB, sustain %.2f dB", dA, dS);
    }

    // ---------------------------------------------------------------- Stereo width
    {
        UBMasterProcessor p; p.prepareToPlay (48000, 512); neutral (p);
        p.setParam (ids::stLow, 0); p.setParam (ids::stMid, 0); p.setParam (ids::stHigh, 0);
        juce::AudioBuffer<float> b (2, 48000); music (b, 48000, 0.3f);
        run (p, b, 512);
        double s = 0, m = 0; for (int i = 24000; i < 48000; ++i) { const double d = b.getSample (0, i) - b.getSample (1, i), a = b.getSample (0, i) + b.getSample (1, i); s += d * d; m += a * a; }
        CHECK (10 * std::log10 (s / m) < -60, "Width 0/0/0 %% -> side %.0f dB below mid", 10 * std::log10 (s / m));
    }

    // ---------------------------------------------------------------- Exciter adds highs
    {
        UBMasterProcessor a; a.prepareToPlay (48000, 512); neutral (a);
        UBMasterProcessor p; p.prepareToPlay (48000, 512); neutral (p);
        p.setParam (ids::exOn, 1); p.setParam (ids::exAmount, 60.0f); p.setParam (ids::exFreq, 3000.0f);
        juce::AudioBuffer<float> b0 (2, 48000), b1 (2, 48000);
        for (int i = 0; i < 48000; ++i) { const float v = 0.3f * (float) std::sin (2 * M_PI * 3500 * i / 48000.0); for (int c = 0; c < 2; ++c) { b0.setSample (c, i, v); b1.setSample (c, i, v); } }
        run (a, b0, 512); run (p, b1, 512);
        auto thd = [] (juce::AudioBuffer<float>& b) {
            double sn = 0, cs = 0, e = 0; const int s0 = 24000, n = 48000; const double w = 2 * M_PI * 3500 / 48000.0;
            for (int i = s0; i < n; ++i) { sn += b.getSample (0, i) * std::sin (w * i); cs += b.getSample (0, i) * std::cos (w * i); }
            sn *= 2.0 / (n - s0); cs *= 2.0 / (n - s0);
            for (int i = s0; i < n; ++i) { const double r = b.getSample (0, i) - sn * std::sin (w * i) - cs * std::cos (w * i); e += r * r; }
            return 10 * std::log10 (e / (n - s0) / (0.5 * (sn * sn + cs * cs)) + 1e-30); };
        const double off = thd (b0), on = thd (b1);
        CHECK (on > -30 && on < -6 && off < -90, "Exciter 60%% on 3.5 kHz sine: harmonics %.1f dB (off: %.0f dB)", on, off);
    }

    // ---------------------------------------------------------------- match to target + mono + odd blocks
    {
        UBMasterProcessor p; p.prepareToPlay (44100, 512);
        p.selectPlatform (0);
        juce::AudioBuffer<float> b (2, 44100 * 8); music (b, 44100, 0.12f);
        run (p, b, 512);
        const float before = p.meter.integrated.load();
        const float d = p.matchTargetLoudness();
        juce::AudioBuffer<float> b2 (2, 44100 * 8); music (b2, 44100, 0.12f, 9);
        run (p, b2, 1023);
        const float after = p.meter.integrated.load();
        CHECK (std::abs (after + 14.0f) < 1.0f, "Auf Ziel (Spotify): %.1f LUFS -> gain %+.1f dB -> %.1f LUFS, TP %.2f dBTP", before, d, after, p.meter.truePeakDb.load());
        CHECK (p.meter.truePeakDb.load() <= -0.95f, "...and true peak stays <= -1 dBTP (%.2f)", p.meter.truePeakDb.load());
    }
    {
        UBMasterProcessor p;
        juce::AudioProcessor::BusesLayout mono; mono.inputBuses.add (juce::AudioChannelSet::mono()); mono.outputBuses.add (juce::AudioChannelSet::mono());
        CHECK (p.setBusesLayout (mono), "mono layout accepted");
        p.prepareToPlay (48000, 300);
        juce::AudioBuffer<float> b (1, 48000); juce::MidiBuffer mm;
        for (int i = 0; i < 48000; ++i) b.setSample (0, i, 0.5f * (float) std::sin (i * 0.05));
        for (int pos = 0; pos < 48000; pos += 300) { juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), 1, pos, 300); p.processBlock (v, mm); }
        bool finite = true; for (int i = 0; i < 48000; ++i) finite &= std::isfinite (b.getSample (0, i));
        CHECK (finite && b.getMagnitude (0, 0, 48000) > 0.1f, "mono processing finite (peak %.2f)", b.getMagnitude (0, 0, 48000));
    }

    // ---------------------------------------------------------------- state round-trip
    {
        UBMasterProcessor p; p.setParam (ids::tapeDrive, 11.0f); p.setParam (ids::eqMode, 2);
        juce::MemoryBlock mb; p.getStateInformation (mb);
        UBMasterProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (std::abs (q.get (ids::tapeDrive) - 11.0f) < 0.01f && (int) q.get (ids::eqMode) == 2, "state save/load");
    }

    // ---------------------------------------------------------------- advice
    {
        ubm::AdviceInput in; in.platform = "Spotify"; in.spotify = true; in.target = -14; in.ceiling = -1;
        auto a = ubm::makeAdvice (in);
        CHECK (! a.hasSignal, "advice without signal: %s", a.headline.toRawUTF8());
        in.integrated = -19.3f; in.shortTerm = -18.0f; in.seconds = 30; in.truePeak = -3; in.grDb = -1;
        a = ubm::makeAdvice (in);
        CHECK (std::abs (a.delta - 5.3f) < 0.01f && a.headline.contains ("5.3"), "too quiet: %s | %s", a.headline.toRawUTF8(), a.tips[0].toRawUTF8());
        in.integrated = -9.0f; in.truePeak = -0.4f;
        a = ubm::makeAdvice (in);
        CHECK (a.delta < -4.9f && a.tips.size() >= 2, "too loud: %s | %s | %s", a.headline.toRawUTF8(), a.tips[0].toRawUTF8(), a.tips[1].toRawUTF8());
        in.integrated = -14.2f; in.truePeak = -1.3f;
        a = ubm::makeAdvice (in);
        CHECK (a.onTarget, "on target: %s", a.headline.toRawUTF8());
    }

    // ---------------------------------------------------------------- CPU (realtime factor, all modules on)
    {
        UBMasterProcessor p; p.prepareToPlay (48000, 512);
        p.setParam (ids::limGain, 8.0f); p.setParam (ids::eqBand (2, "gain"), 2.0f);
        juce::AudioBuffer<float> b (2, 48000 * 20); music (b, 48000, 0.3f);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        run (p, b, 512);
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        CHECK (ms < 20000 * 0.25, "CPU: 20 s of stereo audio @48k in %.0f ms -> %.1f %% of one core", ms, ms / 200.0);
    }

    std::printf ("\n%s (%d failure%s)\n", fails == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", fails, fails == 1 ? "" : "s");
    return fails == 0 ? 0 : 1;
}
