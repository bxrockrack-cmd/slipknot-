# Slipknot Drummer

A JUCE-based VST3 drum sampler focused on aggressive, live-feeling Nu-Metal drums. The sound palette is original and user-supplied: no Slipknot recordings or proprietary samples are bundled.

## Current MVP

- 14 sample slots: kick, two snare layers, four toms, closed/open hi-hat, ride, two crashes, china and splash.
- WAV loading from the plugin interface.
- Up to four takes per slot with round-robin playback.
- Hybrid snare: MIDI note 38 blends Bright and Tight layers; MIDI note 40 triggers Tight only.
- Master, snare blend, room reverb and global pitch controls.
- Stereo output, velocity response and one-shot playback.

The plugin is silent until WAV samples are loaded. See [SAMPLES.md](SAMPLES.md) for free/CC0 sourcing and setup notes.

## General MIDI note map

| Instrument | MIDI note |
|---|---:|
| Kick | 36 |
| Snare (hybrid) | 38 |
| Snare (tight) | 40 |
| Floor Tom | 43 |
| Low Tom | 45 |
| Mid Tom | 47 |
| High Tom | 50 |
| Closed Hat | 42 |
| Open Hat | 46 |
| Ride | 51 |
| Crash 1 | 49 |
| Crash 2 | 57 |
| China | 52 |
| Splash | 55 |

## Build

CMake 3.22+ and a C++17 toolchain are required. JUCE 8.0.12 is fetched automatically by CMake.

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --parallel

GitHub Actions builds VST3 packages for Windows, macOS and Linux. Download the Windows artifact from the latest successful [Actions run](https://github.com/bxrockrack-cmd/slipknot-/actions).

## Notes

This is an early sampler MVP, not a complete commercial drum library. Multichannel outputs, built-in grooves, advanced velocity-layer mapping, and acoustic round-robin humanisation are planned for later iterations.
