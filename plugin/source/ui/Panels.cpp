#include "Panels.h"

using namespace theme;

// ================================================================================================ base
ModulePanel::ModulePanel (UBMasterProcessor& p, Icon ic, juce::String t, juce::String d, const juce::String& powerId)
    : proc (p), power (p, powerId), icon (ic), title (std::move (t)), desc (std::move (d))
{
    addAndMakeVisible (power);
    power.onChange = [this] { for (auto* k : knobs) { k->slider.getProperties().set ("ubDim", ! power.isOn()); k->repaint(); } };
}

Knob& ModulePanel::addKnob (const juce::String& id, const juce::String& label)
{
    auto* k = knobs.add (new Knob (proc, id, label));
    addAndMakeVisible (k);
    k->slider.getProperties().set ("ubDim", ! power.isOn());
    return *k;
}

void ModulePanel::layoutKnobs (juce::Rectangle<int> row, int maxW)
{
    if (knobs.isEmpty()) return;
    const int w = std::min (maxW, row.getWidth() / knobs.size());
    auto r = row.withSizeKeepingCentre (w * knobs.size(), row.getHeight());
    for (auto* k : knobs) k->setBounds (r.removeFromLeft (w));
}

void ModulePanel::paint (juce::Graphics& g)
{
    card (g, getLocalBounds().toFloat());
    auto ib = juce::Rectangle<float> (22, 20, 26, 26);
    auto path = theme::icon (icon, ib);
    if (power.isOn()) glowPath (g, path, 1.7f, tiffany, 3.0f, 0.45f);
    else { g.setColour (dim); g.strokePath (path, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
    g.setColour (text); g.setFont (sans (19.0f, 700));
    g.drawText (title, 60, 14, 300, 22, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (sans (12.5f));
    g.drawText (desc, 60, 35, getWidth() - 200, 16, juce::Justification::centredLeft);
}

void ModulePanel::resized()
{
    power.setBounds (getWidth() - 62, 20, 40, 26);
    layoutBody (getLocalBounds().reduced (20, 0).withTrimmedTop (64).withTrimmedBottom (16));
}

// ================================================================================================ RangeView
RangeView::RangeView (UBMasterProcessor& p, Analyzer& a, const juce::String& loId, const juce::String& hiId, juce::String cap)
    : proc (p), an (a), caption (std::move (cap))
{
    aLo = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter (loId), [this] (float v) { lo = v; repaint(); });
    aHi = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter (hiId), [this] (float v) { hi = v; repaint(); });
    aLo->sendInitialUpdate(); aHi->sendInitialUpdate();
}

void RangeView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (b, 16.0f);
    const auto ax = axis();
    ax.drawGrid (g);
    if (showSpectrum && an.hasSignal())
    {
        juce::Path sp; sp.startNewSubPath (ax.r.getX(), ax.r.getBottom());
        for (float x = ax.r.getX(); x <= ax.r.getRight(); x += 3.0f)
            sp.lineTo (x, ax.r.getBottom() - ax.r.getHeight() * 0.9f * an.levelAt (ax.hz (x)));
        sp.lineTo (ax.r.getRight(), ax.r.getBottom()); sp.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillPath (sp);
    }
    const float x0 = ax.x (lo), x1 = ax.x (std::max (hi, lo));
    auto sel = juce::Rectangle<float> (x0, ax.r.getY(), x1 - x0, ax.r.getHeight());
    juce::ColourGradient gr (tiffany.withAlpha (0.20f), 0, sel.getY(), tiffany.withAlpha (0.04f), 0, sel.getBottom(), false);
    g.setGradientFill (gr); g.fillRect (sel);
    for (float x : { x0, x1 })
    {
        juce::Path l; l.startNewSubPath (x, ax.r.getY()); l.lineTo (x, ax.r.getBottom());
        glowPath (g, l, 1.6f, tiffany, 3.0f, 0.4f);
        g.setColour (tiffany); g.fillRoundedRectangle (x - 4, ax.r.getCentreY() - 14, 8, 28, 4);
    }
    if (overlay) overlay (g, ax);
    g.setColour (text); g.setFont (mono (11.0f, true));
    auto lbl = [] (float v) { return v >= 1000 ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String ((int) std::round (v)) + " Hz"; };
    g.drawText (caption + "  " + lbl (lo) + juce::String::fromUTF8 (" – ") + lbl (hi), b.reduced (14, 10).removeFromTop (14), juce::Justification::left);
}

void RangeView::mouseMove (const juce::MouseEvent& e)
{
    const auto ax = axis();
    const float d0 = std::abs (e.position.x - ax.x (lo)), d1 = std::abs (e.position.x - ax.x (hi));
    setMouseCursor (std::min (d0, d1) < 10 ? juce::MouseCursor::LeftRightResizeCursor
                    : (e.position.x > ax.x (lo) && e.position.x < ax.x (hi) ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor));
}

void RangeView::mouseDown (const juce::MouseEvent& e)
{
    const auto ax = axis();
    const float d0 = std::abs (e.position.x - ax.x (lo)), d1 = std::abs (e.position.x - ax.x (hi));
    if (std::min (d0, d1) < 12) drag = d0 <= d1 ? 1 : 2;
    else if (e.position.x > ax.x (lo) && e.position.x < ax.x (hi)) drag = 3;
    else drag = e.position.x < ax.x (lo) ? 1 : 2;
    dragHz0 = ax.hz (e.position.x); lo0 = lo; hi0 = hi;
    if (drag & 1) aLo->beginGesture();
    if (drag & 2) aHi->beginGesture();
    mouseDrag (e);
}

void RangeView::mouseDrag (const juce::MouseEvent& e)
{
    const auto ax = axis();
    const float f = ax.hz (e.position.x);
    if (drag == 1) aLo->setValueAsPartOfGesture (std::min (f, hi / 1.25f));
    else if (drag == 2) aHi->setValueAsPartOfGesture (std::max (f, lo * 1.25f));
    else if (drag == 3)
    {
        float k = f / dragHz0;
        k = juce::jlimit (20.0f / lo0, 20000.0f / hi0, k);
        aLo->setValueAsPartOfGesture (lo0 * k); aHi->setValueAsPartOfGesture (hi0 * k);
    }
}

void RangeView::mouseUp (const juce::MouseEvent&)
{
    if (drag & 1) aLo->endGesture();
    if (drag & 2) aHi->endGesture();
    drag = 0;
}

// ================================================================================================ EQ graph
EqGraph::EqGraph (UBMasterProcessor& p, Analyzer& a) : proc (p), an (a)
{
    for (auto* prm : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID().startsWith ("eq")) p.apvts.addParameterListener (r->getParameterID(), this);
}
EqGraph::~EqGraph()
{
    for (auto* prm : proc.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID().startsWith ("eq")) proc.apvts.removeParameterListener (r->getParameterID(), this);
}

float EqGraph::yFor (float db) const { auto r = axis().r; return r.getCentreY() - db / 18.0f * r.getHeight() * 0.5f; }
float EqGraph::dbFor (float y) const { auto r = axis().r; return (r.getCentreY() - y) / (r.getHeight() * 0.5f) * 18.0f; }

juce::Point<float> EqGraph::nodePos (int b) const
{
    const auto t = ubm::bandSpecs()[(size_t) b].type;
    const float f = proc.get (ids::eqBand (b, "freq"));
    const bool cut = t == ubm::BandType::LowCut || t == ubm::BandType::HighCut;
    return { axis().x (f), cut ? yFor (0.0f) : yFor (proc.get (ids::eqBand (b, "gain"))) };
}

int EqGraph::nodeAt (juce::Point<float> p) const
{
    int best = -1; float bd = 16.0f;
    for (int b = 0; b < ubm::kEqBands; ++b) { const float d = nodePos (b).getDistanceFrom (p); if (d < bd) { bd = d; best = b; } }
    return best;
}

void EqGraph::rebuild()
{
    const auto ax = axis();
    const double sr = 48000.0;
    const int mode = (int) proc.get (ids::eqMode);
    const bool eqOn = proc.get (ids::eqOn) > 0.5f;
    curveA.clear(); curveB.clear();
    std::array<ubm::Coeffs, ubm::kEqBands * 2> cs; std::array<int, ubm::kEqBands> tg {}; std::array<bool, ubm::kEqBands> on {};
    for (int b = 0; b < ubm::kEqBands; ++b)
    {
        on[(size_t) b] = eqOn && proc.get (ids::eqBand (b, "on")) > 0.5f;
        tg[(size_t) b] = (int) proc.get (ids::eqBand (b, "tgt"));
        ubm::bandCoeffs (ubm::bandSpecs()[(size_t) b].type, sr, proc.get (ids::eqBand (b, "freq")), proc.get (ids::eqBand (b, "gain")),
                         proc.get (ids::eqBand (b, "q")), cs[(size_t) b * 2], cs[(size_t) b * 2 + 1]);
    }
    for (int ch = 0; ch < (mode == ubm::EqGlobal ? 1 : 2); ++ch)
    {
        auto& path = ch == 0 ? curveA : curveB;
        bool firstPt = true;
        for (float x = ax.r.getX(); x <= ax.r.getRight() + 0.1f; x += 2.0f)
        {
            const float f = ax.hz (x);
            double d = 0;
            for (int b = 0; b < ubm::kEqBands; ++b)
                if (on[(size_t) b] && ubm::bandHits (mode, tg[(size_t) b], ch))
                {
                    d += ubm::magDb (cs[(size_t) b * 2], f, sr);
                    const auto t = ubm::bandSpecs()[(size_t) b].type;
                    if (t == ubm::BandType::LowCut || t == ubm::BandType::HighCut) d += ubm::magDb (cs[(size_t) b * 2 + 1], f, sr);
                }
            const float y = juce::jlimit (ax.r.getY(), ax.r.getBottom(), yFor ((float) d));
            if (firstPt) { path.startNewSubPath (x, y); firstPt = false; } else path.lineTo (x, y);
        }
    }
    dirty = false;
}

void EqGraph::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (b, 16.0f);
    const auto ax = axis();
    ax.drawGrid (g);
    for (int d : { -12, -6, 6, 12 })
    {
        g.setColour (juce::Colours::white.withAlpha (0.035f)); g.drawHorizontalLine ((int) yFor ((float) d), ax.r.getX(), ax.r.getRight());
        g.setColour (faint); g.setFont (mono (9.5f)); g.drawText ((d > 0 ? "+" : "") + juce::String (d), (int) ax.r.getRight() - 30, (int) yFor ((float) d) - 12, 28, 11, juce::Justification::right);
    }
    g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawHorizontalLine ((int) yFor (0), ax.r.getX(), ax.r.getRight());

    if (an.hasSignal())
    {
        juce::Path sp; sp.startNewSubPath (ax.r.getX(), ax.r.getBottom());
        for (float x = ax.r.getX(); x <= ax.r.getRight(); x += 3.0f)
            sp.lineTo (x, ax.r.getBottom() - ax.r.getHeight() * 0.85f * an.levelAt (ax.hz (x)));
        sp.lineTo (ax.r.getRight(), ax.r.getBottom()); sp.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillPath (sp);
    }
    if (dirty) rebuild();
    const int mode = (int) proc.get (ids::eqMode);
    if (mode != ubm::EqGlobal)
    {
        g.setColour (text.withAlpha (0.55f));
        g.strokePath (curveB, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    {
        juce::Path fill (curveA); fill.lineTo (ax.r.getRight(), yFor (0)); fill.lineTo (ax.r.getX(), yFor (0)); fill.closeSubPath();
        g.setColour (tiffany.withAlpha (0.08f)); g.fillPath (fill);
    }
    glowPath (g, curveA, 2.0f, tiffany, 4.0f, 0.45f);
    if (mode != ubm::EqGlobal)
    {
        g.setFont (mono (10.5f, true));
        g.setColour (tiffany); g.drawText (mode == ubm::EqMidSide ? "MID" : "LEFT", (int) ax.r.getX() + 10, (int) ax.r.getY() + 4, 60, 12, juce::Justification::left);
        g.setColour (text.withAlpha (0.6f)); g.drawText (mode == ubm::EqMidSide ? "SIDE" : "RIGHT", (int) ax.r.getX() + 52, (int) ax.r.getY() + 4, 60, 12, juce::Justification::left);
    }
    for (int bd = 0; bd < ubm::kEqBands; ++bd)
    {
        const auto p = nodePos (bd);
        const bool on = proc.get (ids::eqBand (bd, "on")) > 0.5f;
        const bool s = bd == selected;
        if (s) { g.setColour (tiffany.withAlpha (0.22f)); g.fillEllipse (p.x - 13, p.y - 13, 26, 26); }
        g.setColour (on ? (s ? tiffany : text) : panel3); g.fillEllipse (p.x - 7, p.y - 7, 14, 14);
        g.setColour (on ? bg : dim); g.setFont (mono (9.0f, true));
        g.drawText (juce::String (bd + 1), juce::Rectangle<float> (14, 14).withCentre (p), juce::Justification::centred);
    }
}

void EqGraph::mouseDown (const juce::MouseEvent& e)
{
    drag = nodeAt (e.position);
    if (drag >= 0)
    {
        selected = drag; if (onSelect) onSelect (drag);
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            const int b = drag;
            const bool on = proc.get (ids::eqBand (b, "on")) > 0.5f;
            m.addItem (1, on ? "Band aus" : "Band an");
            m.addSeparator();
            const int mode = (int) proc.get (ids::eqMode);
            const int tgt = (int) proc.get (ids::eqBand (b, "tgt"));
            m.addItem (10, mode == ubm::EqLeftRight ? "Links + Rechts" : "Mid + Side", mode != ubm::EqGlobal, tgt == 0);
            m.addItem (11, mode == ubm::EqLeftRight ? "nur Links" : "nur Mid", mode != ubm::EqGlobal, tgt == 1);
            m.addItem (12, mode == ubm::EqLeftRight ? "nur Rechts" : "nur Side", mode != ubm::EqGlobal, tgt == 2);
            m.addItem (20, "Reset Band");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, b] (int r) {
                if (r == 1) proc.setParam (ids::eqBand (b, "on"), proc.get (ids::eqBand (b, "on")) > 0.5f ? 0.0f : 1.0f);
                if (r >= 10 && r <= 12) proc.setParam (ids::eqBand (b, "tgt"), (float) (r - 10));
                if (r == 20)
                {
                    const auto& s = ubm::bandSpecs()[(size_t) b];
                    proc.setParam (ids::eqBand (b, "freq"), s.defFreq); proc.setParam (ids::eqBand (b, "gain"), 0.0f); proc.setParam (ids::eqBand (b, "q"), s.defQ);
                }
            });
            drag = -1; return;
        }
        for (auto* w : { "freq", "gain" }) proc.apvts.getParameter (ids::eqBand (drag, w))->beginChangeGesture();
    }
    repaint();
}

void EqGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (drag < 0) return;
    const auto ax = axis();
    auto set = [this] (const juce::String& id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    set (ids::eqBand (drag, "freq"), ax.hz (e.position.x));
    const auto t = ubm::bandSpecs()[(size_t) drag].type;
    if (t != ubm::BandType::LowCut && t != ubm::BandType::HighCut) set (ids::eqBand (drag, "gain"), juce::jlimit (-15.0f, 15.0f, dbFor (e.position.y)));
    if (proc.get (ids::eqBand (drag, "on")) < 0.5f) proc.setParam (ids::eqBand (drag, "on"), 1.0f);
}

void EqGraph::mouseUp (const juce::MouseEvent&)
{
    if (drag >= 0) for (auto* w : { "freq", "gain" }) proc.apvts.getParameter (ids::eqBand (drag, w))->endChangeGesture();
    drag = -1;
}

void EqGraph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = nodeAt (e.position);
    if (b >= 0) proc.setParam (ids::eqBand (b, "on"), proc.get (ids::eqBand (b, "on")) > 0.5f ? 0.0f : 1.0f);
}

void EqGraph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int b = nodeAt (e.position); if (b < 0) b = selected;
    const float q = proc.get (ids::eqBand (b, "q"));
    proc.setParam (ids::eqBand (b, "q"), juce::jlimit (0.2f, 10.0f, q * std::pow (1.25f, w.deltaY * 4.0f)));
}

// ================================================================================================ EQ panel
EqPanel::EqPanel (UBMasterProcessor& p, Analyzer& a)
    : ModulePanel (p, Icon::EQ, "Master-EQ", juce::String::fromUTF8 ("6 Bänder · Global, Mid/Side oder Links/Rechts · Punkte ziehen, Mausrad = Q"), ids::eqOn),
      mode (p, ids::eqMode, { "GLOBAL", "MID/SIDE", "L/R" }), graph (p, a)
{
    addAndMakeVisible (mode);
    addAndMakeVisible (graph);
    graph.onSelect = [this] (int b) { select (b); };
    for (int b = 0; b < ubm::kEqBands; ++b)
    {
        auto* btn = bandBtns.add (new PillButton (juce::String (b + 1)));
        btn->fontSize = 12.0f;
        btn->onClick = [this, b] { select (b); };
        addAndMakeVisible (btn);
        for (auto [w, l] : { std::pair<const char*, const char*> { "freq", "Freq" }, { "gain", "Gain" }, { "q", "Q" } })
            addChildComponent (bandKnobs.add (new Knob (p, ids::eqBand (b, w), l)));
        addChildComponent (targets.add (new Segmented (p, ids::eqBand (b, "tgt"), { "BEIDE", "M / L", "S / R" })));
        addChildComponent (bandOn.add (new PowerSwitch (p, ids::eqBand (b, "on"))));
    }
    select (2);
}

void EqPanel::select (int b)
{
    sel = b; graph.selected = b; graph.repaint();
    for (int i = 0; i < ubm::kEqBands; ++i)
    {
        bandBtns[i]->setSelected (i == b);
        const auto t = ubm::bandSpecs()[(size_t) i].type;
        const bool cut = t == ubm::BandType::LowCut || t == ubm::BandType::HighCut;
        for (int k = 0; k < 3; ++k) bandKnobs[i * 3 + k]->setVisible (i == b && ! (cut && k == 1));
        targets[i]->setVisible (i == b);
        bandOn[i]->setVisible (i == b);
    }
    resized(); repaint();
}

void EqPanel::layoutBody (juce::Rectangle<int> r)
{
    mode.setBounds (getWidth() - 62 - 16 - 260, 20, 260, 28);
    auto row = r.removeFromBottom (112);
    r.removeFromBottom (10);
    graph.setBounds (r);
    rowArea = row;
    auto left = row.removeFromLeft (196);
    auto btnRow = left.removeFromTop (34).withTrimmedTop (22);
    juce::ignoreUnused (btnRow);
    auto br = left.removeFromTop (28);
    const int bw = br.getWidth() / ubm::kEqBands;
    for (auto* b : bandBtns) b->setBounds (br.removeFromLeft (bw).reduced (2, 0));
    left.removeFromTop (12);
    auto tr = left.removeFromTop (28);
    for (int i = 0; i < ubm::kEqBands; ++i) { targets[i]->setBounds (tr); }
    auto onR = row.removeFromRight (70);
    for (auto* o : bandOn) o->setBounds (onR.withSizeKeepingCentre (44, 28).translated (0, -10));
    // knobs of the selected band
    const auto t = ubm::bandSpecs()[(size_t) sel].type;
    const bool cut = t == ubm::BandType::LowCut || t == ubm::BandType::HighCut;
    const int n = cut ? 2 : 3, w = 104;
    auto kr = row.withSizeKeepingCentre (w * n, row.getHeight());
    for (int k = 0; k < 3; ++k)
    {
        if (cut && k == 1) continue;
        for (int i = 0; i < ubm::kEqBands; ++i) if (i == sel) bandKnobs[i * 3 + k]->setBounds (kr.removeFromLeft (w));
    }
}

void EqPanel::paintOverChildren (juce::Graphics& g)
{
    g.setColour (dim); g.setFont (sans (11.0f, 600));
    g.drawText (juce::String ("BAND ") + juce::String (sel + 1) + juce::String::fromUTF8 (" · ") + ubm::bandSpecs()[(size_t) sel].name,
                rowArea.getX(), rowArea.getY() + 2, 260, 16, juce::Justification::left);
    g.drawText ("AN", rowArea.getRight() - 70, rowArea.getY() + 2, 70, 16, juce::Justification::centred);
}

// ================================================================================================ TAPE
TapePanel::TapePanel (UBMasterProcessor& p, Analyzer& a)
    : ModulePanel (p, Icon::Tape, "Tape-Sim", juce::String::fromUTF8 ("Band-Sättigung nur im gewählten Spektrum – Ränder ziehen oder Bereich verschieben"), ids::tapeOn),
      range (p, a, ids::tapeLo, ids::tapeHi, juce::String::fromUTF8 ("SÄTTIGUNG"))
{
    addAndMakeVisible (range);
    addKnob (ids::tapeDrive, "Drive");
    addKnob (ids::tapeChar, "Character");
    addKnob (ids::tapeMix, "Mix");
}

void TapePanel::layoutBody (juce::Rectangle<int> r)
{
    auto row = r.removeFromBottom (120);
    r.removeFromBottom (10);
    range.setBounds (r);
    layoutKnobs (row, 120);
}

// ================================================================================================ TRANSIENT
TransientPanel::TransientPanel (UBMasterProcessor& p, Analyzer& a)
    : ModulePanel (p, Icon::Transient, "Transient", juce::String::fromUTF8 ("Adaptiv & spektral: Transienten frequenzabhängig cutten oder boosten"), ids::trOn),
      mode (p, ids::trMode, { "CUT", "BOOST" }), delta (p, ids::trDelta, "DELTA"),
      range (p, a, ids::trLo, ids::trHi, "WIRKT")
{
    addAndMakeVisible (mode); addAndMakeVisible (delta); addAndMakeVisible (range);
    addKnob (ids::trAmount, "Amount");
    addKnob (ids::trSens, "Sensitivity");
    addKnob (ids::trSpeed, "Speed");
    addKnob (ids::trMix, "Mix");
    range.overlay = [this] (juce::Graphics& g, const FreqAxis& ax) {
        const int nb = ubm::TransientShaper::kBands;
        const float cy = ax.r.getCentreY() + 8, h = ax.r.getHeight() * 0.40f;
        for (int b = 0; b < nb; ++b)
        {
            const float d = proc.transient.displayDb[(size_t) b].load();
            const float x = ax.x (ubm::TransientShaper::bandCentre (b));
            const float bw = ax.r.getWidth() / nb * 0.62f;
            const float hh = juce::jlimit (-1.0f, 1.0f, d / 12.0f) * h;
            auto rr = juce::Rectangle<float> (x - bw * 0.5f, hh > 0 ? cy - hh : cy, bw, std::max (1.0f, std::abs (hh)));
            g.setColour (tiffany.withAlpha (0.25f + 0.75f * std::min (1.0f, std::abs (d) / 6.0f)));
            g.fillRoundedRectangle (rr, 1.5f);
        }
        g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawHorizontalLine ((int) cy, ax.r.getX(), ax.r.getRight());
    };
}

void TransientPanel::layoutBody (juce::Rectangle<int> r)
{
    mode.setBounds (getWidth() - 62 - 16 - 260 + 80, 20, 180, 28);
    auto row = r.removeFromBottom (120);
    r.removeFromBottom (10);
    range.setBounds (r);
    delta.setBounds (row.removeFromRight (110).withSizeKeepingCentre (110, 28).translated (0, -12));
    layoutKnobs (row, 110);
}

// ================================================================================================ STEREO
void Vectorscope::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (b, 16.0f);
    auto sq = b.withTrimmedBottom (26).reduced (10);
    const float s = std::min (sq.getWidth(), sq.getHeight());
    sq = sq.withSizeKeepingCentre (s, s);
    const auto c = sq.getCentre();
    const float R = s * 0.5f;
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawEllipse (c.x - R, c.y - R, 2 * R, 2 * R, 1.0f); g.drawEllipse (c.x - R * 0.5f, c.y - R * 0.5f, R, R, 1.0f);
    g.drawLine (c.x, c.y - R, c.x, c.y + R); g.drawLine (c.x - R, c.y, c.x + R, c.y);
    g.drawLine (c.x - R * 0.707f, c.y - R * 0.707f, c.x + R * 0.707f, c.y + R * 0.707f); g.drawLine (c.x - R * 0.707f, c.y + R * 0.707f, c.x + R * 0.707f, c.y - R * 0.707f);
    g.setFont (mono (9.5f)); g.setColour (faint);
    g.drawText ("M", juce::Rectangle<float> (20, 12).withCentre ({ c.x, c.y - R + 8 }), juce::Justification::centred);
    g.drawText ("L", juce::Rectangle<float> (20, 12).withCentre ({ c.x - R * 0.707f + 8, c.y - R * 0.707f + 8 }), juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (20, 12).withCentre ({ c.x + R * 0.707f - 8, c.y - R * 0.707f + 8 }), juce::Justification::centred);

    const int w = proc.scopeWrite.load (std::memory_order_acquire);
    float peak = 1e-4f;
    constexpr int n = 2048;
    for (int i = 0; i < n; ++i)
    {
        const int k = (w - n + i) & (UBMasterProcessor::kScope - 1);
        peak = std::max ({ peak, std::abs (proc.scopeL[(size_t) k].load()), std::abs (proc.scopeR[(size_t) k].load()) });
    }
    const float gain = 0.9f / std::max (0.25f, peak) * R;
    for (int i = 0; i < n; i += 2)
    {
        const int k = (w - n + i) & (UBMasterProcessor::kScope - 1);
        const float l = proc.scopeL[(size_t) k].load(), r = proc.scopeR[(size_t) k].load();
        const float x = (r - l) * 0.7071f * gain, y = -(l + r) * 0.7071f * gain;
        const float a = 0.10f + 0.5f * (float) i / n;
        g.setColour (tiffany.withAlpha (a));
        g.fillRect (c.x + juce::jlimit (-R, R, x), c.y + juce::jlimit (-R, R, y), 1.6f, 1.6f);
    }
    // correlation
    auto cr = b.removeFromBottom (26).reduced (14, 8);
    g.setColour (panel3); g.fillRoundedRectangle (cr, 5);
    const float corr = juce::jlimit (-1.0f, 1.0f, proc.correlation.load());
    const float cx = cr.getX() + (corr + 1) * 0.5f * cr.getWidth();
    g.setColour (corr < 0 ? text : tiffany); g.fillRoundedRectangle (juce::Rectangle<float> (cx - 3, cr.getY() - 2, 6, cr.getHeight() + 4), 3);
    g.setColour (faint); g.setFont (mono (9.0f));
    g.drawText ("-1", cr.withWidth (20).translated (-22, 0), juce::Justification::centredRight);
    g.drawText ("+1", cr.withX (cr.getRight() + 2).withWidth (20), juce::Justification::centredLeft);
}

void WidthView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (b, 16.0f);
    const FreqAxis ax { b.reduced (6, 10) };
    ax.drawGrid (g);
    const float xl = proc.get (ids::stXLo), xh = proc.get (ids::stXHi);
    const bool on = proc.get (ids::stOn) > 0.5f;
    const float w[3] { proc.get (ids::stLow), proc.get (ids::stMid), proc.get (ids::stHigh) };
    const float edges[4] { 20, xl, xh, 20000 };
    const char* nm[3] { "LOW", "MID", "HIGH" };
    for (int i = 0; i < 3; ++i)
    {
        const float x0 = ax.x (edges[i]), x1 = ax.x (edges[i + 1]);
        const float h = ax.r.getHeight() * 0.42f * (w[i] / 200.0f);
        auto rr = juce::Rectangle<float> (x0 + 3, ax.r.getCentreY() - h, x1 - x0 - 6, 2 * h);
        g.setColour ((on ? tiffany : faint).withAlpha (0.18f)); g.fillRoundedRectangle (rr, 8);
        g.setColour (on ? tiffany : faint); g.drawRoundedRectangle (rr, 8, 1.4f);
        g.setColour (text); g.setFont (mono (11.0f, true));
        g.drawText (juce::String (nm[i]) + "  " + juce::String ((int) std::round (w[i])) + "%", juce::Rectangle<float> (x0, ax.r.getY(), x1 - x0, 14), juce::Justification::centred);
    }
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    const float h100 = ax.r.getHeight() * 0.21f;
    g.drawHorizontalLine ((int) (ax.r.getCentreY() - h100), ax.r.getX(), ax.r.getRight());
    g.drawHorizontalLine ((int) (ax.r.getCentreY() + h100), ax.r.getX(), ax.r.getRight());
}

StereoPanel::StereoPanel (UBMasterProcessor& p)
    : ModulePanel (p, Icon::Stereo, "Stereo-Tool", juce::String::fromUTF8 ("Breite in 3 Bändern · 0 % = Mono, 100 % = original, 200 % = extra breit"), ids::stOn),
      scope (p), widths (p)
{
    addAndMakeVisible (scope); addAndMakeVisible (widths);
    addKnob (ids::stLow, "Width Low");
    addKnob (ids::stMid, "Width Mid");
    addKnob (ids::stHigh, "Width High");
    addKnob (ids::stXLo, "Low | Mid");
    addKnob (ids::stXHi, "Mid | High");
}

void StereoPanel::layoutBody (juce::Rectangle<int> r)
{
    scope.setBounds (r.removeFromLeft (r.getHeight() - 4));
    r.removeFromLeft (12);
    auto row = r.removeFromBottom (120);
    r.removeFromBottom (10);
    widths.setBounds (r);
    layoutKnobs (row, 100);
}

// ================================================================================================ EXCITER
void ExciterPanel::View::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (b, 16.0f);
    const FreqAxis ax { b.reduced (4, 8) };
    ax.drawGrid (g);
    const float f = proc.get (ids::exFreq), amt = proc.get (ids::exAmount) / 100.0f, ch = proc.get (ids::exChar) / 100.0f;
    const bool on = proc.get (ids::exOn) > 0.5f;
    if (an.hasSignal())
    {
        juce::Path sp; sp.startNewSubPath (ax.r.getX(), ax.r.getBottom());
        for (float x = ax.r.getX(); x <= ax.r.getRight(); x += 3.0f) sp.lineTo (x, ax.r.getBottom() - ax.r.getHeight() * 0.85f * an.levelAt (ax.hz (x)));
        sp.lineTo (ax.r.getRight(), ax.r.getBottom()); sp.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillPath (sp);
    }
    const float x0 = ax.x (f);
    juce::ColourGradient gr (tiffany.withAlpha (on ? 0.22f : 0.06f), x0, 0, tiffany.withAlpha (0.02f), ax.r.getRight(), 0, false);
    g.setGradientFill (gr); g.fillRect (juce::Rectangle<float> (x0, ax.r.getY(), ax.r.getRight() - x0, ax.r.getHeight()));
    // harmonic series sketch: fundamental at f, harmonics 2f..6f (even/odd weighted by character)
    for (int h = 1; h <= 6; ++h)
    {
        const float hf = f * (float) h;
        if (hf > 20000) break;
        const bool even = h % 2 == 0;
        const float k = h == 1 ? 0.8f : amt * (even ? (1.0f - ch) * 0.9f + 0.1f : ch * 0.9f + 0.25f) / (float) (h - 0.4f) * 1.8f;
        const float hh = ax.r.getHeight() * 0.8f * juce::jlimit (0.03f, 1.0f, k);
        juce::Path l; l.startNewSubPath (ax.x (hf), ax.r.getBottom()); l.lineTo (ax.x (hf), ax.r.getBottom() - hh);
        if (h == 1) { g.setColour (text.withAlpha (0.6f)); g.strokePath (l, juce::PathStrokeType (2.0f)); }
        else if (on) glowPath (g, l, 2.0f, tiffany, 3.0f, 0.4f);
        else { g.setColour (faint); g.strokePath (l, juce::PathStrokeType (2.0f)); }
    }
    g.setColour (text); g.setFont (mono (11.0f, true));
    g.drawText (U8 ("OBERTÖNE AB ") + (f >= 1000 ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String ((int) f) + " Hz") + U8 ("  ·  ")
                + (ch < 0.35f ? "TUBE (gerade)" : ch > 0.65f ? "TAPE (ungerade)" : "MIX"), b.reduced (14, 10).removeFromTop (14), juce::Justification::left);
}

ExciterPanel::ExciterPanel (UBMasterProcessor& p, Analyzer& a)
    : ModulePanel (p, Icon::Exciter, "Exciter", juce::String::fromUTF8 ("Erzeugt neue Obertöne in den Höhen – mehr Glanz & Präsenz ohne EQ-Boost"), ids::exOn),
      view (p, a)
{
    addAndMakeVisible (view);
    addKnob (ids::exAmount, "Amount");
    addKnob (ids::exFreq, "Freq");
    addKnob (ids::exChar, juce::String::fromUTF8 ("Tube ↔ Tape"));
}

void ExciterPanel::layoutBody (juce::Rectangle<int> r)
{
    auto row = r.removeFromBottom (120);
    r.removeFromBottom (10);
    view.setBounds (r);
    layoutKnobs (row, 120);
}

// ================================================================================================ LIMITER
void LimiterPanel::Graph::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (b, 16.0f);
    auto r = b.reduced (14, 12);
    const float ceil = proc.get (ids::limCeil);
    auto yLvl = [&] (float db) { return r.getY() + r.getHeight() * juce::jlimit (0.0f, 1.0f, -(db - 0.0f) / 36.0f); };
    for (int d = -6; d >= -30; d -= 6)
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f)); g.drawHorizontalLine ((int) yLvl ((float) d), r.getX(), r.getRight());
        g.setColour (faint); g.setFont (mono (9.5f)); g.drawText (juce::String (d), (int) r.getX() + 2, (int) yLvl ((float) d) - 12, 24, 11, juce::Justification::left);
    }
    const int n = (int) gr.size();
    juce::Path lv, grp;
    for (int i = 0; i < n; ++i)
    {
        const int k = (w + i) % n;
        const float x = r.getX() + r.getWidth() * (float) i / (n - 1);
        if (i == 0) { lv.startNewSubPath (x, r.getBottom()); grp.startNewSubPath (x, r.getY()); }
        lv.lineTo (x, yLvl (lvl[(size_t) k]));
        grp.lineTo (x, r.getY() + r.getHeight() * juce::jlimit (0.0f, 1.0f, -gr[(size_t) k] / 12.0f));
    }
    lv.lineTo (r.getRight(), r.getBottom()); lv.closeSubPath();
    g.setColour (juce::Colours::white.withAlpha (0.09f)); g.fillPath (lv);
    glowPath (g, grp, 2.0f, tiffany, 4.0f, 0.5f);
    // ceiling
    const float yc = yLvl (ceil);
    g.setColour (text.withAlpha (0.8f));
    for (float x = r.getX(); x < r.getRight(); x += 9) g.drawLine (x, yc, std::min (x + 5, r.getRight()), yc, 1.2f);
    g.setFont (mono (10.5f, true)); g.drawText ("CEILING " + juce::String (ceil, 1) + " dBTP", (int) r.getX() + 4, (int) yc + 3, 200, 13, juce::Justification::left);
    // current GR big
    const float cur = gr[(size_t) ((w - 1 + n) % n)];
    g.setColour (tiffany); g.setFont (mono (26.0f, true));
    auto vr = r.removeFromRight (150).removeFromBottom (36);
    g.drawText ((cur < -0.05f ? juce::String::fromUTF8 ("−") : "") + juce::String (std::abs (cur), 1), vr, juce::Justification::right);
    g.setColour (dim); g.setFont (mono (10.0f, true));
    g.drawText ("GAIN REDUCTION dB", vr.translated (0, -14).withHeight (12), juce::Justification::right);
}

LimiterPanel::LimiterPanel (UBMasterProcessor& p)
    : ModulePanel (p, Icon::Limiter, "Limiter", juce::String::fromUTF8 ("True-Peak-Brickwall mit Lookahead · Gain rein, Ceiling bleibt sauber"), ids::limOn),
      graph (p)
{
    addAndMakeVisible (graph);
    addKnob (ids::limGain, "Gain");
    addKnob (ids::limCeil, "Ceiling");
    addKnob (ids::limRelease, "Release");
}

void LimiterPanel::tick()
{
    const int n = (int) graph.gr.size();
    graph.gr[(size_t) graph.w] = proc.limiter.grDb.load();
    // output peak from scope tap (last ~16 ms)
    const int wr = proc.scopeWrite.load();
    float pk = 0;
    for (int i = 0; i < 768; ++i) { const int k = (wr - 768 + i) & (UBMasterProcessor::kScope - 1); pk = std::max ({ pk, std::abs (proc.scopeL[(size_t) k].load()), std::abs (proc.scopeR[(size_t) k].load()) }); }
    graph.lvl[(size_t) graph.w] = ubm::gainToDb (pk, -60.0f);
    graph.w = (graph.w + 1) % n;
    graph.repaint();
}

void LimiterPanel::layoutBody (juce::Rectangle<int> r)
{
    auto row = r.removeFromBottom (120);
    r.removeFromBottom (10);
    graph.setBounds (r);
    layoutKnobs (row, 120);
}
