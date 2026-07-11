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

[flick.cpp](src/flick.cpp:533-848) - `AudioCallback()`

The audio callback processes samples in this order:

1. **Input routing** (based on mono/stereo mode)
2. **Notch filtering** (removes Daisy Seed resonant frequencies @ 6020Hz and 12278Hz)
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

[flick.cpp](src/flick.cpp:734-792)

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

[flick.cpp](src/flick.cpp:164-180)

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

**Parameter Capture** - [parameter_capture.h](src/parameter_capture.h)
- `KnobCapture`: Soft takeover for knob parameters in edit modes
- `SwitchCapture`: Soft takeover for toggle switch parameters
- Prevents sudden parameter jumps when entering edit modes
- Captures current value on mode entry, freezes parameter updates
- Knobs activate after 5% movement threshold
- Switches activate on position change
- Drop-in replacement for `Parameter::Process()` in audio callback

### Operational Modes

[flick.cpp](src/flick.cpp:86-103)

**Normal Mode** (`PEDAL_MODE_NORMAL`)
- Standard pedal operation
- Controls mapped to effect parameters
- Footswitch gestures:
  - Footswitch 1 (Left):
    - Single press: Toggle tremolo on/off (deferred past the double-press window)
    - Double press: Toggle reverb on/off
    - Long press: Enter reverb edit mode
  - Footswitch 2 (Right):
    - Single press: Toggle delay on/off
    - Double press: Enter tap tempo mode
    - Long press: Enter device settings
  - Both footswitches:
    - Simultaneous long press: Enter DFU (bootloader) mode

**Reverb Edit Mode** (`PEDAL_MODE_EDIT_REVERB`)
- Activated by long-press of Footswitch 1
- Both LEDs flash together
- Edits the **currently selected** reverb type (locked on entry — toggle switch 1 changes are ignored)
- Each reverb type has its own saved parameter set
- Uses parameter capture (soft takeover) to prevent sudden jumps
- Unified knob mapping (5 knobs, no toggle switches):
  - Knob 2: **Pre-delay** — pre-delay time
  - Knob 3: **Decay** — reverb tail length
  - Knob 4: **Tone** — tank high-cut filter (brightness)
  - Knob 5: **Modulation** — combined mod speed+depth
  - Knob 6: **Diffusion** — tank diffusion (density)
- Toggle switches are ignored in edit mode
- Footswitch 1: Cancel (restore previous)
- Footswitch 2: Save to flash

**Device Settings** (`PEDAL_MODE_EDIT_DEVICE_SETTINGS`)
- Activated by long-press of Footswitch 2
- LEDs flash alternately
- Toggle Switch 1: *(ignored)*
- Toggle Switch 2 selects polarity:
  - RIGHT/UP: Invert Left channel
  - MIDDLE: Normal (no inversion)
  - LEFT/DOWN: Invert Right channel
- Toggle Switch 3 selects mono/stereo mode:
  - LEFT: Mono In, Mono Out (MIMO)
  - MIDDLE: Mono In, Stereo Out (MISO)
  - RIGHT: Stereo In, Stereo Out (SISO)
- Footswitch 1: Cancel
- Footswitch 2: Save to flash

**Tap Tempo Mode** (`PEDAL_MODE_TAP_TEMPO`)
- Activated by double-press of Footswitch 2
- Right LED flashes at tapped tempo; left LED shows reverb status
- Delay is automatically enabled on entry if bypassed
- Footswitch 2 registers taps (tempo averaged from last 3 taps)
- Delay knob (Knob 4) frozen via KnobCapture until physically moved (overrides tapped tempo)
- Toggle Switch 3 timing subdivision still applies to tapped tempo
- Auto-exits after 4 seconds of no taps
- Footswitch 1: Exit tap tempo (return to normal mode)
- `just_exited_tap_tempo` flag prevents spurious double/long press events on exit

### Persistent Settings

[flick.cpp](src/flick.cpp:106-140)

Settings stored in QSPI flash via `PersistentStorage<Settings>`:

```cpp
struct ReverbEditParams {
  float pre_delay;          // Pre-delay amount
  float decay;              // Reverb decay / tail length
  float tone;               // Brightness (high-cut filter)
  float modulation;         // Movement / shimmer
  float diffusion;          // Density / smearing
};

struct Settings {
  int version;              // SETTINGS_VERSION for migration
  ReverbEditParams ambient_params;   // Ambient mode edit params (Dattorro, wet-biased)
  ReverbEditParams plate_params;     // Plate mode edit params (Dattorro, dry/wet mix)
  ReverbEditParams room_params;      // Room mode edit params (Dattorro, hybrid blend + hall morph base)
  int mono_stereo_mode;     // I/O routing mode
  int polarity_mode;        // Phase inversion mode
  bool bypass_reverb;       // Reverb bypass state
  bool bypass_tremolo;      // Tremolo bypass state
  bool bypass_delay;        // Delay bypass state
  float tapped_delay_samples; // Persisted tap tempo delay time (0 = use knob)
};
```

Version checking triggers factory reset if structure changes.

### Factory Reset

[flick.cpp](src/flick.cpp:973-1023)

Initiated by holding Footswitch 2 during boot:
1. LEDs blink alternately
2. Rotate Knob 1: 0% → 100% → 0% → 100% → 0%
3. Each stage increases blink rate
4. Final step restores defaults and starts pedal

## Build System

[Makefile](src/Makefile)

Platform selection via `PLATFORM` variable:
```bash
make              # Funbox (default)
make PLATFORM=hothouse
```

**Sources:**
- Core: `flick.cpp`, `daisy_hardware.cpp`, `flick_oscillator.cpp`
- Reverb: `plate_reverb.cpp`
- Tremolo: `tremolo_effect.cpp`
- Delay: `delay_effect.cpp`
- PlateauNEVersio: Dattorro implementation and dependencies

**Dependencies:**
- libDaisy: Hardware abstraction for Daisy Seed
- DaisySP: DSP library (delay lines, filters, etc.)

**Compilation:**
- C++ for STM32H750
- Uses ARM CMSIS DSP
- SDRAM for large delay buffers
- QSPI flash for persistent storage

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

## User Interface

**Control Mapping:**

| Control | Normal Mode | Tap Tempo | Reverb Edit | Settings Edit |
|---------|-------------|-----------|-------------|---------------|
| Knob 1  | Reverb amount | Reverb amount | Reverb amount | - |
| Knob 2  | Trem speed | Trem speed | Pre-delay | - |
| Knob 3  | Trem depth | Trem depth | Decay | - |
| Knob 4  | Delay time | Delay time (frozen) | Tone | - |
| Knob 5  | Delay feedback | Delay feedback | Modulation | - |
| Knob 6  | Delay amount | Delay amount | Diffusion | - |
| Switch 1 | Reverb type | Reverb type | *(ignored)* | *(ignored)* |
| Switch 2 | Trem type | Trem type | *(ignored)* | Polarity |
| Switch 3 | Delay timing | Delay timing | *(ignored)* | Mono/Stereo |
| FSW 1 Single | Tremolo on/off | Exit tap tempo | Cancel | Cancel |
| FSW 1 Double | Reverb on/off | - | - | - |
| FSW 1 Long | Enter reverb edit | - | - | - |
| FSW 2 Single | Delay on/off | Register tap | Save | Save |
| FSW 2 Double | Enter tap tempo | Register tap | - | - |
| FSW 2 Long | Enter settings edit | - | - | - |
| Both FSW Long | DFU mode | - | - | - |

**LED Indicators:**
- Left LED: Reverb on/off
- Right LED:
  - Normal: Solid (delay only), 40% pulsing (tremolo only), 100% pulsing (both)
  - Tap Tempo: Rhythmic flash at tapped tempo (10% duty cycle), brief flash on each tap

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

### File Structure

```
src/
├── flick.cpp                    # UX orchestrator, audio callback, mode management
├── daisy_hardware.h/cpp         # Hardware abstraction layer (Funbox/HotHouse)
├── parameter_capture.h/cpp      # Soft takeover for edit modes
│
├── delay_effect.h/cpp           # Delay effect (stereo delay with feedback)
│
├── tremolo_effect.h/cpp         # Tremolo base class + 3 algorithms:
│                                #   - SineTremolo (smooth)
│                                #   - SquareTremolo (choppy opto-style)
│                                #   - HarmonicTremolo (band-split + EQ)
│
├── reverb_effect.h              # Reverb base class (polymorphic interface)
├── plate_reverb.h/cpp           # Plate reverb (Dattorro algorithm, all 3 modes)
│
├── flick_oscillator.h/cpp       # LFO/oscillator (used by tremolo effects)
├── flick_filters.hpp            # DSP filter implementations
│
└── PlateauNEVersio/             # Third-party Dattorro implementation
    ├── Dattorro.hpp/cpp         # Plate reverb core (wrapped by PlateReverb)
    ├── dsp/
    │   ├── delays/              # Delay line components
    │   ├── filters/             # Filter components
    │   └── modulation/          # LFO components
    └── utilities/               # Utility functions
```

### Effect Class Hierarchy

All effects use runtime polymorphism (virtual functions) for algorithm switching:

```cpp
// Tremolo: Base class with 3 derived algorithms
class TremoloEffect {
  virtual void ProcessSample(...) = 0;
  virtual void SetSpeed(float hz);
  virtual void SetDepth(float depth);
};
class SineTremolo : public TremoloEffect { /* smooth */ };
class SquareTremolo : public TremoloEffect { /* choppy */ };
class HarmonicTremolo : public TremoloEffect { /* band-split */ };

// Reverb: Single concrete class, 3 modes via parameter sets
class ReverbEffect {
  virtual void ProcessSample(...) = 0;
  virtual void SetDecay(float decay) {}      // Unified edit: Decay
  virtual void SetDiffusion(float d) {}      // Unified edit: Diffusion
  virtual void SetPreDelay(float p) {}       // Unified edit: Pre-delay
  virtual void SetTone(float tone) {}        // Unified edit: Tone (brightness)
  virtual void SetModulation(float mod) {}   // Unified edit: Modulation (shimmer)
};
class PlateReverb : public ReverbEffect { /* Dattorro — used for ambient, plate, and room */ };

// Delay: Single class (no polymorphism needed)
class DelayEffect {
  void ProcessSample(...);
  void SetDelayTime(float samples);
  void SetFeedback(float feedback);
};
```

**Design principles:**
- Effects are **pure DSP** - no knowledge of hardware, knobs, or switches
- **Bypass logic** managed by orchestrator, not effects
- **Parameter scaling** (e.g., tremolo depth × 1.25 for harmonic mode) happens in effect classes
- **Dry/wet mixing** happens in orchestrator (effects return wet-only signal)
- **Polymorphic switching** via base class pointers (`current_tremolo->ProcessSample()`)
- **Algorithm-specific parameters** use virtual methods with no-op defaults

## Key Constants

```cpp
SAMPLE_RATE = 48000.0f
MAX_DELAY = 96000 samples (2 seconds)
TREMOLO_SPEED_MIN = 0.2 Hz
TREMOLO_SPEED_MAX = 16.0 Hz
SETTINGS_VERSION = 19  // Increment on Settings struct change OR when saved values change meaning/defaults
```

## Development Notes

### Adding New Effects

**Creating a new effect module:**
1. Create effect class files (e.g., `chorus_effect.h/cpp`)
2. Inherit from base class if multiple algorithms (or create standalone class)
3. Implement `Init()`, `ProcessSample()`, and parameter setters
4. Keep effect hardware-independent (no knob/switch knowledge)
5. Add source file to [Makefile](src/Makefile) `CPP_SOURCES`

**Integrating into orchestrator:**
1. Include effect header in [flick.cpp](src/flick.cpp)
2. Instantiate effect object(s)
3. Call `Init()` in `main()`
4. Call `ProcessSample()` in audio callback pipeline
5. Map controls to effect parameters (via setters)
6. Handle bypass logic and dry/wet mixing in orchestrator
7. Add bypass state to Settings struct if persistent
8. Update `SETTINGS_VERSION` if Settings struct changes

### Modifying Hardware Abstraction
- Changes to [daisy_hardware.h](src/daisy_hardware.h) affect both platforms
- Use `#if defined(PLATFORM_funbox)` for platform-specific code
- Keep logical switch mapping consistent

### Performance Considerations
- Audio callback runs at 48kHz ÷ 8 samples = 6000 Hz
- Keep callback lean - complex logic outside
- Use SDRAM for large buffers
- Pre-calculate in main loop where possible

### Debugging
- USB Serial: `hw.seed.PrintLine()`
- DFU mode: Hold both footswitches simultaneously for 2 seconds
- Factory reset: Footswitch 2 during boot

## Dependencies (Not Analyzed)

- **libDaisy**: Daisy Seed hardware drivers
- **DaisySP**: DSP library (delay, filters, etc.)
- **PlateauNEVersio**: Third-party Dattorro reverb implementation

These are included as git submodules and documented separately.
