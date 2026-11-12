Dicey is a stochastic melody and rhythm generator for any DAW that hosts VST3
plugins. It sends monophonic MIDI to any instrument and follows your DAW's
transport and tempo.

## New in 1.0.3

- The macOS build is signed and notarised by Apple, so your DAW loads it
  straight away, with no quarantine command first. Nothing else has changed
  since 1.0.2.

## Install

Download the file for your system below.

- **macOS** (universal, Apple silicon + Intel): unzip and move `Dicey.vst3`
  into `~/Library/Audio/Plug-Ins/VST3`.
- **Windows**: unzip and copy the `Dicey.vst3` folder into
  `C:\Program Files\Common Files\VST3`.
- **Linux**: `tar -xzf Dicey-*.tar.gz -C ~/.vst3`

Then rescan plug-ins in your DAW. The README covers routing Dicey's MIDI to
your instruments in Ableton Live and other DAWs.
