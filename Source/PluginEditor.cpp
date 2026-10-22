#include "PluginEditor.h"

using namespace juce;
using namespace dicey;
using namespace dicey::ui;

namespace layout
{
    // Rhythm knobs
    constexpr float knobY = 148.0f;
    constexpr float knobR = 70.0f;
    constexpr float ringR = 91.0f;
    constexpr std::array<float, 4> knobX { 148.0f, 366.0f, 586.0f, 805.0f };

    // Faders
    constexpr std::array<float, 12> faderX { 74, 147, 220, 293, 366, 439, 512, 585, 658, 732, 805, 878 };
    constexpr float faderTop    = 340.0f;
    constexpr float faderBottom = 650.0f;
    constexpr std::array<float, 2> rangeX { 1025.0f, 1097.0f };

    // Ring
    constexpr float ringCX = 1061.0f, ringCY = 182.0f;

    // Right column
    constexpr float colX = 1253.0f;
    constexpr std::array<float, 7> colY { 143, 221, 299, 377, 455, 533, 611 };

    // Bottom
    constexpr float diceY = 850.0f, diceLedY = 792.0f;
    constexpr float rhythmX = 912.0f, melodyX = 1010.0f, lockX = 1155.0f;

    const Rectangle<int> table { 362, 690, 456, 200 };
}

static String noteName (int midiNote)
{
    if (midiNote < 0)
        return "--";
    return ids::noteNames()[midiNote % 12] + String (midiNote / 12 - 2); // Live: 60 = C3
}

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------
DiceyPanel::DiceyPanel (DiceyProcessor& p) : proc (p), apvts (p.apvts)
{
    setLookAndFeel (&lnf);
    setSize (designW, designH);

    using namespace layout;

    // Rhythm section
    addKnob (noteValue, ids::noteValue, knobX[0], knobY, knobR, "NOTE VALUE: the base rhythm.");
    noteValue.setMouseDragSensitivity (160);
    addKnob (variation, ids::variation, knobX[1], knobY, knobR,
             "VARIATION: randomly brings in other note values. Left = longer, right = shorter, centre = off.");
    addKnob (legato, ids::legato, knobX[2], knobY, knobR,
             "LEGATO: chance a note is held right up to the next one. Same pitch twice becomes one long note.");
    addKnob (rest, ids::rest, knobX[3], knobY, knobR, "REST: chance a step becomes silence.");

    // Melody faders
    for (int i = 0; i < 12; ++i)
    {
        auto& f = semis[(size_t) i];
        addAndMakeVisible (f);
        f.setBounds (Rectangle<float> (faderX[(size_t) i] - 22.0f, faderTop, 44.0f, faderBottom - faderTop).toNearestInt());
        f.setTooltip ("Probability of " + ids::noteNames()[i]);
        attachments.push_back (std::make_unique<SliderAttachment> (apvts, ids::semitone (i), f));
        f.setDoubleClickReturnValue (true, i == 0 ? 0.5 : 0.0);
    }

    for (int i = 0; i < 2; ++i)
    {
        auto& f = i == 0 ? rangeLo : rangeHi;
        addAndMakeVisible (f);
        f.setBounds (Rectangle<float> (rangeX[(size_t) i] - 22.0f, faderTop, 44.0f, faderBottom - faderTop).toNearestInt());
        f.setTooltip (i == 0 ? "RANGE low: lowest note (0-5 octaves above the Octave setting)"
                             : "RANGE high: highest note");
        attachments.push_back (std::make_unique<SliderAttachment> (apvts, i == 0 ? ids::rangeLo : ids::rangeHi, f));
        f.setDoubleClickReturnValue (true, i == 0 ? 0.2 : 0.4);
    }

    // Extras rail
    addKnob (velocity,   ids::velocity,   50.0f, 735.0f, 24.0f, "Velocity of generated notes");
    addKnob (velRandom,  ids::velRandom,  125.0f, 735.0f, 24.0f, "Random velocity spread");
    addKnob (gateLength, ids::gateLength, 200.0f, 735.0f, 24.0f, "Gate length of non-legato notes");
    addKnob (toneLevel,  ids::toneLevel,  275.0f, 735.0f, 24.0f, "Test tone volume");
    addKnob (octave,     ids::octave,     50.0f, 860.0f, 24.0f, "Octave at the bottom of the range");
    addKnob (root,       ids::root,       125.0f, 860.0f, 24.0f, "Root used by the SCALE presets in the table");
    addKnob (channel,    ids::channel,    200.0f, 860.0f, 24.0f, "MIDI output channel");

    // Clock: run slower (or faster) than the host
    addAndMakeVisible (clockDisplay);
    clockDisplay.setBounds (Rectangle<float> (66.0f, 38.0f).withCentre ({ 275.0f, 860.0f }).toNearestInt());
    clockDisplay.setTooltip (String::fromUTF8 ("CLOCK: how fast Dicey runs against the DAW's tempo. "
                                               "\xc3\xb7" "4 plays everything four times slower, "
                                               "\xc3\x97" "2 twice as fast."));
    clockDisplay.getText = [this] { return ids::clockRateNames()[roundToInt (getParam (ids::clockRate))]; };
    clockDisplay.buildMenu = [this] (PopupMenu& m)
    {
        const int cur = roundToInt (getParam (ids::clockRate));
        for (int i = 0; i < ids::clockRateNames().size(); ++i)
            m.addItem (i + 1, ids::clockRateNames()[i], true, i == cur);
    };
    clockDisplay.onChoose = [this] (int r) { setParam (ids::clockRate, (float) (r - 1)); };
    for (auto* k : { &octave, &root, &channel })
        k->setMouseDragSensitivity (140);

    // Step ring
    addAndMakeVisible (ring);
    ring.setBounds (Rectangle<float> (260.0f, 260.0f).withCentre ({ ringCX, ringCY }).toNearestInt());
    ring.setTooltip ("Pattern position. Click |< or >| then drag or scroll the encoder to set the loop window "
                     "(both = move it). Click the encoder to restart on the next beat.");
    ring.getFirst     = [this] { return roundToInt (getParam (ids::firstStep)) - 1; };
    ring.getLast      = [this] { return jmax (ring.getFirst(), roundToInt (getParam (ids::lastStep)) - 1); };
    ring.getStep      = [this] { return proc.getRingStep(); };
    ring.firstLatched = [this] { return firstLatched; };
    ring.lastLatched  = [this] { return lastLatched; };
    ring.onTurn       = [this] (int d) { turnEncoder (d); };
    ring.onPush       = [this]
    {
        if (firstLatched || lastLatched)
        {
            firstLatched = lastLatched = false;
            ring.repaint();
        }
        else
        {
            proc.requestRestart();
        }
    };

    // Right column
    addButton (firstBtn, colX, colY[0], "Set first step: click, then turn the encoder");
    addButton (lastBtn, colX, colY[1], "Set last step: click, then turn the encoder");
    addButton (muteBtn, colX, colY[2], "MUTE: the pattern keeps running silently");
    addButton (restartBtn, colX, colY[3], "Restart the pattern on the next beat");
    addButton (allBtn, colX, colY[4], "Reset the loop window to all 16 steps");
    addButton (toneBtn, colX, colY[5], "Built-in test tone on/off");
    addButton (diceBothBtn, colX, colY[6], "Roll new dice for rhythm and melody");

    firstBtn.onClick    = [this] { firstLatched = ! firstLatched; ring.repaint(); };
    lastBtn.onClick     = [this] { lastLatched = ! lastLatched; ring.repaint(); };
    muteBtn.onClick     = [this] { toggleParam (ids::mute); };
    restartBtn.onClick  = [this] { proc.requestRestart(); };
    allBtn.onClick      = [this] { setParam (ids::firstStep, 1.0f); setParam (ids::lastStep, 16.0f); };
    toneBtn.onClick     = [this] { toggleParam (ids::testTone); };
    diceBothBtn.onClick = [this]
    {
        proc.reDiceRhythm();
        proc.reDiceMelody();
        setParam (ids::rhythmRealtime, 0.0f);
        setParam (ids::melodyRealtime, 0.0f);
        diceFlash = rhythmFlash = melodyFlash = 1.0f;
    };

    const float ledX = colX - 57.0f;
    addLed (muteLed, ledX, colY[2]);
    addLed (restartLed, ledX, colY[3]);
    addLed (allLed, ledX, colY[4]);
    addLed (toneLed, ledX, colY[5]);
    addLed (diceBothLed, ledX, colY[6]);

    // Dice & lock
    addButton (diceRhythmBtn, rhythmX, diceY,
               "DICE rhythm: click for new dice. Hold for realtime mode (new values every loop); "
               "click again to return to the saved dice.");
    addButton (diceMelodyBtn, melodyX, diceY,
               "DICE melody: click for new dice. Hold for realtime mode (new values every loop); "
               "click again to return to the saved dice.");
    addButton (lockBtn, lockX, diceY, "LOCK: keep playing the current pattern while you set up the next one");

    auto diceClick = [this] (const char* realtimeId, bool rhythm)
    {
        if (getParam (realtimeId) > 0.5f)
            setParam (realtimeId, 0.0f); // back to the remembered dice
        else if (rhythm)
            proc.reDiceRhythm();
        else
            proc.reDiceMelody();
        (rhythm ? rhythmFlash : melodyFlash) = 1.0f;
    };
    diceRhythmBtn.onClick     = [diceClick] { diceClick (ids::rhythmRealtime, true); };
    diceMelodyBtn.onClick     = [diceClick] { diceClick (ids::melodyRealtime, false); };
    diceRhythmBtn.onLongPress = [this] { setParam (ids::rhythmRealtime, 1.0f); };
    diceMelodyBtn.onLongPress = [this] { setParam (ids::melodyRealtime, 1.0f); };
    lockBtn.onClick           = [this] { toggleParam (ids::lock); };

    addLed (rhythmLed, rhythmX, diceLedY);
    addLed (melodyLed, melodyX, diceLedY);
    addLed (lockLed, lockX, diceLedY);

    // Gate & note display
    addLed (gateLed, colX, 700.0f);
    addAndMakeVisible (display);
    display.setBounds (Rectangle<float> (100.0f, 72.0f).withCentre ({ colX, 806.0f }).toNearestInt());

    // Options table
    addAndMakeVisible (table);
    table.setBounds (layout::table);
    buildTable();

    startTimerHz (30);
}

DiceyPanel::~DiceyPanel()
{
    stopTimer();
    attachments.clear();
    setLookAndFeel (nullptr);
}

void DiceyPanel::addKnob (Knob& k, const String& id, float cx, float cy, float radius, const String& tip)
{
    addAndMakeVisible (k);
    k.setBounds (Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre ({ cx, cy }).toNearestInt());
    k.setTooltip (tip);
    attachments.push_back (std::make_unique<SliderAttachment> (apvts, id, k));
    if (auto* param = apvts.getParameter (id))
        k.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void DiceyPanel::addButton (RoundButton& b, float cx, float cy, const String& tip, float radius)
{
    addAndMakeVisible (b);
    b.setBounds (Rectangle<float> (radius * 2.0f + 4.0f, radius * 2.0f + 4.0f).withCentre ({ cx, cy }).toNearestInt());
    b.setTooltip (tip);
}

void DiceyPanel::addLed (Led& l, float cx, float cy, float size)
{
    addAndMakeVisible (l);
    l.setBounds (Rectangle<float> (size * 2.0f, size * 2.0f).withCentre ({ cx, cy }).toNearestInt());
}

float DiceyPanel::getParam (const String& id) const
{
    if (auto* v = apvts.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}

void DiceyPanel::setParam (const String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void DiceyPanel::toggleParam (const String& id)
{
    setParam (id, getParam (id) > 0.5f ? 0.0f : 1.0f);
}

void DiceyPanel::turnEncoder (int delta)
{
    int first = roundToInt (getParam (ids::firstStep));
    int last  = jmax (first, roundToInt (getParam (ids::lastStep)));

    if (firstLatched && lastLatched)
    {
        const int d = jlimit (1 - first, 16 - last, delta);
        first += d;
        last += d;
    }
    else if (firstLatched)
    {
        first = jlimit (1, last, first + delta);
    }
    else if (lastLatched)
    {
        last = jlimit (first, 16, last + delta);
    }
    else
    {
        return;
    }

    setParam (ids::firstStep, (float) first);
    setParam (ids::lastStep, (float) last);
    ring.repaint();
}

void DiceyPanel::buildTable()
{
    using Row  = OptionTable::Row;
    using Cell = OptionTable::Cell;

    auto boolCell = [this] (const String& text, const String& id, bool valueWhenActive)
    {
        return Cell { text,
                      [this, id, valueWhenActive] { return (getParam (id) > 0.5f) == valueWhenActive; },
                      [this, id, valueWhenActive] { setParam (id, valueWhenActive ? 1.0f : 0.0f); } };
    };

    auto toggleCell = [this] (int valueIndex)
    {
        const String id = ids::varAllowed (valueIndex);
        return Cell { kValueNames[(size_t) valueIndex],
                      [this, id] { return getParam (id) > 0.5f; },
                      [this, id] { toggleParam (id); } };
    };

    auto scaleCell = [this] (const String& text, int scale)
    {
        return Cell { text, nullptr, [this, scale] { proc.applyScale (scale); } };
    };

    auto barsCell = [this] (const String& text, int index)
    {
        return Cell { text,
                      [this, index] { return roundToInt (getParam (ids::bars)) == index; },
                      [this, index] { setParam (ids::bars, (float) index); } };
    };

    std::vector<Row> rows;
    rows.push_back (Row { "MUTE", { boolCell ("NO RESTART", ids::muteRestart, false),
                                    boolCell ("RESTART", ids::muteRestart, true), Cell {}, Cell {} } });
    rows.push_back (Row { "VARIATION", { toggleCell (0), toggleCell (1), toggleCell (2), toggleCell (3) } });
    rows.push_back (Row { "VARIATION", { toggleCell (4), toggleCell (5), toggleCell (6), toggleCell (7) } });
    rows.push_back (Row { "SCALE", { scaleCell ("MAJOR", 0), scaleCell ("MINOR", 1),
                                     scaleCell ("PENTA", 2), scaleCell ("DORIAN", 3) } });
    rows.push_back (Row { "LENGTH", { barsCell ("1 BAR", 0), barsCell ("2 BARS", 1), barsCell ("4 BARS", 2), Cell {} } });
    table.setRows (std::move (rows));
    table.setTooltip ("Options: mute behaviour, which values VARIATION may use, scale presets (with ROOT) "
                      "and pattern length.");
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------
void DiceyPanel::timerCallback()
{
    blinkCounter = (blinkCounter + 1) % 16;
    ring.setBlinkPhase (blinkCounter < 8);
    ring.repaint();

    const int  note    = proc.getActiveNote();
    const bool running = proc.isRunning();

    for (int i = 0; i < 12; ++i)
        semis[(size_t) i].setLit (note >= 0 && note % 12 == i);

    // Pattern loop start flashes the restart LED.
    const int beat = proc.getBeatCounter();
    if (beat != lastBeat)
    {
        lastBeat = beat;
        if (proc.getRingStep() == roundToInt (getParam (ids::firstStep)) - 1)
            restartFlash = 1.0f;
    }

    const bool muted = proc.isEffectivelyMuted() || getParam (ids::mute) > 0.5f;
    muteLed.setLevel (muted ? (blinkCounter < 8 ? 1.0f : 0.25f) : 0.0f);
    restartLed.setLevel (restartFlash);
    allLed.setLevel (roundToInt (getParam (ids::firstStep)) == 1 && roundToInt (getParam (ids::lastStep)) == 16 ? 1.0f : 0.0f);
    toneLed.setLevel (getParam (ids::testTone) > 0.5f ? 1.0f : 0.0f);
    diceBothLed.setLevel (diceFlash);

    // DICE LEDs: lit in dice mode, dark in realtime.
    const bool rRT = getParam (ids::rhythmRealtime) > 0.5f;
    const bool mRT = getParam (ids::melodyRealtime) > 0.5f;
    rhythmLed.setLevel (rRT ? 0.0f : jmax (0.75f, rhythmFlash));
    melodyLed.setLevel (mRT ? 0.0f : jmax (0.75f, melodyFlash));
    lockLed.setLevel (getParam (ids::lock) > 0.5f ? 1.0f : 0.0f);
    gateLed.setLevel (note >= 0 ? 1.0f : 0.0f);

    restartFlash = jmax (0.0f, restartFlash - 0.12f);
    diceFlash    = jmax (0.0f, diceFlash - 0.08f);
    rhythmFlash  = jmax (0.0f, rhythmFlash - 0.08f);
    melodyFlash  = jmax (0.0f, melodyFlash - 0.08f);

    // Display
    String status;
    if (! running)                         status = "STOPPED";
    else if (muted)                        status = "MUTED";
    else if (getParam (ids::lock) > 0.5f)  status = "LOCKED";
    else                                   status = "CH " + String (roundToInt (getParam (ids::channel)));

    // Show a non-default clock in the readout too, so a slowed-down Dicey is obvious.
    const int clockIndex = roundToInt (getParam (ids::clockRate));
    if (running && clockIndex != ids::clockRateDefaultIndex)
        status << "  " << ids::clockRateNames()[clockIndex];
    display.setText (noteName (note), status);

    if (blinkCounter % 3 == 0)
        table.refresh();
    clockDisplay.refresh();
}

// ---------------------------------------------------------------------------
// Artwork
// ---------------------------------------------------------------------------
static void strokeArc (Graphics& g, float cx, float cy, float r, float from, float to, float thickness, Colour c)
{
    Path p;
    p.addCentredArc (cx, cy, r, r, 0.0f, from, to, true);
    g.setColour (c);
    g.strokePath (p, PathStrokeType (thickness, PathStrokeType::curved, PathStrokeType::butt));
}

static void drawRotatedLabel (Graphics& g, const String& text, Point<float> at, float angle, float h)
{
    // Text runs tangent to the ring; flipped on the lower half so it never reads upside down.
    const float rot = std::cos (angle) < -0.3f ? angle + MathConstants<float>::pi : angle;
    Graphics::ScopedSaveState s (g);
    g.addTransform (AffineTransform::rotation (rot, at.x, at.y));
    drawLabel (g, text, at.x, at.y, h);
}

void DiceyPanel::paintKnobRings (Graphics& g) const
{
    using namespace layout;
    constexpr float start = MathConstants<float>::pi * 1.25f;
    constexpr float end   = MathConstants<float>::pi * 2.75f;

    // NOTE VALUE: one segment per value, labelled
    {
        const float cx = knobX[0], cy = knobY;
        for (int i = 0; i < kNumValues; ++i)
        {
            const float a = start + (float) i * (end - start) / (float) (kNumValues - 1);
            strokeArc (g, cx, cy, ringR, a - 0.12f, a + 0.12f, 7.0f, colours::ink);
            const Point<float> lp (cx + 116.0f * std::sin (a), cy - 116.0f * std::cos (a));
            drawRotatedLabel (g, kValueNames[(size_t) i], lp, a, 15.0f);
        }
    }

    // VARIATION: full ring with a centre mark
    strokeArc (g, knobX[1], knobY, ringR, start, end, 7.0f, colours::ink);
    g.setColour (colours::ink);
    g.drawLine (knobX[1], knobY - ringR - 4.0f, knobX[1], knobY - ringR - 16.0f, 4.0f);
    drawLabel (g, "LONGER", knobX[1] - 78.0f, 240.0f, 14.0f);
    drawLabel (g, "SHORTER", knobX[1] + 88.0f, 240.0f, 14.0f);

    // LEGATO, REST
    for (int k = 2; k < 4; ++k)
    {
        strokeArc (g, knobX[(size_t) k], knobY, ringR, start, end, 7.0f, colours::ink);
        drawLabel (g, "0%", knobX[(size_t) k] - 62.0f, 240.0f, 14.0f);
        drawLabel (g, "100%", knobX[(size_t) k] + 76.0f, 240.0f, 14.0f);
    }

    // Section labels
    drawLabel (g, "NOTE VALUE", knobX[0] + 4.0f, 268.0f, 21.0f);
    g.setColour (colours::ink);
    g.fillRect (Rectangle<float> (213.0f, 265.0f, 104.0f, 3.0f));
    drawLabel (g, "VARIATION", knobX[1] + 6.0f, 268.0f, 21.0f);
    drawLabel (g, "LEGATO", knobX[2] + 4.0f, 268.0f, 21.0f);
    drawLabel (g, "REST", knobX[3] + 4.0f, 268.0f, 21.0f);
}

void DiceyPanel::paintFaderArt (Graphics& g) const
{
    using namespace layout;
    g.setColour (colours::ink);

    for (int i = 0; i < 12; ++i)
    {
        drawLabel (g, ids::noteNames()[i], faderX[(size_t) i], 327.0f, 20.0f);
        drawLabel (g, String (i + 1), faderX[(size_t) i], 664.0f, 15.0f);
    }

    // Scale ticks between the faders
    g.setColour (colours::ink);
    for (int i = 0; i < 12; ++i)
    {
        const float x = faderX[(size_t) i] + 36.5f;
        for (int j = 0; j < 11; ++j)
        {
            const float y = 375.0f + (float) j * 23.7f;
            g.fillRoundedRectangle (Rectangle<float> (x - 17.0f, y - 2.0f, 34.0f, 4.0f), 2.0f);
        }
    }

    // Range
    drawLabel (g, "RANGE", (rangeX[0] + rangeX[1]) * 0.5f, 327.0f, 20.0f);
    const float mx = (rangeX[0] + rangeX[1]) * 0.5f;
    for (int j = 0; j < 6; ++j)
    {
        const float y = 376.0f + (float) j * 47.2f;
        const float w = 40.0f - (float) j * 2.5f;
        g.fillRoundedRectangle (Rectangle<float> (mx - w * 0.5f, y - (j == 0 ? 3.5f : 2.0f), w, j == 0 ? 7.0f : 4.0f), 2.0f);
    }

    // Down / up arrows to the floor & ceiling
    auto arrow = [&g] (float x, float y, bool up)
    {
        Path p;
        if (up)
        {
            g.fillRect (Rectangle<float> (x - 11.0f, y - 11.0f, 22.0f, 3.0f));
            p.addTriangle (x - 9.0f, y + 1.0f, x + 9.0f, y + 1.0f, x, y - 7.0f);
            g.fillRect (Rectangle<float> (x - 2.5f, y, 5.0f, 11.0f));
        }
        else
        {
            g.fillRect (Rectangle<float> (x - 2.5f, y - 11.0f, 5.0f, 11.0f));
            p.addTriangle (x - 9.0f, y - 1.0f, x + 9.0f, y - 1.0f, x, y + 7.0f);
            g.fillRect (Rectangle<float> (x - 11.0f, y + 8.0f, 22.0f, 3.0f));
        }
        g.fillPath (p);
    };
    g.setColour (colours::ink);
    arrow (rangeX[0], 672.0f, false);
    arrow (rangeX[1], 672.0f, true);
}

void DiceyPanel::paintRightColumn (Graphics& g) const
{
    using namespace layout;
    g.setColour (colours::ink);

    // |< and >| icons
    auto stepIcon = [&g] (float x, float y, bool toStart)
    {
        Path p;
        const float dir = toStart ? -1.0f : 1.0f;
        p.addTriangle (x + dir * 10.0f, y, x - dir * 2.0f, y - 9.0f, x - dir * 2.0f, y + 9.0f);
        g.fillPath (p);
        g.fillRect (Rectangle<float> (x - dir * 12.0f - 4.0f, y - 3.0f, 12.0f, 6.0f).translated (toStart ? 0.0f : 4.0f, 0.0f));
        g.fillRect (Rectangle<float> (x + dir * 12.0f - 2.0f, y - 11.0f, 4.0f, 22.0f));
    };
    stepIcon (1197.0f, colY[0] + 3.0f, true);
    stepIcon (1197.0f, colY[1] + 3.0f, false);

    auto letterBox = [&g] (const String& text, float x, float y, float w)
    {
        g.setColour (colours::ink);
        g.fillRect (Rectangle<float> (x, y - 14.0f, w, 28.0f));
        drawLabel (g, text, x + w * 0.5f, y, 19.0f, colours::panel);
    };

    letterBox ("MUTE", 1122.0f, colY[2], 56.0f);

    const char* letters[] = { "A", "B", "C", "D" };
    const char* subs[]    = { "restart", "all steps", "test tone", "dice both" };
    for (int i = 0; i < 4; ++i)
    {
        const float y = colY[(size_t) i + 3];
        letterBox (letters[i], 1148.0f, y - 6.0f, 26.0f);
        drawLabel (g, subs[i], 1181.0f, y + 26.0f, 13.0f, colours::accent);
    }

    drawLabel (g, "LOOP", colX - 3.0f, 182.0f, 13.0f, colours::inkDim);
}

void DiceyPanel::paintBottom (Graphics& g) const
{
    using namespace layout;

    // Extras rail labels & small arcs
    struct K { float x, y; const char* name; };
    const K ks[] = { { 50, 735, "VELOCITY" }, { 125, 735, "VEL RAND" }, { 200, 735, "GATE" }, { 275, 735, "LEVEL" },
                     { 50, 860, "OCTAVE" }, { 125, 860, "ROOT" }, { 200, 860, "CHANNEL" } };
    constexpr float start = MathConstants<float>::pi * 1.25f;
    constexpr float end   = MathConstants<float>::pi * 2.75f;
    for (const auto& k : ks)
    {
        strokeArc (g, k.x, k.y, 32.0f, start, end, 3.0f, colours::ink);
        drawLabel (g, k.name, k.x, k.y + 44.0f, 14.0f);
    }
    drawLabel (g, "CLOCK", 275.0f, 904.0f, 14.0f);

    // Rail divider
    g.setColour (colours::ink.withAlpha (0.25f));
    g.fillRect (Rectangle<float> (330.0f, 700.0f, 2.0f, 210.0f));

    // Dice & lock
    drawLabel (g, "RHYTHM", rhythmX, 758.0f, 16.0f);
    drawLabel (g, "MELODY", melodyX, 758.0f, 16.0f);
    drawLabel (g, "DICE", (rhythmX + melodyX) * 0.5f, 910.0f, 21.0f);
    drawLabel (g, "LOCK", lockX, 910.0f, 21.0f);

    drawLabel (g, "GATE", colX, 730.0f, 15.0f);
    drawLabel (g, "NOTE OUT", colX, 860.0f, 15.0f);
}

void DiceyPanel::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // Panel
    ColourGradient bg (colours::panel.brighter (0.04f), 0.0f, 0.0f, colours::panel.darker (0.08f), 0.0f, b.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (b);
    g.setColour (colours::panelEdge);
    g.drawRect (b, 3.0f);

    // Mounting slots
    for (auto p : { Point<float> (55, 25), Point<float> (1265, 25), Point<float> (55, 965), Point<float> (1265, 965) })
    {
        g.setColour (Colour (0xffe9e9e9));
        g.fillRoundedRectangle (Rectangle<float> (44.0f, 26.0f).withCentre (p), 13.0f);
        g.setColour (Colour (0xff8a8a8a));
        g.drawRoundedRectangle (Rectangle<float> (44.0f, 26.0f).withCentre (p), 13.0f, 1.0f);
    }

    // Title
    g.setColour (colours::accent);
    g.setFont (panelFont (32.0f));
    g.drawText ("DICEY", Rectangle<float> (390.0f, 12.0f, 120.0f, 40.0f), Justification::centredRight, false);
    g.setColour (colours::ink);
    g.setFont (panelFont (18.0f));
    g.drawText ("stochastic pattern generator  /  MIDI for Live", Rectangle<float> (522.0f, 16.0f, 420.0f, 34.0f),
                Justification::centredLeft, false);

    paintKnobRings (g);
    paintFaderArt (g);
    paintRightColumn (g);
    paintBottom (g);

    // A small die as the panel mark
    {
        const auto c = Point<float> (662.0f, 952.0f);
        const auto d = Rectangle<float> (30.0f, 30.0f).withCentre (c);
        g.setColour (colours::ink);
        g.drawRoundedRectangle (d, 6.0f, 2.2f);
        for (auto o : { Point<float> (-7, -7), Point<float> (7, -7), Point<float> (0, 0), Point<float> (-7, 7), Point<float> (7, 7) })
            g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (c + o));
    }
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------
DiceyEditor::DiceyEditor (DiceyProcessor& p) : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);

    constexpr double aspect = (double) DiceyPanel::designW / (double) DiceyPanel::designH;
    setResizable (true, true);
    setResizeLimits (660, 495, 1980, 1485);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (aspect);

    setSize (990, 743);
}

void DiceyEditor::paint (Graphics& g)
{
    g.fillAll (colours::panel);
}

void DiceyEditor::resized()
{
    const float scale = (float) getWidth() / (float) DiceyPanel::designW;
    panel.setTransform (AffineTransform::scale (scale));
    panel.setBounds (0, 0, DiceyPanel::designW, DiceyPanel::designH);
}
