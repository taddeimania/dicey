#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

#include "Engine/DiceEngine.h"
#include "Params.h"

// A tiny monophonic voice so the plugin can be heard without routing MIDI.
class TestToneVoice
{
public:
    void prepare (double sr) noexcept { sampleRate = sr; env = 0.0; stage = Stage::idle; }
    void noteOn (int note, int velocity) noexcept
    {
        freq   = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        amp    = velocity / 127.0;
        stage  = Stage::attack;
    }
    void noteOff() noexcept { if (stage != Stage::idle) stage = Stage::release; }
    void allOff() noexcept { stage = Stage::idle; env = 0.0; }

    void render (float* left, float* right, int start, int num, float level) noexcept;

private:
    enum class Stage { idle, attack, decay, sustain, release };
    double sampleRate = 48000.0, phase = 0.0, freq = 440.0, amp = 0.0, env = 0.0;
    Stage  stage = Stage::idle;
};

class DiceyProcessor final : public juce::AudioProcessor
{
public:
    DiceyProcessor();
    ~DiceyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // UI actions (message thread)
    void reDiceRhythm();
    void reDiceMelody();
    void requestRestart() { restartRequested.store (true); }
    void applyScale (int scaleIndex); // 0 major, 1 minor, 2 minor pentatonic, 3 dorian

    // UI state (read on message thread)
    int  getRingStep() const noexcept { return uiRingStep.load(); }
    int  getActiveNote() const noexcept { return uiActiveNote.load(); }
    bool isRunning() const noexcept { return uiRunning.load(); }
    bool isEffectivelyMuted() const noexcept { return uiMuted.load(); }
    int  getBeatCounter() const noexcept { return uiBeat.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    dicey::Params readParams() const;
    float get (const juce::String& id) const { return params.at (id)->load(); }

    std::map<juce::String, std::atomic<float>*> params;

    dicey::Sequencer sequencer;
    dicey::Params    lockedParams;
    bool             wasLocked = false;

    std::atomic<uint64_t> rhythmSeed { 0x1234ABCDull };
    std::atomic<uint64_t> melodySeed { 0x5678EF01ull };
    std::atomic<bool>     restartRequested { false };

    TestToneVoice tone;
    juce::MidiBuffer generated;

    std::atomic<int>  uiRingStep { -1 };
    std::atomic<int>  uiActiveNote { -1 };
    std::atomic<bool> uiRunning { false };
    std::atomic<bool> uiMuted { false };
    std::atomic<int>  uiBeat { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiceyProcessor)
};
