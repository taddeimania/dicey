#include "PluginProcessor.h"
#include "PluginEditor.h"

// ---------------------------------------------------------------------------
// Test tone
// ---------------------------------------------------------------------------
void TestToneVoice::render (float* left, float* right, int start, int num, float level) noexcept
{
    if (stage == Stage::idle)
        return;

    const double attackStep  = 1.0 / (0.003 * sampleRate);
    const double decayCoef   = std::exp (-1.0 / (0.12 * sampleRate));
    const double releaseCoef = std::exp (-1.0 / (0.06 * sampleRate));
    const double sustain     = 0.6;
    const double inc         = juce::MathConstants<double>::twoPi * freq / sampleRate;
    const double gain        = 0.25 * level * amp;

    for (int i = start; i < start + num; ++i)
    {
        switch (stage)
        {
            case Stage::attack:
                env += attackStep;
                if (env >= 1.0) { env = 1.0; stage = Stage::decay; }
                break;
            case Stage::decay:
                env = sustain + (env - sustain) * decayCoef;
                if (env - sustain < 1.0e-4) stage = Stage::sustain;
                break;
            case Stage::sustain:
                break;
            case Stage::release:
                env *= releaseCoef;
                if (env < 1.0e-5) { env = 0.0; stage = Stage::idle; }
                break;
            case Stage::idle:
                break;
        }

        // Sine with a little second harmonic: soft, easy to pitch-judge.
        const double s = std::sin (phase) + 0.25 * std::sin (2.0 * phase);
        phase += inc;
        if (phase > juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;

        const float out = (float) (s * env * gain);
        left[i] += out;
        if (right != nullptr)
            right[i] += out;

        if (stage == Stage::idle)
            break;
    }
}

// ---------------------------------------------------------------------------
// Processor
// ---------------------------------------------------------------------------
DiceyProcessor::DiceyProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DiceyState", dicey::createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            params[withId->paramID] = apvts.getRawParameterValue (withId->paramID);

    // Start with a fresh roll of the dice.
    juce::Random r;
    rhythmSeed.store ((uint64_t) r.nextInt64());
    melodySeed.store ((uint64_t) r.nextInt64());
}

bool DiceyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void DiceyProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    tone.prepare (sampleRate);
    generated.ensureSize ((size_t) juce::jmax (256, samplesPerBlock));
}

dicey::Params DiceyProcessor::readParams() const
{
    using namespace dicey;
    Params p;
    p.noteValue = juce::roundToInt (get (ids::noteValue));
    p.variation = get (ids::variation);
    p.legato    = get (ids::legato);
    p.rest      = get (ids::rest);
    for (int i = 0; i < 12; ++i)
        p.semitone[(size_t) i] = get (ids::semitone (i));
    p.rangeLo   = get (ids::rangeLo);
    p.rangeHi   = get (ids::rangeHi);
    p.firstStep = juce::roundToInt (get (ids::firstStep)) - 1;
    p.lastStep  = juce::jmax (p.firstStep, juce::roundToInt (get (ids::lastStep)) - 1);
    p.bars      = ids::barsFromIndex (juce::roundToInt (get (ids::bars)));
    for (int i = 0; i < kNumValues; ++i)
        p.variationAllowed[(size_t) i] = get (ids::varAllowed (i)) > 0.5f;

    p.velocity       = juce::roundToInt (get (ids::velocity));
    p.velocityRandom = get (ids::velRandom);
    p.gateLength     = get (ids::gateLength);
    p.baseNote       = ids::baseNoteFromOctaveIndex (juce::roundToInt (get (ids::octave)));
    return p;
}

void DiceyProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    using namespace dicey;

    const int numSamples = audio.getNumSamples();
    audio.clear();
    midi.clear(); // incoming MIDI is not used (yet)

    // --- Parameters, with LOCK holding the rhythm/melody/window section -----
    Params live = readParams();
    const bool locked = get (ids::lock) > 0.5f;
    if (locked && ! wasLocked)
        lockedParams = live;
    wasLocked = locked;

    Params p = live;
    if (locked)
    {
        p.noteValue        = lockedParams.noteValue;
        p.variation        = lockedParams.variation;
        p.legato           = lockedParams.legato;
        p.rest             = lockedParams.rest;
        p.semitone         = lockedParams.semitone;
        p.rangeLo          = lockedParams.rangeLo;
        p.rangeHi          = lockedParams.rangeHi;
        p.firstStep        = lockedParams.firstStep;
        p.lastStep         = lockedParams.lastStep;
        p.bars             = lockedParams.bars;
        p.variationAllowed = lockedParams.variationAllowed;
    }

    sequencer.setParams (p);
    sequencer.setSeeds ({ rhythmSeed.load(), melodySeed.load(),
                          get (ids::rhythmRealtime) > 0.5f, get (ids::melodyRealtime) > 0.5f });
    sequencer.setMuted (get (ids::mute) > 0.5f, get (ids::muteRestart) > 0.5f);
    if (restartRequested.exchange (false))
        sequencer.requestRestart();

    // --- Host transport ------------------------------------------------------
    BlockInfo info;
    info.numSamples = numSamples;
    info.sampleRate = getSampleRate();
    info.rate       = ids::clockRateFromIndex (juce::roundToInt (get (ids::clockRate)));
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            info.playing = pos->getIsPlaying();
            info.bpm     = pos->getBpm().orFallback (120.0);
            info.ppq     = pos->getPpqPosition().orFallback (0.0);
        }
    }

    // --- Generate ------------------------------------------------------------
    const int ch = juce::jlimit (1, 16, juce::roundToInt (get (ids::channel)));
    generated.clear();
    sequencer.process (info, [&] (int offset, bool on, int note, int velocity)
    {
        if (on)
            generated.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) velocity), offset);
        else
            generated.addEvent (juce::MidiMessage::noteOff (ch, note), offset);
    });

    // --- Test tone, rendered sample-accurately between events ---------------
    const bool  toneOn = get (ids::testTone) > 0.5f;
    const float level  = get (ids::toneLevel);
    float* left  = audio.getNumChannels() > 0 ? audio.getWritePointer (0) : nullptr;
    float* right = audio.getNumChannels() > 1 ? audio.getWritePointer (1) : nullptr;

    if (! toneOn)
        tone.allOff();

    int cursor = 0;
    for (const auto meta : generated)
    {
        const auto msg = meta.getMessage();
        const int  at  = juce::jlimit (0, numSamples, meta.samplePosition);
        if (toneOn && left != nullptr && at > cursor)
            tone.render (left, right, cursor, at - cursor, level);
        cursor = juce::jmax (cursor, at);

        if (toneOn)
        {
            if (msg.isNoteOn())
                tone.noteOn (msg.getNoteNumber(), msg.getVelocity());
            else if (msg.isNoteOff())
                tone.noteOff();
        }
    }
    if (toneOn && left != nullptr && cursor < numSamples)
        tone.render (left, right, cursor, numSamples - cursor, level);

    midi.addEvents (generated, 0, numSamples, 0);

    // --- UI state --------------------------------------------------------------
    const int prevStep = uiRingStep.load();
    const int step = sequencer.ringStep();
    uiRingStep.store (step);
    uiActiveNote.store (sequencer.activeNote());
    uiRunning.store (sequencer.isRunning());
    uiMuted.store (sequencer.isMutedNow());
    if (step != prevStep && step >= 0)
        uiBeat.fetch_add (1);
}

// ---------------------------------------------------------------------------
// UI actions
// ---------------------------------------------------------------------------
void DiceyProcessor::reDiceRhythm()
{
    rhythmSeed.store ((uint64_t) juce::Random::getSystemRandom().nextInt64());
}

void DiceyProcessor::reDiceMelody()
{
    melodySeed.store ((uint64_t) juce::Random::getSystemRandom().nextInt64());
}

void DiceyProcessor::applyScale (int scaleIndex)
{
    static const std::array<std::vector<int>, 4> scales {
        std::vector<int> { 0, 2, 4, 5, 7, 9, 11 }, // major
        std::vector<int> { 0, 2, 3, 5, 7, 8, 10 }, // natural minor
        std::vector<int> { 0, 3, 5, 7, 10 },       // minor pentatonic
        std::vector<int> { 0, 2, 3, 5, 7, 9, 10 }  // dorian
    };

    const auto& degrees = scales[(size_t) juce::jlimit (0, 3, scaleIndex)];
    const int   root    = juce::roundToInt (get (dicey::ids::root));

    for (int pc = 0; pc < 12; ++pc)
    {
        const int interval = (pc - root + 12) % 12;
        float v = 0.0f;
        if (std::find (degrees.begin(), degrees.end(), interval) != degrees.end())
            v = interval == 0 ? 1.0f : (interval == 7 ? 0.7f : 0.5f);

        if (auto* param = apvts.getParameter (dicey::ids::semitone (pc)))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (v));
            param->endChangeGesture();
        }
    }
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
void DiceyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("rhythmSeed", juce::String::toHexString ((juce::int64) rhythmSeed.load()), nullptr);
    state.setProperty ("melodySeed", juce::String::toHexString ((juce::int64) melodySeed.load()), nullptr);
    state.setProperty ("stateVersion", 1, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DiceyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (state.hasProperty ("rhythmSeed"))
        rhythmSeed.store ((uint64_t) state["rhythmSeed"].toString().getHexValue64());
    if (state.hasProperty ("melodySeed"))
        melodySeed.store ((uint64_t) state["melodySeed"].toString().getHexValue64());

    apvts.replaceState (state);
}

juce::AudioProcessorEditor* DiceyProcessor::createEditor()
{
    return new DiceyEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DiceyProcessor();
}
