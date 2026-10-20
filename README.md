# Dicey

A stochastic melody and rhythm generator for Ableton Live, after the Vermona
meloDICER Eurorack sequencer. It sends monophonic MIDI to any instrument and
follows Live's transport and tempo.

Personal project, VST3, Apple silicon.

## Build (macOS)

You need the Xcode command line tools and CMake.

```sh
cd ~/Projects/melodicer
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

The first configure downloads JUCE 8.0.9, which takes a minute or two. The
build installs the plugin to `~/Library/Audio/Plug-Ins/VST3/Dicey.vst3`
automatically.

To run the engine tests:

```sh
cmake --build build --target DiceyEngineTests && ./build/DiceyEngineTests
```

## Using it in Live

1. **Preferences → Plug-Ins**: turn on *Use VST3 Plug-in System Folders*, then
   rescan. Dicey shows up under Plug-Ins → Taddei.
2. Drop **Dicey** on a MIDI track and press play. You'll hear the built-in test
   tone.
3. To drive a real instrument, create a second MIDI track with your synth on it.
   Set its **MIDI From** to the Dicey track, the chooser underneath to **Dicey**,
   and **Monitor** to **In**.
4. Switch the test tone off with button **C**.

Dicey only runs while Live's transport is playing.

## Controls

**Rhythm**: these four knobs work like the hardware's.
- **NOTE VALUE**: the base rhythm, from 1 to 1/32, including 1/4T and 1/8T.
- **VARIATION**: randomly brings in other note values. Turn left for longer
  values, right for shorter, and leave it at the centre for off. Row 2–3 of the
  table chooses which values it can use.
- **LEGATO**: the chance a note is held right up to the next one. Two held notes
  of the same pitch become one long note. Notes never overlap, because the output
  is monophonic.
- **REST**: the chance a step is silent.

**Melody**
- **C…B faders**: how likely each semitone is. The cap lights while its note is
  playing.
- **RANGE**: the lowest and highest note, across five octaves above the
  **OCTAVE** setting.

**Dice**
- **Click** RHYTHM or MELODY for new dice, which means a new pattern.
- **Hold** for realtime mode: the LED goes dark and every pass through the loop
  gets new values.
- **Click again** to return to the dice you had before.
- The dice are saved with your Live set.

**LOCK**: keeps playing the current pattern while you move the controls. Release
it to apply the changes.

**Loop window**: the ring shows the pattern position. Click **|◀** or **▶|**,
then drag or scroll the encoder to set the first or last step. Latch both to move
the whole window. Click the encoder to commit, or click it when nothing is latched
to restart on the next beat. Each ring step is 1/16 of the pattern, so in a 4-bar
pattern a step is one beat.

**Right column**
- **MUTE**: the pattern keeps running silently.
- **A**: restart.
- **B**: reset the window to all 16 steps.
- **C**: test tone.
- **D**: dice both sections.

**Table**
- Row 1: mute behaviour.
- Rows 2–3: which values VARIATION may use.
- Row 4: scale presets. They set the faders to a scale on **ROOT**, and you can
  edit them freely afterwards.
- Row 5: pattern length of 1, 2 or 4 bars.

**Extras**
- **VELOCITY** and **VEL RAND**.
- **GATE**: the length of non-legato notes.
- **LEVEL**: the test tone's volume.
- **OCTAVE**, **ROOT** and MIDI **CHANNEL**.

Every control can be automated from Live.

## Differences from the hardware

- Dicey follows Live's transport. There is no free-running internal clock and
  no TAP.
- There are no CV or gate inputs. Use Live automation or MIDI mapping on any
  parameter instead.
- Only the sequencer mode is included. The quantizer modes and Seq + Gate are
  left out.
- There is no 16-slot pattern memory. Live sets and presets keep the full
  state, including the dice.
- The extras are new: velocity, gate length, scale presets, pattern lengths
  longer than one bar, and the test tone.
- Realtime mode is deterministic for a given song position, so bouncing the same
  section twice gives the same notes.

## Layout

```
Source/Engine/DiceEngine.h   the generator and scheduler (pure C++, unit tested)
Source/PluginProcessor.*     parameters, Live transport, MIDI out, test tone, state
Source/PluginEditor.*        the panel
Source/UI/Widgets.*          knobs, faders, buttons, LEDs, step ring, table
Tests/EngineTests.cpp        engine tests
```
