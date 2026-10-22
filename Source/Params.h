// Params.h — parameter IDs and layout for Dicey.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/DiceEngine.h"

namespace dicey::ids
{
inline constexpr const char* noteValue      = "noteValue";
inline constexpr const char* variation      = "variation";
inline constexpr const char* legato         = "legato";
inline constexpr const char* rest           = "rest";
inline constexpr const char* rangeLo        = "rangeLo";
inline constexpr const char* rangeHi        = "rangeHi";
inline constexpr const char* firstStep      = "firstStep";
inline constexpr const char* lastStep       = "lastStep";
inline constexpr const char* bars           = "bars";
inline constexpr const char* lock           = "lock";
inline constexpr const char* mute           = "mute";
inline constexpr const char* muteRestart    = "muteRestart";
inline constexpr const char* rhythmRealtime = "rhythmRealtime";
inline constexpr const char* melodyRealtime = "melodyRealtime";
inline constexpr const char* velocity       = "velocity";
inline constexpr const char* velRandom      = "velRandom";
inline constexpr const char* gateLength     = "gateLength";
inline constexpr const char* octave         = "octave";
inline constexpr const char* root           = "root";
inline constexpr const char* channel        = "channel";
inline constexpr const char* testTone       = "testTone";
inline constexpr const char* toneLevel      = "toneLevel";
inline constexpr const char* clockRate      = "clockRate";

inline juce::String semitone (int i) { return "semi" + juce::String (i); }
inline juce::String varAllowed (int i) { return "var" + juce::String (i); }

inline const juce::StringArray& noteNames()
{
    static const juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
    return n;
}

inline const juce::StringArray& octaveNames()
{
    static const juce::StringArray n { "C-1", "C0", "C1", "C2", "C3" }; // Live naming: MIDI 60 = C3
    return n;
}

inline const juce::StringArray& barNames()
{
    static const juce::StringArray n { "1 bar", "2 bars", "4 bars" };
    return n;
}

inline int barsFromIndex (int i) { return i == 2 ? 4 : (i == 1 ? 2 : 1); }

// Clock: how fast Dicey runs relative to the host. "\xc3\x97" is the multiply
// sign and "\xc3\xb7" the divide sign (UTF-8).
inline const juce::StringArray& clockRateNames()
{
    static const juce::StringArray n {
        juce::String::fromUTF8 ("\xc3\x97" "2"),  juce::String::fromUTF8 ("\xc3\x97" "1"),
        juce::String::fromUTF8 ("\xc3\xb7" "2"),  juce::String::fromUTF8 ("\xc3\xb7" "4"),
        juce::String::fromUTF8 ("\xc3\xb7" "8"),  juce::String::fromUTF8 ("\xc3\xb7" "16"),
        juce::String::fromUTF8 ("\xc3\xb7" "32")
    };
    return n;
}

inline constexpr int clockRateDefaultIndex = 1; // x1

// Dicey beats per host beat for each clock setting.
inline double clockRateFromIndex (int i)
{
    static constexpr double r[] { 2.0, 1.0, 0.5, 0.25, 0.125, 1.0 / 16.0, 1.0 / 32.0 };
    return r[juce::jlimit (0, 6, i)];
}
inline int baseNoteFromOctaveIndex (int i) { return 12 * (juce::jlimit (0, 4, i) + 1); } // index 0 = MIDI 12 = C-1 in Live
} // namespace dicey::ids

namespace dicey
{
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + "%"; });

    // Rhythm
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::noteValue, 1 }, "Note Value",
        StringArray (kValueNames.data(), (int) kValueNames.size()), (int) kSixteenth));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::variation, 1 }, "Variation",
        NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const int p = juce::roundToInt (std::abs (v) * 100.0f);
            if (p == 0) return String ("Off");
            return String (p) + (v < 0 ? "% longer" : "% shorter");
        })));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::legato, 1 }, "Legato",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::rest, 1 }, "Rest",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));

    // Melody
    for (int i = 0; i < 12; ++i)
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::semitone (i), 1 },
            ids::noteNames()[i], NormalisableRange<float> (0.0f, 1.0f), i == 0 ? 0.5f : 0.0f, pct));

    auto octs = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (v * 5.0f, 2) + " oct"; });
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::rangeLo, 1 }, "Range Low",
        NormalisableRange<float> (0.0f, 1.0f), 0.2f, octs));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::rangeHi, 1 }, "Range High",
        NormalisableRange<float> (0.0f, 1.0f), 0.4f, octs));

    // Pattern
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::firstStep, 1 }, "First Step", 1, 16, 1));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::lastStep, 1 }, "Last Step", 1, 16, 16));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::bars, 1 }, "Pattern Length", ids::barNames(), 0));

    // Performance switches
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::lock, 1 }, "Lock", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::mute, 1 }, "Mute", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::muteRestart, 1 }, "Restart On Unmute", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::rhythmRealtime, 1 }, "Rhythm Realtime", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::melodyRealtime, 1 }, "Melody Realtime", false));

    // Values VARIATION may introduce (default: no triplets, no 1/32)
    const Params defaults;
    for (int i = 0; i < kNumValues; ++i)
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::varAllowed (i), 1 },
            String ("Variation ") + kValueNames[(size_t) i], defaults.variationAllowed[(size_t) i]));

    // Live extras
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::velocity, 1 }, "Velocity", 1, 127, 100));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::velRandom, 1 }, "Velocity Random",
        NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::gateLength, 1 }, "Gate Length",
        NormalisableRange<float> (0.05f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::octave, 1 }, "Octave", ids::octaveNames(), 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::root, 1 }, "Scale Root", ids::noteNames(), 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::channel, 1 }, "MIDI Channel", 1, 16, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::clockRate, 1 }, "Clock",
        ids::clockRateNames(), ids::clockRateDefaultIndex));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::testTone, 1 }, "Test Tone", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::toneLevel, 1 }, "Test Tone Level",
        NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));

    return layout;
}
} // namespace dicey
