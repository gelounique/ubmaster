#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using Attr = juce::AudioParameterFloatAttributes;

namespace
{
juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return { lo, hi,
             [] (float a, float b, float t) { return a * std::pow (b / a, t); },
             [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
             [] (float a, float b, float v) { return juce::jlimit (a, b, v); } };
}
juce::String hz (float v) { return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String ((int) std::round (v)) + " Hz"; }
juce::String db (float v, int dec = 1) { return (v > 0.049f ? "+" : "") + juce::String (v, dec) + " dB"; }
juce::String pct (float v) { return juce::String ((int) std::round (v)) + " %"; }

Attr fmt (std::function<juce::String (float)> f, const char* label = "")
{
    return Attr().withLabel (label).withStringFromValueFunction ([f] (float v, int) { return f (v); });
}
}

juce::AudioProcessorValueTreeState::ParameterLayout UBMasterProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto F = [&] (const juce::String& id, const juce::String& name, juce::NormalisableRange<float> r, float def, Attr a) {
        p.push_back (std::make_unique<APF> (juce::ParameterID { id, 1 }, name, r, def, a)); };
    auto B = [&] (const juce::String& id, const juce::String& name, bool def) {
        p.push_back (std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def)); };
    auto C = [&] (const juce::String& id, const juce::String& name, juce::StringArray items, int def) {
        p.push_back (std::make_unique<APC> (juce::ParameterID { id, 1 }, name, items, def)); };

    juce::StringArray modes; for (auto& pl : platforms()) modes.add (pl.name);
    C (ids::mode, "Loudness Mode", modes, 0);
    F (ids::target, "Target LUFS", { -30.0f, -5.0f, 0.1f }, -14.0f, fmt ([] (float v) { return juce::String (v, 1) + " LUFS"; }, "LUFS"));

    // EQ
    B (ids::eqOn, "EQ On", true);
    C (ids::eqMode, "EQ Mode", { "Global", "Mid/Side", "Left/Right" }, 0);
    for (int b = 0; b < ubm::kEqBands; ++b)
    {
        const auto& s = ubm::bandSpecs()[(size_t) b];
        const juce::String n = juce::String ("EQ ") + juce::String (b + 1) + " ";
        B (ids::eqBand (b, "on"), n + "On", s.defOn);
        F (ids::eqBand (b, "freq"), n + "Freq", logRange (20.0f, 20000.0f), s.defFreq, fmt (hz, "Hz"));
        F (ids::eqBand (b, "gain"), n + "Gain", { -15.0f, 15.0f, 0.01f }, s.defGain, fmt ([] (float v) { return db (v); }, "dB"));
        F (ids::eqBand (b, "q"), n + "Q", logRange (0.2f, 10.0f), s.defQ, fmt ([] (float v) { return juce::String (v, 2); }));
        C (ids::eqBand (b, "tgt"), n + "Channel", { "Both", "Mid / Left", "Side / Right" }, 0);
    }
    // Tape
    B (ids::tapeOn, "Tape On", true);
    F (ids::tapeDrive, "Tape Drive", { 0.0f, 24.0f, 0.01f }, 4.0f, fmt ([] (float v) { return db (v); }, "dB"));
    F (ids::tapeLo, "Tape Range Low", logRange (20.0f, 5000.0f), 60.0f, fmt (hz, "Hz"));
    F (ids::tapeHi, "Tape Range High", logRange (200.0f, 20000.0f), 8000.0f, fmt (hz, "Hz"));
    F (ids::tapeChar, "Tape Character", { 0.0f, 100.0f, 0.1f }, 35.0f, fmt (pct, "%"));
    F (ids::tapeMix, "Tape Mix", { 0.0f, 100.0f, 0.1f }, 60.0f, fmt (pct, "%"));
    // Transient
    B (ids::trOn, "Transient On", true);
    C (ids::trMode, "Transient Mode", { "Cut", "Boost" }, 0);
    F (ids::trAmount, "Transient Amount", { 0.0f, 100.0f, 0.1f }, 30.0f, fmt (pct, "%"));
    F (ids::trSens, "Transient Sensitivity", { 0.0f, 100.0f, 0.1f }, 50.0f, fmt (pct, "%"));
    auto sp = juce::NormalisableRange<float> (5.0f, 250.0f, 0.1f); sp.setSkewForCentre (40.0f);
    F (ids::trSpeed, "Transient Speed", sp, 40.0f, fmt ([] (float v) { return juce::String (v, v < 10 ? 1 : 0) + " ms"; }, "ms"));
    F (ids::trLo, "Transient Range Low", logRange (20.0f, 20000.0f), 20.0f, fmt (hz, "Hz"));
    F (ids::trHi, "Transient Range High", logRange (20.0f, 20000.0f), 20000.0f, fmt (hz, "Hz"));
    F (ids::trMix, "Transient Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, fmt (pct, "%"));
    B (ids::trDelta, "Transient Delta", false);
    // Stereo
    B (ids::stOn, "Stereo On", true);
    F (ids::stLow, "Width Low", { 0.0f, 200.0f, 0.1f }, 100.0f, fmt (pct, "%"));
    F (ids::stMid, "Width Mid", { 0.0f, 200.0f, 0.1f }, 100.0f, fmt (pct, "%"));
    F (ids::stHigh, "Width High", { 0.0f, 200.0f, 0.1f }, 100.0f, fmt (pct, "%"));
    F (ids::stXLo, "Crossover Low/Mid", logRange (40.0f, 1000.0f), 180.0f, fmt (hz, "Hz"));
    F (ids::stXHi, "Crossover Mid/High", logRange (1000.0f, 12000.0f), 4000.0f, fmt (hz, "Hz"));
    // Exciter
    B (ids::exOn, "Exciter On", true);
    F (ids::exAmount, "Exciter Amount", { 0.0f, 100.0f, 0.1f }, 15.0f, fmt (pct, "%"));
    F (ids::exFreq, "Exciter Freq", logRange (1000.0f, 12000.0f), 4000.0f, fmt (hz, "Hz"));
    F (ids::exChar, "Exciter Character", { 0.0f, 100.0f, 0.1f }, 30.0f, fmt (pct, "%"));
    // Limiter
    B (ids::limOn, "Limiter On", true);
    F (ids::limGain, "Limiter Gain", { 0.0f, 24.0f, 0.01f }, 0.0f, fmt ([] (float v) { return db (v); }, "dB"));
    F (ids::limCeil, "Ceiling", { -12.0f, 0.0f, 0.01f }, -1.0f, fmt ([] (float v) { return juce::String (v, 1) + " dBTP"; }, "dBTP"));
    auto rl = juce::NormalisableRange<float> (1.0f, 1000.0f, 0.1f); rl.setSkewForCentre (100.0f);
    F (ids::limRelease, "Release", rl, 120.0f, fmt ([] (float v) { return juce::String ((int) std::round (v)) + " ms"; }, "ms"));
    return { p.begin(), p.end() };
}

UBMasterProcessor::UBMasterProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "UBMASTER", createLayout())
{
    auto P = [this] (const juce::String& id) { auto* r = apvts.getRawParameterValue (id); jassert (r != nullptr); return r; };
    raw.eqOn = P (ids::eqOn); raw.eqMode = P (ids::eqMode);
    const char* what[5] { "on", "freq", "gain", "q", "tgt" };
    for (int b = 0; b < ubm::kEqBands; ++b) for (int k = 0; k < 5; ++k) raw.band[b][k] = P (ids::eqBand (b, what[k]));
    raw.tapeOn = P (ids::tapeOn); raw.tapeDrive = P (ids::tapeDrive); raw.tapeLo = P (ids::tapeLo); raw.tapeHi = P (ids::tapeHi);
    raw.tapeChar = P (ids::tapeChar); raw.tapeMix = P (ids::tapeMix);
    raw.trOn = P (ids::trOn); raw.trMode = P (ids::trMode); raw.trAmount = P (ids::trAmount); raw.trSens = P (ids::trSens);
    raw.trSpeed = P (ids::trSpeed); raw.trLo = P (ids::trLo); raw.trHi = P (ids::trHi); raw.trMix = P (ids::trMix); raw.trDelta = P (ids::trDelta);
    raw.stOn = P (ids::stOn); raw.stLow = P (ids::stLow); raw.stMid = P (ids::stMid); raw.stHigh = P (ids::stHigh); raw.stXLo = P (ids::stXLo); raw.stXHi = P (ids::stXHi);
    raw.exOn = P (ids::exOn); raw.exAmount = P (ids::exAmount); raw.exFreq = P (ids::exFreq); raw.exChar = P (ids::exChar);
    raw.limOn = P (ids::limOn); raw.limGain = P (ids::limGain); raw.limCeil = P (ids::limCeil); raw.limRelease = P (ids::limRelease);
}

bool UBMasterProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void UBMasterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSR = sampleRate;
    maxBlock = 512;
    juce::ignoreUnused (samplesPerBlock);
    tmp.setSize (2, maxBlock);
    eq.prepare (sampleRate);
    tape.prepare (sampleRate, maxBlock);
    transient.prepare (sampleRate);
    stereo.prepare (sampleRate);
    exciter.prepare (sampleRate, maxBlock);
    limiter.prepare (sampleRate);
    meter.prepare (sampleRate);
    setLatencySamples (transient.getLatency() + limiter.getLatency());
}

void UBMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = buffer.getNumChannels(), n = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;
    for (int pos = 0; pos < n; pos += maxBlock)
    {
        const int len = std::min (maxBlock, n - pos);
        float* L = tmp.getWritePointer (0); float* R = tmp.getWritePointer (1);
        juce::FloatVectorOperations::copy (L, buffer.getReadPointer (0, pos), len);
        juce::FloatVectorOperations::copy (R, buffer.getReadPointer (numCh > 1 ? 1 : 0, pos), len);
        processStereo (L, R, len);
        if (numCh > 1)
        {
            juce::FloatVectorOperations::copy (buffer.getWritePointer (0, pos), L, len);
            juce::FloatVectorOperations::copy (buffer.getWritePointer (1, pos), R, len);
        }
        else
        {
            auto* o = buffer.getWritePointer (0, pos);
            for (int i = 0; i < len; ++i) o[i] = 0.5f * (L[i] + R[i]);
        }
    }
    for (int c = 2; c < numCh; ++c) buffer.clear (c, 0, n);
}

void UBMasterProcessor::processStereo (float* L, float* R, int n)
{
    float ip = 0; for (int i = 0; i < n; ++i) ip = std::max (ip, std::max (std::abs (L[i]), std::abs (R[i])));
    inPeakDb.store (ubm::gainToDb (ip, -100.0f), std::memory_order_relaxed);

    auto v = [] (std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    auto on = [&] (std::atomic<float>* a) { return v (a) > 0.5f; };
    // 1 — Master-EQ
    {
        const bool eqOn = on (raw.eqOn);   // off = every band glides to neutral (no click)
        std::array<ubm::BandParams, ubm::kEqBands> bp;
        for (int b = 0; b < ubm::kEqBands; ++b)
            bp[(size_t) b] = { eqOn && on (raw.band[b][0]), v (raw.band[b][1]), v (raw.band[b][2]), v (raw.band[b][3]), (int) v (raw.band[b][4]) };
        eq.process (L, R, n, (int) v (raw.eqMode), bp);
    }
    // 2 — Tape-Sim (frequency-selective)
    tape.process (L, R, n, on (raw.tapeOn), v (raw.tapeDrive), v (raw.tapeLo), v (raw.tapeHi), v (raw.tapeChar) * 0.01f, v (raw.tapeMix) * 0.01f);
    // 3 — Transient (spectral, adaptive) — always runs (constant latency), neutral when off
    ubm::TransientShaper::Params tp;
    tp.on = on (raw.trOn); tp.boost = v (raw.trMode) > 0.5f; tp.amount = v (raw.trAmount) * 0.01f;
    tp.sensitivity = v (raw.trSens) * 0.01f; tp.speedMs = v (raw.trSpeed); tp.lo = v (raw.trLo); tp.hi = std::max (v (raw.trHi), tp.lo * 1.1f);
    tp.mix = v (raw.trMix) * 0.01f; tp.delta = on (raw.trDelta);
    transient.process (L, R, n, tp);
    // 4 — Stereo
    stereo.process (L, R, n, on (raw.stOn), v (raw.stLow) * 0.01f, v (raw.stMid) * 0.01f, v (raw.stHigh) * 0.01f, v (raw.stXLo), v (raw.stXHi));
    // 5 — Exciter
    exciter.process (L, R, n, on (raw.exOn), v (raw.exAmount) * 0.01f, v (raw.exFreq), v (raw.exChar) * 0.01f);
    // 6 — Limiter
    limiter.process (L, R, n, on (raw.limOn), v (raw.limGain), v (raw.limCeil), v (raw.limRelease));
    // 7 — Loudness
    meter.process (L, R, n);

    // taps for UI
    int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        scopeL[(size_t) w].store (L[i], std::memory_order_relaxed);
        scopeR[(size_t) w].store (R[i], std::memory_order_relaxed);
        w = (w + 1) & (kScope - 1);
        corrLR = corrLR * 0.99995 + (double) L[i] * R[i];
        corrLL = corrLL * 0.99995 + (double) L[i] * L[i];
        corrRR = corrRR * 0.99995 + (double) R[i] * R[i];
    }
    scopeWrite.store (w, std::memory_order_release);
    const double den = std::sqrt (corrLL * corrRR);
    correlation.store (den > 1e-9 ? (float) (corrLR / den) : 1.0f, std::memory_order_relaxed);
}

void UBMasterProcessor::setParam (const juce::String& id, float plain)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

void UBMasterProcessor::selectPlatform (int index)
{
    index = juce::jlimit (0, (int) platforms().size() - 1, index);
    setParam (ids::mode, (float) index);
    if (index < (int) platforms().size() - 1)   // Custom keeps the user's target
    {
        setParam (ids::target, platforms()[(size_t) index].lufs);
        setParam (ids::limCeil, platforms()[(size_t) index].ceiling);
    }
}

float UBMasterProcessor::matchTargetLoudness()
{
    const bool haveI = meter.seconds.load() >= 3.0f && meter.integrated.load() > -70.0f;
    const float now = haveI ? meter.integrated.load() : meter.shortTerm.load();
    if (now <= -70.0f) return 0.0f;
    const float delta = get (ids::target) - now;
    const float g0 = get (ids::limGain);
    const float g1 = juce::jlimit (0.0f, 24.0f, g0 + delta);
    setParam (ids::limGain, g1);
    if (get (ids::limOn) < 0.5f) setParam (ids::limOn, 1.0f);
    meter.resetRequest = true;   // re-measure from here
    return g1 - g0;
}

void UBMasterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void UBMasterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* UBMasterProcessor::createEditor() { return new UBMasterEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new UBMasterProcessor(); }
