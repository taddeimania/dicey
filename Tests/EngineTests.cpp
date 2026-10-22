// Engine tests — plain C++, no framework. Build target: DiceyEngineTests.
#include "../Source/Engine/DiceEngine.h"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace dicey;

static int failures = 0;
static int checks   = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        ++checks;                                                                   \
        if (! (cond)) {                                                             \
            ++failures;                                                             \
            std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                           \
    } while (0)

struct Msg
{
    long long sample; // absolute sample index
    bool      on;
    int       note;
    int       velocity;
};

// Run the sequencer over `beats` at the given tempo in blocks and collect MIDI.
static std::vector<Msg> run (Sequencer& seq, double beats, double bpm = 120.0, double sr = 48000.0,
                             int block = 512, double startPpq = 0.0, double rate = 1.0)
{
    std::vector<Msg> out;
    const double bps   = bpm / (60.0 * sr);
    const long long total = (long long) std::llround (beats / bps);
    for (long long s = 0; s < total; s += block)
    {
        BlockInfo b;
        b.playing    = true;
        b.ppq        = startPpq + s * bps;
        b.bpm        = bpm;
        b.sampleRate = sr;
        b.numSamples = (int) std::min<long long> (block, total - s);
        b.rate       = rate;
        seq.process (b, [&] (int off, bool on, int note, int vel) { out.push_back ({ s + off, on, note, vel }); });
    }
    return out;
}

static int countOns (const std::vector<Msg>& m)
{
    int n = 0;
    for (auto& x : m) n += x.on ? 1 : 0;
    return n;
}

// Every on is followed by its off before the next on; offs match notes.
static bool isMonophonic (const std::vector<Msg>& m)
{
    int active = -1;
    long long last = -1;
    for (auto& x : m)
    {
        if (x.sample < last) return false; // time must not go backwards
        last = x.sample;
        if (x.on)
        {
            if (active >= 0) return false;
            active = x.note;
        }
        else
        {
            if (active != x.note) return false;
            active = -1;
        }
    }
    return true;
}

static void testDefaultPattern()
{
    std::printf ("default pattern: 1/16 notes, C only, two octaves\n");
    Params p; // defaults mirror the manual's basic settings
    Pattern pat;
    buildPattern (p, 11, 22, pat);
    CHECK (pat.count == 16);
    std::set<int> notes;
    for (int i = 0; i < pat.count; ++i)
    {
        CHECK (! pat.events[i].rest);
        CHECK (std::abs (pat.events[i].length - 0.25) < 1e-9);
        notes.insert (pat.events[i].note);
    }
    // rangeLo 0.2 -> 12 semis, rangeHi 0.4 -> 24 semis, base C1 (24): C2 and C3.
    CHECK (notes.size() == 2);
    CHECK (notes.count (36) == 1);
    CHECK (notes.count (48) == 1);
}

static void testRestAll()
{
    std::printf ("REST at 100%% silences everything\n");
    Params p;
    p.rest = 1.0f;
    Sequencer seq;
    seq.setParams (p);
    auto m = run (seq, 8.0);
    CHECK (countOns (m) == 0);
}

static void testLegatoTies()
{
    std::printf ("LEGATO at 100%% on a single pitch ties into one long note\n");
    Params p;
    p.legato  = 1.0f;
    p.rangeLo = 0.2f;
    p.rangeHi = 0.2f; // only semitone 12
    Pattern pat;
    buildPattern (p, 5, 6, pat);
    int heads = 0;
    for (int i = 0; i < pat.count; ++i)
        if (! pat.events[i].tieFromPrev && ! pat.events[i].rest) ++heads;
    CHECK (heads == 1);
    CHECK (std::abs (pat.events[0].gate - 4.0) < 1e-9);

    Sequencer seq;
    seq.setParams (p);
    auto m = run (seq, 8.0);
    CHECK (countOns (m) == 2); // one per bar
    CHECK (isMonophonic (m));
}

static void testLegatoGapless()
{
    std::printf ("LEGATO at 100%% with changing pitch: no gaps, no overlaps\n");
    Params p;
    p.legato   = 1.0f;
    p.semitone = { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 };
    p.rangeHi  = 1.0f;
    Sequencer seq;
    seq.setParams (p);
    auto m = run (seq, 16.0);
    CHECK (isMonophonic (m));
    // Every off (except possibly the last) coincides with the next on.
    for (size_t i = 0; i + 1 < m.size(); ++i)
        if (! m[i].on)
            CHECK (m[i + 1].on && m[i + 1].sample == m[i].sample);
}

static void testTiming()
{
    std::printf ("1/16 notes at 120 bpm land every 6000 samples\n");
    Params p;
    Sequencer seq;
    seq.setParams (p);
    auto m = run (seq, 4.0, 120.0, 48000.0, 480);
    CHECK (countOns (m) == 16);
    int k = 0;
    for (auto& x : m)
        if (x.on)
            CHECK (x.sample == (long long) k++ * 6000);
    // Non-legato gate = 50% of the step.
    CHECK (! m[1].on && m[1].sample == 3000);
    CHECK (isMonophonic (m));
}

static void testDiceDeterminism()
{
    std::printf ("same seeds -> same notes; new seed -> different notes\n");
    Params p;
    p.semitone  = { 1, 0.5f, 1, 0, 0.7f, 1, 0, 1, 0.3f, 1, 0, 0.6f };
    p.rangeHi   = 1.0f;
    p.variation = 0.6f;
    p.rest      = 0.3f;
    p.legato    = 0.3f;
    p.variationAllowed.fill (true);

    auto notesFor = [&] (uint64_t r, uint64_t mm)
    {
        Sequencer s;
        s.setParams (p);
        s.setSeeds ({ r, mm, false, false });
        auto out = run (s, 8.0);
        std::vector<std::pair<long long, int>> v;
        for (auto& x : out) if (x.on) v.push_back ({ x.sample, x.note });
        return v;
    };

    CHECK (notesFor (100, 200) == notesFor (100, 200));
    CHECK (notesFor (100, 200) != notesFor (101, 200));
    CHECK (notesFor (100, 200) != notesFor (100, 201));

    // In dice mode, the pattern repeats bar to bar.
    auto v = notesFor (100, 200);
    std::vector<int> bar1, bar2;
    for (auto& [s, n] : v) (s < 96000 ? bar1 : bar2).push_back (n);
    CHECK (bar1 == bar2);
}

static void testRealtimeVaries()
{
    std::printf ("realtime melody changes each loop, rhythm stays put\n");
    Params p;
    p.semitone = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    p.rangeHi  = 1.0f;
    p.rest     = 0.4f;

    Sequencer s;
    s.setParams (p);
    s.setSeeds ({ 7, 8, false, true });
    auto out = run (s, 16.0);

    std::vector<std::vector<int>> barsNotes (4);
    std::vector<std::vector<long long>> barsTimes (4);
    for (auto& x : out)
        if (x.on)
        {
            const int bar = (int) (x.sample / 96000);
            barsNotes[bar].push_back (x.note);
            barsTimes[bar].push_back (x.sample % 96000);
        }
    CHECK (barsTimes[0] == barsTimes[1]);
    CHECK (barsTimes[1] == barsTimes[2]);
    CHECK (barsNotes[0] != barsNotes[1] || barsNotes[1] != barsNotes[2]);
}

static void testWindow()
{
    std::printf ("step window 5..8 loops one beat\n");
    Params p;
    p.firstStep = 4;
    p.lastStep  = 7;
    Pattern pat;
    buildPattern (p, 1, 2, pat);
    CHECK (std::abs (pat.loopLength - 1.0) < 1e-9);
    CHECK (std::abs (pat.windowStart - 1.0) < 1e-9);
    CHECK (pat.count == 4);

    Sequencer seq;
    seq.setParams (p);
    auto m = run (seq, 4.0);
    CHECK (countOns (m) == 16);

    // Window over a 4-bar pattern: each ring step is a quarter note.
    p.bars = 4;
    p.firstStep = 0;
    p.lastStep = 3;
    buildPattern (p, 1, 2, pat);
    CHECK (std::abs (pat.loopLength - 4.0) < 1e-9);
}

static void testVariationRespectsAllowed()
{
    std::printf ("VARIATION only picks allowed values, in the right direction\n");
    Params p;
    p.noteValue = kEighth;
    p.variation = 1.0f; // always vary, shorter
    p.variationAllowed = { true, true, true, false, true, false, true, false }; // no 1/32, no triplets
    Pattern pat;
    buildPattern (p, 3, 4, pat);
    for (int i = 0; i < pat.count; ++i)
        CHECK (std::abs (pat.events[i].length - 0.25) < 1e-9); // only 1/16 is shorter & allowed

    p.variation = -1.0f; // always longer
    buildPattern (p, 3, 4, pat);
    double sum = 0;
    for (int i = 0; i < pat.count; ++i)
    {
        sum += pat.events[i].length;
        CHECK (pat.events[i].length >= 0.5 - 1e-9 || i == pat.count - 1); // last may be truncated
    }
    CHECK (std::abs (sum - 4.0) < 1e-9); // pattern always fills exactly

    // Triplets come in full groups so the grid recovers.
    p.variation = 1.0f;
    p.noteValue = kQuarter;
    p.variationAllowed = { false, false, false, false, false, true, false, false }; // only 1/8T
    buildPattern (p, 9, 9, pat);
    CHECK (pat.count == 12);
    for (int i = 0; i < pat.count; ++i)
        CHECK (std::abs (pat.events[i].length - 1.0 / 3.0) < 1e-9);
}

static void testNoNotesAllowed()
{
    std::printf ("all semitone faders down -> silence\n");
    Params p;
    p.semitone.fill (0.0f);
    Sequencer seq;
    seq.setParams (p);
    CHECK (countOns (run (seq, 4.0)) == 0);
}

static void testPitchWeights()
{
    std::printf ("semitone weights shape the distribution\n");
    Params p;
    p.semitone.fill (0.0f);
    p.semitone[0] = 1.0f;  // C
    p.semitone[7] = 0.25f; // G
    p.rangeLo = 0.2f;
    p.rangeHi = 0.3f; // semis 12..18 -> C only... widen
    p.rangeHi = 0.35f; // 12..21 -> C(12), G(19)
    int c = 0, g = 0;
    for (uint32_t i = 0; i < 20000; ++i)
    {
        const int s = choosePitch (p, 42, i);
        if (s == 12) ++c;
        else if (s == 19) ++g;
        else CHECK (false);
    }
    const double ratio = (double) c / (double) g;
    CHECK (ratio > 3.5 && ratio < 4.5);

    // Inverted range is treated as the same range.
    std::swap (p.rangeLo, p.rangeHi);
    CHECK (choosePitch (p, 1, 1) == 12 || choosePitch (p, 1, 1) == 19);
}

static void testTransportStop()
{
    std::printf ("transport stop sends note-off\n");
    Params p;
    p.legato = 1.0f;
    Sequencer seq;
    seq.setParams (p);
    std::vector<Msg> out;
    BlockInfo b { true, 0.0, 120.0, 48000.0, 512 };
    seq.process (b, [&] (int o, bool on, int n, int v) { out.push_back ({ o, on, n, v }); });
    CHECK (countOns (out) == 1);
    b.playing = false;
    seq.process (b, [&] (int o, bool on, int n, int v) { out.push_back ({ o, on, n, v }); });
    CHECK (out.size() == 2 && ! out[1].on);
    // Nothing more while stopped.
    seq.process (b, [&] (int o, bool on, int n, int v) { out.push_back ({ o, on, n, v }); });
    CHECK (out.size() == 2);
}

static void testJumpAndMidSongStart()
{
    std::printf ("starting mid-song lands on the same notes as playing through\n");
    Params p;
    p.semitone.fill (1.0f);
    p.rangeHi = 1.0f;
    p.rest = 0.3f;

    Sequencer a;
    a.setParams (p);
    auto full = run (a, 8.0);

    Sequencer b;
    b.setParams (p);
    auto second = run (b, 4.0, 120.0, 48000.0, 512, 4.0);

    std::vector<int> fullBar2, startBar2;
    for (auto& x : full) if (x.on && x.sample >= 96000) fullBar2.push_back (x.note);
    for (auto& x : second) if (x.on) startBar2.push_back (x.note);
    CHECK (fullBar2 == startBar2);
}

static void testMute()
{
    std::printf ("mute suppresses notes; restart-on-unmute waits for the beat\n");
    Params p;
    Sequencer seq;
    seq.setParams (p);
    seq.setMuted (true, false);
    CHECK (countOns (run (seq, 2.0)) == 0);

    // Unmute with restart: open on the next whole beat, pattern restarts there.
    Sequencer s2;
    s2.setParams (p);
    s2.setSeeds ({ 1, 2, false, false });
    s2.setMuted (true, true);
    std::vector<Msg> out;
    const double bps = 120.0 / (60.0 * 48000.0);
    for (long long smp = 0; smp < 4 * 24000; smp += 512)
    {
        if (smp >= 30000) s2.setMuted (false, true); // ~1.25 beats in
        BlockInfo b { true, smp * bps, 120.0, 48000.0, 512 };
        s2.process (b, [&] (int o, bool on, int n, int v) { out.push_back ({ smp + o, on, n, v }); });
    }
    CHECK (! out.empty());
    CHECK (out.front().on && out.front().sample == 48000); // beat 2
    CHECK (isMonophonic (out));
}

static void testStressMonophony()
{
    std::printf ("random parameter sweep stays monophonic\n");
    for (uint32_t trial = 0; trial < 300; ++trial)
    {
        Params p;
        auto r = [&] (uint32_t f) { return (float) rand01 (999, trial, f); };
        p.noteValue = (int) (r (1) * kNumValues);
        p.variation = r (2) * 2.0f - 1.0f;
        p.legato    = r (3);
        p.rest      = r (4) * 0.8f;
        for (int i = 0; i < 12; ++i) p.semitone[i] = r (10 + i) > 0.5f ? r (30 + i) : 0.0f;
        p.semitone[0] = 0.5f;
        p.rangeLo   = r (5);
        p.rangeHi   = r (6);
        p.firstStep = (int) (r (7) * 16);
        p.lastStep  = p.firstStep + (int) (r (8) * (16 - p.firstStep));
        p.bars      = std::array<int, 3> { 1, 2, 4 }[(size_t) (r (9) * 3)];
        for (int i = 0; i < kNumValues; ++i) p.variationAllowed[i] = r (50 + i) > 0.4f;
        p.velocityRandom = r (60);

        Sequencer seq;
        seq.setParams (p);
        seq.setSeeds ({ trial, trial * 7 + 1, r (61) > 0.5f, r (62) > 0.5f });
        const double bpm = 60.0 + r (63) * 140.0;
        const double rates[] = { 2.0, 1.0, 0.5, 0.25, 0.125, 1.0 / 16.0, 1.0 / 32.0 };
        auto out = run (seq, 16.0, bpm, 44100.0, 64 + (int) (r (64) * 900), 0.0, rates[(int) (r (65) * 7)]);
        CHECK (isMonophonic (out));
        for (auto& x : out)
            if (x.on) CHECK (x.velocity >= 1 && x.velocity <= 127 && x.note >= 0 && x.note <= 127);
    }
}

static void testClockRate()
{
    std::printf ("clock rate slows everything down, aligned to the host\n");
    Params p; // 1/16 notes, 50% gate

    // At 1/4 rate a 1/16 note lasts a host quarter note: 24000 samples at 120 bpm.
    Sequencer s4;
    s4.setParams (p);
    auto m = run (s4, 16.0, 120.0, 48000.0, 512, 0.0, 0.25);
    CHECK (countOns (m) == 16);
    int k = 0;
    for (auto& x : m)
        if (x.on)
            CHECK (x.sample == (long long) k++ * 24000);
    CHECK (! m[1].on && m[1].sample == 12000); // gate scales too
    CHECK (isMonophonic (m));

    // At 1/32 a one-bar pattern of 1/16 notes spans 32 host bars.
    Sequencer s32;
    s32.setParams (p);
    CHECK (countOns (run (s32, 32.0, 120.0, 48000.0, 512, 0.0, 1.0 / 32.0)) == 4);

    // x2 doubles the speed.
    Sequencer s2;
    s2.setParams (p);
    CHECK (countOns (run (s2, 4.0, 120.0, 48000.0, 512, 0.0, 2.0)) == 32);

    // Starting mid-song at a slow rate lands on the same notes as playing through.
    p.semitone.fill (1.0f);
    p.rangeHi = 1.0f;
    p.rest    = 0.3f;
    Sequencer a;
    a.setParams (p);
    auto full = run (a, 32.0, 120.0, 48000.0, 512, 0.0, 0.25);
    Sequencer b;
    b.setParams (p);
    auto tail = run (b, 16.0, 120.0, 48000.0, 512, 16.0, 0.25);
    std::vector<int> fullTail, startTail;
    for (auto& x : full) if (x.on && x.sample >= 384000) fullTail.push_back (x.note);
    for (auto& x : tail) if (x.on) startTail.push_back (x.note);
    CHECK (! fullTail.empty() && fullTail == startTail);
}

static void testRestartOnHostBeatAtSlowRate()
{
    std::printf ("restart at a slow rate waits only for the next host beat\n");
    Params p;
    Sequencer seq;
    seq.setParams (p);
    std::vector<Msg> out;
    const double bps = 120.0 / (60.0 * 48000.0);
    bool requested = false;
    for (long long smp = 0; smp < 4 * 24000; smp += 512)
    {
        if (! requested && smp >= 30000) { seq.requestRestart(); requested = true; } // ~1.25 host beats
        BlockInfo b { true, smp * bps, 120.0, 48000.0, 512, 1.0 / 16.0 };
        seq.process (b, [&] (int o, bool on, int n, int v) { out.push_back ({ smp + o, on, n, v }); });
    }
    // Notes at 1/16 rate are a whole host bar apart: one at 0, then the restart
    // fires a fresh first step on host beat 2 (sample 48000).
    std::vector<long long> ons;
    for (auto& x : out) if (x.on) ons.push_back (x.sample);
    CHECK (ons.size() == 2 && ons[0] == 0 && ons[1] == 48000);
    CHECK (isMonophonic (out));
}

int main()
{
    testDefaultPattern();
    testRestAll();
    testLegatoTies();
    testLegatoGapless();
    testTiming();
    testDiceDeterminism();
    testRealtimeVaries();
    testWindow();
    testVariationRespectsAllowed();
    testNoNotesAllowed();
    testPitchWeights();
    testTransportStop();
    testJumpAndMidSongStart();
    testMute();
    testStressMonophony();
    testClockRate();
    testRestartOnHostBeatAtSlowRate();

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
