# Doppler Distance VST

A VST3 audio plugin built in C++ with the JUCE framework that simulates the acoustic effect of physical distance on a sound source. By combining time-based delay, amplitude attenuation, frequency filtering, and a physically modeled Doppler effect, it lets producers place audio elements spatially in a mix in a way that standard reverb and panning cannot replicate.

---

## Installation

1. Download the latest `.vst3` zip from the [Releases](../../releases) section.
2. Extract the zip and copy the folder into your DAW's VST3 plugin folder:
   - **Windows:** `C:\Program Files\Common Files\VST3\`
   - **macOS:** `/Library/Audio/Plug-Ins/VST3/`
3. Rescan plugins in your DAW. The plugin will appear as **Doppler Distance**.

> **Windows note:** If your DAW does not detect the plugin, you may need to install the [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).

---

## What It Does

Doppler Distance models how a sound changes as its source moves farther from or closer to a listener. As distance increases:

- The sound arrives **later** (speed-of-sound delay)
- The sound is **quieter** (inverse amplitude attenuation)
- The sound loses **high-frequency content** (atmospheric absorption)
- Moving the distance in real time produces a **Doppler pitch shift**

Each of these effects is independently toggleable, so you can use as much or as little of the simulation as the mix calls for.

---

## Features

### Distance Slider
The left-hand vertical slider controls the simulated distance from the listener, ranging from **1 to 150 feet**. All active effects derive their parameters from this single value, so the result is always physically coherent — closer means louder, brighter, and less delayed; farther means quieter, darker, and more delayed.

The distance parameter is **fully automatable** in any VST3-compatible DAW, allowing distance to be keyframed or envelope-controlled across a timeline.

### Enable Speed of Sound Delay
Introduces a time delay equal to `distance / speed_of_sound`. At the default reference distance of 4 ft the delay is effectively zero; at 150 ft it reaches approximately 125 ms. The delay is implemented via a **circular ring buffer** with **Hermite cubic interpolation**, which allows the read position to move fractionally without introducing the aliasing artifacts that simpler linear interpolation produces at higher pitch-shift rates.

### Enable Reverb
Adds a wet reverb signal whose parameters (wet level and stereo width) scale with distance, modeling the way distant sound sources blend into room acoustics more than close ones.

### Enable Frequency Attenuation
Applies a low-pass filter whose cutoff frequency decreases with distance, approximating the frequency-dependent atmospheric absorption described in **ISO 9613-1**. At close range the full frequency spectrum passes through; at 150 ft the cutoff drops to roughly 1.5 kHz, reflecting the characteristic "muffled" quality of distant sound.

### Doppler Effect (requires Speed of Sound Delay)

Two modes control how the pitch shifts when the distance changes in real time:

- **Natural Doppler** — models a physically moving source by velocity-limiting the rate at which the delay position can change. The maximum simulated source velocity is **100 ft/s (~68 mph)**, producing up to ±1.47 semitones of pitch shift. A pre-smoothing stage absorbs discrete slider steps so that only intentionally fast movements produce the pitch shift, avoiding artifacts. Gain also tracks the velocity-limited position so that amplitude and pitch shift stay physically coherent.

- **Custom Smoothing** — replaces the physics model with a user-controlled ramp time (10–500 ms), giving creative control over how quickly the pitch and delay transition when the slider moves.

---

## Technical Notes

This plugin was built as a learning project in real-time C++ audio programming. Key implementation considerations include:

- **Lock-free thread safety** — all parameters shared between the UI and audio threads are `std::atomic`, preventing data races without blocking the audio thread.
- **Zero allocation in the audio callback** — all buffers and state are pre-allocated in `prepareToPlay`; `processBlock` never calls `malloc` or `new`.
- **Hermite cubic interpolation** — a 4-point Catmull-Rom spline read from the delay ring buffer, which handles pitch shifts up to ~10–15% without aliasing (versus ~2% for linear interpolation).
- **Velocity-limiting Doppler** — rather than smoothing delay time directly, the natural Doppler mode caps the rate of change of the delay position per sample (`v / c` samples/sample), which correctly models a source that cannot exceed a maximum physical speed and produces a pitch shift proportional to that velocity.
- **Pre-smoother stage** — a `SmoothedValue` sits between the raw parameter value and the velocity limiter so that discrete slider ticks are absorbed without each one triggering a full-speed pitch burst.
- **Mode-switch continuity** — when switching between Doppler modes, the inactive tracker is snapped to the active one's current position before it takes over, preventing position jumps at the transition boundary.
- **DAW automation support** — the distance parameter is registered via JUCE's `AudioParameterFloat` and `addParameter()`, exposing it to the host's automation system with proper gesture begin/end signaling.

---

## TODO

- Add button for zeroing baseline distance
- Improve reverb algorithm and add options for different types of spaces
- Add info tooltips which can be hovered over to explain each feature in more detail
- Add custom graphics
- Add option to disable gain attenuation over distance for users who may simply want the other effects of distance without changing volume

---

## License

This project is licensed under the **GNU General Public License v3.0**.

This software is built with the [JUCE framework](https://juce.com/), which is also licensed under the GPL v3 for open-source use.

You are free to use, modify, and distribute this software under the terms of the GPL v3. Any derivative works must also be distributed under the GPL v3 with source code made available. See the full license text at [https://www.gnu.org/licenses/gpl-3.0.en.html](https://www.gnu.org/licenses/gpl-3.0.en.html).
