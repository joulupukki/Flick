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

Three reverb algorithms selectable via Toggle Switch 1 in normal mode:

**Plate Reverb** (Dattorro Algorithm)
- Source: [PlateauNEVersio/Dattorro.cpp](src/PlateauNEVersio/Dattorro.cpp)
- Based on Jon Dattorro's 1997 reverb paper
- Uses SDRAM for large delay buffers
- Features:
  - Pre-delay (0-250ms)
  - Input diffusion
  - Tank diffusion (0-100%)
  - High/low-cut filtering
  - LFO modulation (speed, depth, shape)
  - Decay control
- Editable parameters saved to QSPI flash

**Hall Reverb** (Schroeder Algorithm)
- Source: [hall_reverb.h](src/hall_reverb.h) / [hall_reverb.cpp](src/hall_reverb.cpp)
- 4 comb filters with damping
- 2 all-pass filters
- Low-pass filtering for natural decay
- Longer delay times (~50-68ms) for hall character

**Spring Reverb** (Digital Waveguide)
- Source: [spring_reverb.h](src/spring_reverb.h) / [spring_reverb.cpp](src/spring_reverb.cpp)
- Emulates 1960s Fender Deluxe Reverb
- 4 all-pass filters (short delays ~2.5-10ms)
- Main delay for recirculation
- Tap delays for "boing" character
- Pre-delay buffer
- Drive parameter for spring saturation

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
- Multiple waveforms (sine, triangle, saw, square)
- PolyBLEP anti-aliasing for square/saw/triangle
- Rounded square wave for opto tremolo
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
    - Single press: Toggle reverb on/off
    - Double press: Enter tap tempo mode
    - Long press: Enter reverb edit mode
  - Footswitch 2 (Right):
    - Single press: Toggle tremolo on/off
    - Double press: Toggle delay on/off
    - Long press: Enter device settings
  - Both footswitches:
    - Simultaneous long press: Enter DFU (bootloader) mode

**Reverb Edit Mode** (`PEDAL_MODE_EDIT_REVERB`)
- Activated by long-press of Footswitch 1
- Both LEDs flash together
- Uses parameter capture (soft takeover) to prevent sudden jumps
- Knobs control reverb parameters:
  - Knob 2: Pre-delay (scaled 0.25x)
  - Knob 3: Decay
  - Knob 4: Tank diffusion
  - Knob 5: Input high-cut frequency (scaled 10x)
  - Knob 6: Tank high-cut frequency (scaled 10x)
- Toggle switches control modulation (speed/depth/shape):
  - Switch 1: Tank Mod Speed (0.5, 0.25, 0.1 → scaled by `PLATE_TANK_MOD_SPEED_SCALE` when applied)
  - Switch 2: Tank Mod Depth (0.5, 0.25, 0.1 → scaled by `PLATE_TANK_MOD_DEPTH_SCALE` when applied)
  - Switch 3: Tank Mod Shape (0.5, 0.25, 0.1)
- Footswitch 1: Cancel (restore previous)
- Footswitch 2: Save to flash

**Device Settings** (`PEDAL_MODE_EDIT_DEVICE_SETTINGS`)
- Activated by long-press of Footswitch 2
- LEDs flash alternately
- Toggle Switch 1 selects reverb wet/dry mode:
  - RIGHT/UP: All Dry (100% dry, 0-100% wet)
  - MIDDLE: Dry/Wet Mix
  - LEFT/DOWN: All Wet (0% dry, 0-100% wet)
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
- Activated by double-press of Footswitch 1
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
struct Settings {
  int version;              // SETTINGS_VERSION for migration
  float decay;              // Reverb decay
  float diffusion;          // Tank diffusion
  float input_cutoff_freq;  // Input high-cut
  float tank_cutoff_freq;   // Tank high-cut
  int tank_mod_speed_pos;   // LFO speed switch position
  int tank_mod_depth_pos;   // LFO depth switch position
  int tank_mod_shape_pos;   // LFO shape switch position
  float pre_delay;          // Pre-delay amount
  int mono_stereo_mode;     // I/O routing mode
  int polarity_mode;        // Phase inversion mode
  int reverb_knob_mode;     // Reverb wet/dry knob behavior
  bool bypass_reverb;       // Reverb bypass state
  bool bypass_tremolo;      // Tremolo bypass state
  bool bypass_delay;        // Delay bypass state
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
- Reverbs: `hall_reverb.cpp`, `spring_reverb.cpp`
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
- Plate reverb buffers (50 InterpDelay buffers)

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
| Knob 4  | Delay time | Delay time (frozen) | Diffusion | - |
| Knob 5  | Delay feedback | Delay feedback | Input cut | - |
| Knob 6  | Delay amount | Delay amount | Tank cut | - |
| Switch 1 | Reverb type | Reverb type | Mod speed | Reverb wet/dry |
| Switch 2 | Trem type | Trem type | Mod depth | Polarity |
| Switch 3 | Delay timing | Delay timing | Mod shape | Mono/Stereo |
| FSW 1 Single | Reverb on/off | Exit tap tempo | Cancel | Cancel |
| FSW 1 Double | Enter tap tempo | - | - | - |
| FSW 1 Long | Enter reverb edit | - | - | - |
| FSW 2 Single | Tremolo on/off | Register tap | Save | Save |
| FSW 2 Double | Delay on/off | - | - | - |
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

```
src/
├── flick.cpp                    # Main program, audio callback
├── daisy_hardware.h/cpp         # Hardware abstraction layer
├── parameter_capture.h          # Soft takeover for edit modes
├── flick_oscillator.h/cpp       # LFO/oscillator for tremolo
├── flick_filters.hpp            # DSP filter implementations
├── hall_reverb.h/cpp            # Schroeder hall reverb
├── spring_reverb.h/cpp          # Spring reverb emulation
└── PlateauNEVersio/
    ├── Dattorro.hpp/cpp         # Plate reverb (Dattorro)
    ├── dsp/
    │   ├── delays/              # Delay line components
    │   ├── filters/             # Filter components
    │   └── modulation/          # LFO components
    └── utilities/               # Utility functions
```

## Key Constants

```cpp
SAMPLE_RATE = 48000.0f
MAX_DELAY = 96000 samples (2 seconds)
TREMOLO_SPEED_MIN = 0.2 Hz
TREMOLO_SPEED_MAX = 16.0 Hz
PLATE_TANK_MOD_SPEED_SCALE = 8.0f   // Dattorro reverb speed scaling
PLATE_TANK_MOD_DEPTH_SCALE = 15.0f  // Dattorro reverb depth scaling
SETTINGS_VERSION = 7  // Increment on Settings struct change
```

## Development Notes

### Adding New Effects
1. Include effect class in [flick.cpp](src/flick.cpp)
2. Add to audio callback pipeline
3. Map controls in appropriate mode
4. Add bypass state to Settings if needed
5. Update SETTINGS_VERSION

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
