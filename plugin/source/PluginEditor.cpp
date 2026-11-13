#include "PluginEditor.h"

using namespace theme;

#ifndef UBMASTER_VERSION_STRING
 #define UBMASTER_VERSION_STRING "1.0.0"
#endif

// ================================================================================================ NavBar
NavBar::NavBar (UBMasterProcessor& p) : proc (p)
{
    for (int i = 0; i < (int) platforms().size(); ++i)
    {
        auto* b = pills.add (new PillButton (platforms()[(size_t) i].name));
        b->onClick = [this, i] { proc.selectPlatform (i); refresh(); };
        addAndMakeVisible (b);
    }
    target.setFont (mono (17.0f, true));
    target.setJustification (juce::Justification::centred);
    target.setInputRestrictions (6, "-0123456789.,");
    target.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    target.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    target.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    target.setSelectAllWhenFocused (true);
    target.onReturnKey = [this] { commitTarget(); target.giveAwayKeyboardFocus(); };
    target.onFocusLost = [this] { commitTarget(); };
    target.setTooltip ("Ziel-Loudness (Integrated LUFS)");
    addAndMakeVisible (target);
    p.apvts.addParameterListener (ids::mode, this);
    p.apvts.addParameterListener (ids::target, this);
    refresh();
}

NavBar::~NavBar()
{
    proc.apvts.removeParameterListener (ids::mode, this);
    proc.apvts.removeParameterListener (ids::target, this);
}

void NavBar::commitTarget()
{
    const float v = juce::jlimit (-30.0f, -5.0f, target.getText().replaceCharacter (',', '.').getFloatValue());
    if (target.getText().trim().isEmpty()) { refresh(); return; }
    const float t = v > 0 ? -v : v;
    proc.setParam (ids::target, juce::jlimit (-30.0f, -5.0f, t));
    const int mode = (int) proc.get (ids::mode);
    if (mode < (int) platforms().size() - 1 && std::abs (platforms()[(size_t) mode].lufs - t) > 0.05f)
        proc.setParam (ids::mode, (float) (platforms().size() - 1));   // -> Custom
    refresh();
}

void NavBar::refresh()
{
    const int mode = (int) proc.get (ids::mode);
    for (int i = 0; i < pills.size(); ++i) pills[i]->setSelected (i == mode);
    if (! target.hasKeyboardFocus (true)) target.setText (juce::String (proc.get (ids::target), 1), false);
    repaint();
}

void NavBar::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    // glass: frosted graphite, top highlight, hairline
    juce::ColourGradient gl (juce::Colours::white.withAlpha (0.075f), 0, b.getY(), juce::Colours::white.withAlpha (0.03f), 0, b.getBottom(), false);
    g.setGradientFill (gl); g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
    g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawRoundedRectangle (b.reduced (0.5f), b.getHeight() * 0.5f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawHorizontalLine (1, b.getX() + 30, b.getRight() - 30);
    // brand
    auto lg = logo();
    g.setOpacity (1.0f);
    g.drawImage (lg, juce::Rectangle<float> (22, 15, 54, 26), juce::RectanglePlacement::centred);
    g.setColour (text); g.setFont (sans (21.0f, 700));
    g.drawText ("UBMaster", 84, 0, 140, getHeight(), juce::Justification::centredLeft);
    // target field frame
    auto tf = target.getBounds().toFloat().expanded (4, 2);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (tf, tf.getHeight() * 0.5f);
    g.setColour (tiffany.withAlpha (target.hasKeyboardFocus (true) ? 0.9f : 0.35f)); g.drawRoundedRectangle (tf, tf.getHeight() * 0.5f, 1.2f);
    g.setColour (dim); g.setFont (mono (10.5f, true));
    g.drawText ("ZIEL", (int) tf.getX() - 46, 0, 40, getHeight(), juce::Justification::centredRight);
    g.drawText ("LUFS", (int) tf.getRight() + 8, 0, 40, getHeight(), juce::Justification::centredLeft);
}

void NavBar::resized()
{
    auto r = getLocalBounds().reduced (12, 12);
    r.removeFromLeft (220);
    auto t = r.removeFromRight (150);
    target.setBounds (t.withTrimmedRight (48).withSizeKeepingCentre (84, 30));
    r.removeFromRight (30);
    const int pw[] { 92, 118, 96, 116, 88 };
    int total = 0; for (int w : pw) total += w + 8;
    auto pr = r.withSizeKeepingCentre (total, 32);
    for (int i = 0; i < pills.size(); ++i) { pills[i]->setBounds (pr.removeFromLeft (pw[i])); pr.removeFromLeft (8); }
}

// ================================================================================================ ModuleCard
ModuleCard::ModuleCard (UBMasterProcessor& p, int index, Icon ic, juce::String n, const juce::String& powerId, std::function<juce::String()> st)
    : proc (p), idx (index), icon (ic), name (std::move (n)), status (std::move (st))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    att = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter (powerId), [this] (float v) { on = v > 0.5f; repaint(); });
    att->sendInitialUpdate();
}

void ModuleCard::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked()) return;
    if (dotArea().expanded (6).contains (e.position)) att->setValueAsCompleteGesture (on ? 0.0f : 1.0f);
    else if (onSelect) onSelect (idx);
}

void ModuleCard::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool hover = isMouseOver (true);
    if (sel)
    {
        for (int i = 3; i >= 1; --i) { g.setColour (tiffany.withAlpha (0.05f * (4 - i))); g.drawRoundedRectangle (b.expanded ((float) i * 1.5f), 18.0f + i, 2.0f); }
    }
    card (g, b, 18.0f, sel ? panel2 : (hover ? panel2 : panel));
    if (sel) { g.setColour (tiffany.withAlpha (0.8f)); g.drawRoundedRectangle (b, 18.0f, 1.3f); }
    // icon
    auto ib = juce::Rectangle<float> (18, 18, 30, 30);
    auto path = theme::icon (icon, ib);
    if (on) glowPath (g, path, 1.8f, tiffany, 3.0f, sel ? 0.6f : 0.35f);
    else { g.setColour (faint); g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
    // name + step number
    g.setColour (faint); g.setFont (mono (10.0f, true));
    g.drawText ("0" + juce::String (idx + 1), 18, 58, 30, 12, juce::Justification::left);
    g.setColour (on ? text : dim); g.setFont (sans (15.0f, 700));
    g.drawText (name, 60, 16, (int) b.getWidth() - 96, 20, juce::Justification::left);
    g.setColour (on ? dim : faint); g.setFont (mono (11.0f, true));
    g.drawText (on ? status() : juce::String ("AUS"), 60, 38, (int) b.getWidth() - 70, 14, juce::Justification::left);
    // power dot
    auto d = dotArea();
    g.setColour (on ? tiffany.withAlpha (0.25f) : juce::Colours::white.withAlpha (0.05f)); g.fillEllipse (d);
    g.setColour (on ? tiffany : dim); g.fillEllipse (d.reduced (6.5f));
}

// ================================================================================================ MeterBar
MeterBar::MeterBar (UBMasterProcessor& p) : proc (p)
{
    addAndMakeVisible (reset);
    reset.onClick = [this] { proc.meter.resetRequest = true; tpHold = -100; };
}

void MeterBar::tick()
{
    auto sm = [] (float& v, float t, float k) { v = v < -90 ? t : v + (t - v) * k; };
    sm (m, proc.meter.momentary.load(), 0.35f);
    sm (s, proc.meter.shortTerm.load(), 0.25f);
    in = proc.meter.integrated.load();
    tp = proc.meter.truePeakDb.load();
    lra = proc.meter.lra.load();
    repaint();
}

void MeterBar::paint (juce::Graphics& g)
{
    card (g, getLocalBounds().toFloat());
    g.setColour (text); g.setFont (sans (15.0f, 700));
    g.drawText ("Loudness", 22, 18, 200, 20, juce::Justification::left);
    g.setColour (dim); g.setFont (mono (10.5f, true));
    g.drawText (U8 ("EBU R128 · ITU-R BS.1770-4 · ") + juce::String (proc.meter.seconds.load(), 0) + " s", 112, 19, 300, 20, juce::Justification::left);

    const float target = proc.get (ids::target), ceil = proc.get (ids::limCeil);
    auto area = getLocalBounds().reduced (22, 0).withTrimmedTop (56).withTrimmedBottom (18);
    struct Row { const char* label; float v; float lo, hi; float mark; const char* unit; bool lu; };
    const Row rows[] { { "INTEGRATED", in, -36, 0, target, "LUFS", false },
                       { "SHORT-TERM", s, -36, 0, target, "LUFS", false },
                       { "MOMENTARY", m, -36, 0, target, "LUFS", false },
                       { "TRUE PEAK", tp, -24, 3, ceil, "dBTP", false } };
    const int rh = (area.getHeight() - 8) / 5;
    for (auto& r : rows)
    {
        auto ra = area.removeFromTop (rh);
        auto lab = ra.removeFromLeft (104);
        auto val = ra.removeFromRight (108);
        g.setColour (dim); g.setFont (mono (10.5f, true)); g.drawText (r.label, lab, juce::Justification::centredLeft);
        const bool sig = r.v > -70.0f;
        g.setColour (text); g.setFont (mono (16.0f, true));
        g.drawText (sig ? juce::String (r.v, 1) : juce::String::fromUTF8 ("–"), val.removeFromLeft (64), juce::Justification::centredRight);
        g.setColour (faint); g.setFont (mono (10.0f)); g.drawText (r.unit, val.withTrimmedLeft (6), juce::Justification::centredLeft);
        auto bar = ra.toFloat().withSizeKeepingCentre ((float) ra.getWidth() - 12, 8.0f);
        g.setColour (panel3); g.fillRoundedRectangle (bar, 4.0f);
        if (sig)
        {
            const float t = juce::jlimit (0.0f, 1.0f, (r.v - r.lo) / (r.hi - r.lo));
            auto fb = bar.withWidth (std::max (8.0f, bar.getWidth() * t));
            const bool over = r.v > r.mark + 0.1f && juce::String (r.label) == "TRUE PEAK";
            juce::ColourGradient gr (tiffany.withAlpha (0.35f), fb.getX(), 0, over ? text : tiffany, fb.getRight(), 0, false);
            g.setGradientFill (gr); g.fillRoundedRectangle (fb, 4.0f);
        }
        const float mx = bar.getX() + bar.getWidth() * juce::jlimit (0.0f, 1.0f, (r.mark - r.lo) / (r.hi - r.lo));
        g.setColour (text); g.fillRoundedRectangle (mx - 1.0f, bar.getY() - 5, 2.0f, bar.getHeight() + 10, 1.0f);
        area.removeFromTop (2);
    }
    // LRA + GR row
    auto ra = area.removeFromTop (rh);
    g.setColour (dim); g.setFont (mono (10.5f, true));
    g.drawText ("LRA", ra.removeFromLeft (104), juce::Justification::centredLeft);
    g.setColour (text); g.setFont (mono (16.0f, true));
    g.drawText (juce::String (lra, 1) + " LU", ra.removeFromLeft (120), juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (mono (10.5f, true));
    g.drawText ("LIMITER GR", ra.removeFromLeft (110), juce::Justification::centredLeft);
    const float gr = proc.limiter.grDb.load();
    g.setColour (tiffany); g.setFont (mono (16.0f, true));
    g.drawText ((gr < -0.05f ? juce::String::fromUTF8 ("−") : "") + juce::String (std::abs (gr), 1) + " dB", ra.removeFromLeft (110), juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (mono (10.5f, true));
    g.drawText ("KORRELATION", ra.removeFromLeft (110), juce::Justification::centredLeft);
    g.setColour (text); g.setFont (mono (16.0f, true));
    g.drawText (juce::String (proc.correlation.load(), 2), ra.removeFromLeft (80), juce::Justification::centredLeft);
}

// ================================================================================================ AdviceBox
AdviceBox::AdviceBox (UBMasterProcessor& p) : proc (p)
{
    addAndMakeVisible (match);
    match.onClick = [this] {
        const float d = proc.matchTargetLoudness();
        flash = std::abs (d) < 0.05f ? juce::String ("Kein Signal / schon auf Ziel")
                                     : "Limiter-Gain " + ubm::fmtDb (d) + U8 (" dB  ·  Messung neu gestartet");
        flashTicks = 150;
    };
}

void AdviceBox::resized() { match.setBounds (getWidth() - 176, getHeight() - 50, 156, 32); }

void AdviceBox::tick()
{
    ubm::AdviceInput in;
    const int mode = (int) proc.get (ids::mode);
    in.platform = platforms()[(size_t) mode].name;
    in.custom = mode == (int) platforms().size() - 1;
    in.spotify = mode == 0;
    in.target = proc.get (ids::target);
    in.integrated = proc.meter.integrated.load(); in.shortTerm = proc.meter.shortTerm.load(); in.seconds = proc.meter.seconds.load();
    in.truePeak = proc.meter.truePeakDb.load(); in.ceiling = proc.get (ids::limCeil); in.lra = proc.meter.lra.load();
    in.grDb = proc.limiter.grDb.load(); in.limGain = proc.get (ids::limGain); in.limOn = proc.get (ids::limOn) > 0.5f;
    adv = ubm::makeAdvice (in);
    match.setEnabled (adv.hasSignal && ! adv.onTarget);
    if (flashTicks > 0) --flashTicks;
    repaint();
}

void AdviceBox::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    card (g, b);
    const int mode = (int) proc.get (ids::mode);
    g.setColour (tiffany); g.setFont (mono (10.5f, true));
    g.drawText (U8 ("EMPFEHLUNG  ·  ") + juce::String (platforms()[(size_t) mode].name).toUpperCase() + "  " + juce::String (proc.get (ids::target), 1) + " LUFS",
                22, 18, getWidth() - 44, 16, juce::Justification::left);
    g.setColour (text); g.setFont (sans (16.5f, 700));
    g.drawFittedText (adv.headline, 22, 38, getWidth() - 44, 22, juce::Justification::left, 1);
    float y = 70;
    for (auto& t : adv.tips)
    {
        g.setColour (tiffany); g.fillEllipse (24, y + 6, 5, 5);
        g.setColour (text.withAlpha (0.82f)); g.setFont (sans (12.5f));
        juce::AttributedString as; as.append (t, sans (12.5f), text.withAlpha (0.82f)); as.setWordWrap (juce::AttributedString::byWord);
        juce::TextLayout tl; tl.createLayout (as, (float) getWidth() - 62);
        tl.draw (g, { 38, y, (float) getWidth() - 62, tl.getHeight() });
        y += tl.getHeight() + 7;
        if (y > getHeight() - 66) break;
    }
    if (flashTicks > 0)
    {
        g.setColour (tiffany.withAlpha (juce::jmin (1.0f, flashTicks / 30.0f))); g.setFont (mono (10.5f, true));
        g.drawText (flash, 22, getHeight() - 46, getWidth() - 210, 24, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (faint); g.setFont (mono (10.0f));
        g.drawFittedText (juce::String::fromUTF8 (platforms()[(size_t) mode].note), juce::Rectangle<int> (22, getHeight() - 54, getWidth() - 214, 38), juce::Justification::centredLeft, 3, 1.0f);
    }
}

// ================================================================================================ Main
UBMasterMain::UBMasterMain (UBMasterProcessor& p)
    : proc (p), analyzer (p), nav (p), halo (p, analyzer), meters (p), advice (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (halo);
    panels.add (new EqPanel (p, analyzer));
    panels.add (new TapePanel (p, analyzer));
    panels.add (new TransientPanel (p, analyzer));
    panels.add (new StereoPanel (p));
    panels.add (new ExciterPanel (p, analyzer));
    panels.add (new LimiterPanel (p));
    for (auto* pn : panels) addChildComponent (pn);

    auto P = [&p] (const juce::String& id) { return p.get (id); };
    auto hz = [] (float v) { return v >= 1000 ? juce::String (v / 1000.0f, 1) + "k" : juce::String ((int) std::round (v)); };
    const std::array<std::tuple<Icon, const char*, juce::String, std::function<juce::String()>>, 6> defs { {
        { Icon::EQ, "EQ", ids::eqOn, [P] {
              const int m = (int) P (ids::eqMode); int act = 0;
              for (int b = 0; b < ubm::kEqBands; ++b) if (P (ids::eqBand (b, "on")) > 0.5f && (std::abs (P (ids::eqBand (b, "gain"))) > 0.05f || b == 0 || b == 5)) ++act;
              return juce::String (m == 0 ? "GLOBAL" : m == 1 ? "M/S" : "L/R") + juce::String::fromUTF8 (" · ") + juce::String (act) + juce::String::fromUTF8 (" BÄNDER"); } },
        { Icon::Tape, "TAPE", ids::tapeOn, [P, hz] { return "+" + juce::String (P (ids::tapeDrive), 1) + U8 (" dB · ") + hz (P (ids::tapeLo)) + "-" + hz (P (ids::tapeHi)); } },
        { Icon::Transient, "TRANSIENT", ids::trOn, [P] { return juce::String (P (ids::trMode) > 0.5f ? "BOOST " : "CUT ") + juce::String ((int) P (ids::trAmount)) + " %"; } },
        { Icon::Stereo, "STEREO", ids::stOn, [P] { return juce::String ((int) P (ids::stLow)) + " / " + juce::String ((int) P (ids::stMid)) + " / " + juce::String ((int) P (ids::stHigh)) + " %"; } },
        { Icon::Exciter, "EXCITER", ids::exOn, [P, hz] { return juce::String ((int) P (ids::exAmount)) + juce::String::fromUTF8 (" % · ab ") + hz (P (ids::exFreq)); } },
        { Icon::Limiter, "LIMITER", ids::limOn, [&p] { const float gr = p.limiter.grDb.load();
              return juce::String::fromUTF8 ("GR ") + (gr < -0.05f ? juce::String::fromUTF8 ("−") : "") + juce::String (std::abs (gr), 1) + " · " + juce::String (p.get (ids::limCeil), 1) + " dBTP"; } },
    } };
    for (int i = 0; i < 6; ++i)
    {
        auto& [ic, nm, pid, st] = defs[(size_t) i];
        auto* c = cards.add (new ModuleCard (p, i, ic, nm, pid, st));
        c->onSelect = [this] (int k) { select (k); };
        addAndMakeVisible (c);
    }
    addAndMakeVisible (nav);
    addAndMakeVisible (meters);
    addAndMakeVisible (advice);
    setSize (W, H);
    select (0);
    startTimerHz (60);
}

UBMasterMain::~UBMasterMain() { stopTimer(); setLookAndFeel (nullptr); }

void UBMasterMain::select (int m)
{
    current = juce::jlimit (0, panels.size() - 1, m);
    for (int i = 0; i < panels.size(); ++i) { panels[i]->setVisible (i == current); cards[i]->setSelected (i == current); }
}

void UBMasterMain::tick (float dt)
{
    analyzer.update();
    halo.tick (dt);
    panels[current]->tick();
    if (++frame % 2 == 0) { meters.tick(); for (auto* c : cards) c->repaint(); }
    if (frame % 6 == 0) advice.tick();
}

void UBMasterMain::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    // faint Tiffany aura top-left behind the halo
    juce::ColourGradient aura (tiffany.withAlpha (0.06f), 240, 290, tiffany.withAlpha (0.0f), 740, 290, true);
    g.setGradientFill (aura); g.fillRect (getLocalBounds());
    // chain arrows between cards
    for (int i = 0; i < cards.size() - 1; ++i)
    {
        auto a = cards[i]->getBounds(), b = cards[i + 1]->getBounds();
        const float x = (a.getRight() + b.getX()) * 0.5f, y = (float) a.getCentreY();
        juce::Path p; p.startNewSubPath (x - 3, y - 5); p.lineTo (x + 2, y); p.lineTo (x - 3, y + 5);
        g.setColour (faint); g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // footer
    g.setFont (sans (12.0f, 600)); g.setColour (dim);
    g.drawText ("by unique & MELO", 24, H - 36, 300, 24, juce::Justification::centredLeft);
    g.setFont (mono (10.5f)); g.setColour (faint);
    g.drawText (juce::String ("UBMaster ") + UBMASTER_VERSION_STRING + juce::String::fromUTF8 (" · UNIQUEBABY"), 0, H - 36, W, 24, juce::Justification::centred);
    g.setFont (sans (12.0f, 600)); g.setColour (isMouseOver() && linkArea.contains (getMouseXYRelative()) ? tiffany : dim);
    g.drawText ("uniquebaby.de", linkArea, juce::Justification::centredRight);
}

void UBMasterMain::mouseUp (const juce::MouseEvent& e)
{
    if (linkArea.contains (e.getPosition())) juce::URL ("https://uniquebaby.de").launchInDefaultBrowser();
}

void UBMasterMain::resized()
{
    nav.setBounds (20, 16, W - 40, 56);
    halo.setBounds (20, 84, 440, 408);
    for (auto* p : panels) p->setBounds (472, 88, W - 472 - 20, 400);
    const int cw = (W - 40 - 5 * 14) / 6;
    for (int i = 0; i < cards.size(); ++i) cards[i]->setBounds (20 + i * (cw + 14), 502, cw, 86);
    meters.setBounds (20, 602, 744, 214);
    advice.setBounds (776, 602, W - 776 - 20, 214);
    linkArea = { W - 220, H - 36, 196, 24 };
}

// ================================================================================================ Editor
UBMasterEditor::UBMasterEditor (UBMasterProcessor& p) : AudioProcessorEditor (p), main (p)
{
    addAndMakeVisible (main);
    setResizable (true, true);
    setResizeLimits (UBMasterMain::W / 2, UBMasterMain::H / 2, UBMasterMain::W * 2, UBMasterMain::H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) UBMasterMain::W / UBMasterMain::H);
    setSize ((int) (UBMasterMain::W * 0.86), (int) (UBMasterMain::H * 0.86));
}

void UBMasterEditor::resized()
{
    const float s = (float) getWidth() / (float) UBMasterMain::W;
    main.setTransform (juce::AffineTransform::scale (s));
}
