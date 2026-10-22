#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UI/Widgets.h"

// The panel is laid out in a fixed 1320 x 990 design space and scaled to fit.
class DiceyPanel final : public juce::Component, private juce::Timer
{
public:
    static constexpr int designW = 1320;
    static constexpr int designH = 990;

    explicit DiceyPanel (DiceyProcessor&);
    ~DiceyPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;

    float getParam (const juce::String& id) const;
    void  setParam (const juce::String& id, float value);
    void  toggleParam (const juce::String& id);
    void  turnEncoder (int delta);

    void addKnob (dicey::ui::Knob& k, const juce::String& id, float cx, float cy, float radius, const juce::String& tip);
    void addButton (dicey::ui::RoundButton& b, float cx, float cy, const juce::String& tip, float radius = 30.0f);
    void addLed (dicey::ui::Led& l, float cx, float cy, float size = 30.0f);
    void buildTable();

    void paintKnobRings (juce::Graphics&) const;
    void paintFaderArt (juce::Graphics&) const;
    void paintRightColumn (juce::Graphics&) const;
    void paintBottom (juce::Graphics&) const;

    DiceyProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;
    dicey::ui::PanelLookAndFeel lnf;

    // Rhythm
    dicey::ui::Knob noteValue, variation { true }, legato, rest;

    // Melody
    std::array<dicey::ui::Fader, 12> semis;
    dicey::ui::Fader rangeLo, rangeHi;

    // Extras rail
    dicey::ui::Knob velocity, velRandom, gateLength, toneLevel, octave, root, channel;

    // Ring & buttons
    dicey::ui::StepRing ring;
    dicey::ui::RoundButton firstBtn { dicey::ui::RoundButton::Style::black };
    dicey::ui::RoundButton lastBtn { dicey::ui::RoundButton::Style::black };
    dicey::ui::RoundButton muteBtn { dicey::ui::RoundButton::Style::tan };
    dicey::ui::RoundButton restartBtn { dicey::ui::RoundButton::Style::cream };
    dicey::ui::RoundButton allBtn { dicey::ui::RoundButton::Style::cream };
    dicey::ui::RoundButton toneBtn { dicey::ui::RoundButton::Style::cream };
    dicey::ui::RoundButton diceBothBtn { dicey::ui::RoundButton::Style::cream };
    dicey::ui::RoundButton diceRhythmBtn { dicey::ui::RoundButton::Style::cream };
    dicey::ui::RoundButton diceMelodyBtn { dicey::ui::RoundButton::Style::cream };
    dicey::ui::RoundButton lockBtn { dicey::ui::RoundButton::Style::tan };

    dicey::ui::Led muteLed { dicey::ui::colours::ledRed };
    dicey::ui::Led restartLed { dicey::ui::colours::ledYellow };
    dicey::ui::Led allLed { dicey::ui::colours::ledYellow };
    dicey::ui::Led toneLed { dicey::ui::colours::ledYellow };
    dicey::ui::Led diceBothLed { dicey::ui::colours::ledYellow };
    dicey::ui::Led rhythmLed { dicey::ui::colours::ledGreen };
    dicey::ui::Led melodyLed { dicey::ui::colours::ledGreen };
    dicey::ui::Led lockLed { dicey::ui::colours::ledRed };
    dicey::ui::Led gateLed { dicey::ui::colours::ledYellow };

    dicey::ui::OptionTable table;
    dicey::ui::Display display;
    dicey::ui::ChoiceDisplay clockDisplay;

    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    bool firstLatched = false, lastLatched = false;
    int  blinkCounter = 0;
    int  lastBeat = 0;
    float restartFlash = 0.0f, diceFlash = 0.0f, rhythmFlash = 0.0f, melodyFlash = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiceyPanel)
};

class DiceyEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DiceyEditor (DiceyProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DiceyPanel panel;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DiceyEditor)
};
