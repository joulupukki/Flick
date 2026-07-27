# Flick - Digital Guitar Pedal

## Development Workflow

**IMPORTANT: Git Commit Policy**
- **NEVER** commit or push changes without explicit user approval
- After making changes, present a summary and wait for approval
- Only commit/push when the user explicitly says to do so
- If uncertain, always ask before committing

## Project Overview

Flick is a multi-effect digital guitar pedal firmware for the Daisy Seed module. It combines reverb, tremolo, and delay effects, designed to replace the Strymon Flint with additional delay capabilities. The project is licensed under GPLv3.

### Target Platforms

The firmware supports two similar hardware platforms with identical I/O but different switch configurations:

- **Funbox**: Uses three horizontal on-off-on toggle switches, includes 4 DIP switches
- **HotHouse**: Uses three vertically-mounted on-off-on toggle switches, no DIP switches

Both platforms share:
- 6 potentiometers (knobs)
- 2 footswitches
- 2 LEDs
- Stereo audio I/O
- Daisy Seed module (STM32H750 @ 48kHz)

## Architecture

### Hardware Abstraction Layer

[daisy_hardware.h](src/daisy_hardware.h) / [daisy_hardware.cpp](src/daisy_hardware.cpp)

The `DaisyHardware` class (aliased as `Funbox`) provides a unified hardware proxy that abstracts platform differences through compile-time switches:

```cpp
#if defined(PLATFORM_funbox)
  // Funbox-specific pin mappings and switch enums
#else
  // Hothouse-specific pin mappings and switch enums
#endif
```

**Key Features:**
- Logical switch position mapping (RIGHT/HIGH=0, MIDDLE=1, LEFT/LOW=2) unifies both platforms
- Footswitch callbacks for normal, double, and long press detection
- Configurable audio sample rate and block size
- ADC management for analog controls (knobs)
- Switch debouncing for all digital inputs
- DFU bootloader entry via simultaneous long-press of both footswitches

### Audio Processing Pipeline

[flick.cpp](src/flick.cpp) - `AudioCallback()`

The audio callback processes samples in this order:

1. **Anti-alias filtering + 2:1 decimation** (96kHz codec → 48kHz DSP): every raw
   96kHz input sample passes through a 6th-order elliptic low-pass at 14kHz
   (`AntiAliasLowpass` in flick_filters.hpp) before decimation, so high-frequency
   interference (DMA/clock/switching hash) cannot fold into the audible band.
   This replaced the naive "drop every other sample" decimation, which was the
   root cause of a persistent aliased high-pitch tone. The 14kHz cutoff (flat to
   14kHz, so no audible effect on guitar) also attenuates residual input-side
   ~17kHz EMI picked up at the guitar input and gives strong anti-aliasing at
   Nyquist (-73dB @24k). (Legacy 6k/12k notch filters were removed once the
   anti-aliasing fix made them audibly redundant.)
2. **Input routing** (based on mono/stereo mode)
3. **Delay effect** (if enabled)
4. **Tremolo effect** (if enabled, with three modes)
5. **Reverb effect** (if enabled, with three types)
6. **Output routing** (based on mono/stereo mode)

### Effects Architecture

#### 1. Reverb System

Three reverb modes selectable via Toggle Switch 1 in normal mode, all using the same Dattorro plate reverb algorithm ([PlateauNEVersio/Dattorro.cpp](src/PlateauNEVersio/Dattorro.cpp)) but with different factory default parameters, dry/wet behaviour, and tank size (timeScale). There is a single `PlateReverb` instance; switching reverb type re-applies the saved parameter set and size for that type.

Per-mode tank size (`kTimeScale*` constants in flick.cpp): Ambient 1.6, Plate 1.0075, Room 1.0075 morphing to 1.8 (see Room below). Buffers support up to 4.0×.

**Ambient** (Toggle UP/RIGHT)
- Default (base) params: decay 0.85, diffusion 0.75, modulation 0.2, tone 0.725, pre-delay 0.06 (~15 ms)
- Dry/wet behaviour: **Wet-biased crossfade** (wet = knob 1 + 0.1 boost, dry = 1 − wet)
- **Ambient bloom morph**: knob 1 morphs all five params from the (editable) base toward a fixed bloom voice (`kAmbientMorphTarget` in flick.cpp: pre-delay 0.14 (~35 ms), decay 0.95, tone 0.80, mod 0.25, diffusion 0.82; tank size stays 1.6×). Low knob = the base ambient; max knob = a long-lingering, dense, present wash with the transient dissolved into it. Morph is normal-mode only; edit mode edits the base endpoint.
- Long, spacious sound (1.6× tank size) with gentle modulation for shimmer

**Plate** (Dattorro Algorithm, Toggle MIDDLE)
- Default params: decay 0.8, diffusion 0.85, modulation 0.0, tone 0.725, pre-delay 0.0
- Dry/wet behaviour: **Dry/Wet Mix** (dry = 1 − wet, wet = knob 1)
- Classic plate character, balanced dry/wet blend
- Algorithm: [PlateauNEVersio/Dattorro.cpp](src/PlateauNEVersio/Dattorro.cpp), based on Jon Dattorro's 1997 reverb paper

**Room** (Toggle DOWN/LEFT)
- Default (base) params: decay 0.4, diffusion 0.425, modulation 0.0, tone 0.725, pre-delay 0.0
- Dry/wet behaviour: **Hybrid blend** (dry = 1 − 0.5 × knob 1, wet = knob 1)
- **Room→Hall morph**: knob 1 also morphs all five params plus tank size from the (editable) small-room base toward a fixed hall voice (`kRoomHallMorphTarget` in flick.cpp: pre-delay 0.08, decay 0.55, tone 0.85, mod 0.1, diffusion 0.7; size 1.0075 → 1.8). Low knob = subtle room, max knob = deep hall. Morph is normal-mode only; reverb edit mode edits the base (small-room) endpoint. A gentle pitch sweep while turning the knob is expected (size rescaling).

**Modulation mapping** (`PlateReverb::SetModulation`): knob 0–1 → mod depth 0.1–1.0 (within the Dattorro design ceiling; excursion = depth × 16 samples) and LFO speed multiplier 0.5–1.5× the base rates (0.10–0.18 Hz). Gentle shimmer, no audible pitch bending.

#### 2. Tremolo System

[flick.cpp](src/flick.cpp)

Three tremolo modes via Toggle Switch 2:

**Sine Wave Tremolo** (TREMOLO_SINE)
- Smooth amplitude modulation
- Traditional tremolo sound
- Speed: 0.2-16 Hz
- Depth: 0-50%

**Harmonic Tremolo** (TREMOLO_HARMONIC)
- Splits signal into low/high bands
- Applies tremolo with opposite phase to each band
- Crossover filters @ 144Hz (LPF) and 636Hz (HPF)
- Additional EQ shaping:
  - HPF @ 63Hz
  - LPF @ 11.2kHz
  - Low shelf cut @ 37Hz (-10.5dB)
  - Peaking boost @ 254Hz (+2dB)
  - Peaking cut @ 7500Hz (-3.37dB)
- Depth: 0-62.5% (scaled 1.25x)

**Square Wave Tremolo** (TREMOLO_SQUARE)
- Rounded square wave (opto-style)
- Hard on/off character
- Speed: 0.2-16 Hz
- Depth: 0-50%

#### 3. Delay System

[flick.cpp](src/flick.cpp)

Simple digital delay with:
- Delay time: 50ms to 2 seconds (logarithmic), multiplied by timing subdivision
- Timing subdivision via Toggle Switch 3: Triplet (×0.3333), Quarter/straight (×1.0), Dotted Eighth (×0.75)
- Feedback: 0-100%
- Dry/wet mix: 0-100%
- Makeup gain: automatically engaged (×1.66 for delay, ×1.2 for tremolo) when delay or tremolo is active
- Stored in SDRAM
- Stereo independent processing

### DSP Components

**Oscillator** - [flick_oscillator.h](src/flick_oscillator.h) / [flick_oscillator.cpp](src/flick_oscillator.cpp)
- Two waveforms: sine (`WAVE_SIN`) and rounded square (`WAVE_SQUARE_ROUNDED`)
- Rounded square wave for opto-style tremolo
- Phase accumulator architecture

**Filters** - [flick_filters.hpp](src/flick_filters.hpp)
- `LowPassFilter`: One-pole exponential smoothing
- `HighPassFilter`: One-pole high-pass
- `PeakingEQ`: Biquad peaking/notch filter
- `LowShelf`: Biquad low-shelf filter
- `Biquad`: Generic Direct-Form-I biquad with externally supplied coefficients
- `AntiAliasLowpass`: 6th-order elliptic low-pass @14kHz (three cascaded `Biquad`s)
  applied to every 96kHz input sample before 2:1 decimation (see Audio Processing
  Pipeline step 1)

**Parameter Capture** - [parameter_capture.h](src/parameter_capture.h)
- `KnobCapture`: Soft takeover for knob parameters in edit modes
- `SwitchCapture`: Soft takeover for toggle switch parameters
- Prevents sudden parameter jumps when entering edit modes
- Captures current value on mode entry, freezes parameter updates
- Knobs activate after 5% movement threshold
- Switches activate on position change
- Drop-in replacement for `Parameter::Process()` in audio callback

### Operational Modes and Controls

Pedal controls, operational modes (reverb edit, device settings, tap tempo),
the knob/switch mapping table, LED indicators, factory reset, and on-device
debugging: see the `flick-ui-reference` skill.

### Persistent Settings

[flick.cpp](src/flick.cpp)

Settings are stored in QSPI flash via `PersistentStorage<Settings>` (see the
`Settings` / `ReverbEditParams` structs in flick.cpp). Each reverb type has its
own saved `ReverbEditParams` set. Version checking triggers factory reset if the
structure changes.

## Build System

[Makefile](src/Makefile)

Platform selection via `PLATFORM` variable:
```bash
make              # Funbox (default)
make PLATFORM=hothouse
```

## Memory Management

**SDRAM Usage:**
- Delay lines (2 seconds × 2 channels @ 48kHz)
- Plate reverb buffers (50 InterpDelay buffers, ~28.8 MB)

**Flash Usage:**
- Persistent settings in QSPI
- Settings versioning for migration

**Stack/Heap:**
- Reverb objects allocated statically
- No dynamic allocation in audio callback

## Platform Differences

### Pin Mapping

Platform-specific via preprocessor:
- Switch positions read differently
- Logical position mapping unifies both platforms
- Reverb type is selected via Toggle Switch 1 on both platforms (DIP switches no longer used)

### Switch Orientation

- **Funbox**: Horizontal (LEFT/MIDDLE/RIGHT)
- **HotHouse**: Vertical (DOWN/MIDDLE/UP)

Abstraction layer maps both to (RIGHT/MIDDLE/LEFT) = (0/1/2)

## Code Organization

### Architecture: Orchestrator + Modular Effects

Flick uses a clean separation between UX orchestration and DSP processing:

- **flick.cpp** - UX orchestrator (hardware I/O, modes, parameter processing, audio pipeline)
- **Effect modules** - Hardware-independent DSP processors (base classes + derived algorithms)

Tremolo and reverb use runtime polymorphism (virtual functions) for algorithm
switching; delay is a single class with no base.

**Design principles:**
- Effects are **pure DSP** - no knowledge of hardware, knobs, or switches
- **Bypass logic** managed by orchestrator, not effects
- **Parameter scaling** (e.g., tremolo depth × 1.25 for harmonic mode) happens in effect classes
- **Dry/wet mixing** happens in orchestrator (effects return wet-only signal)
- **Polymorphic switching** via base class pointers (`current_tremolo->ProcessSample()`)
- **Algorithm-specific parameters** use virtual methods with no-op defaults

## Key Constants

`SETTINGS_VERSION` (in [flick.cpp](src/flick.cpp)) must be incremented on any
`Settings` struct change **or** when saved values change meaning or defaults.

## Development Notes

### Performance Considerations
- Codec runs at 96kHz with block size 4, so the audio callback fires at ~24kHz
- Keep callback lean - complex logic outside
- Use SDRAM for large buffers
- Pre-calculate in main loop where possible
