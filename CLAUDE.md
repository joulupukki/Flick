# Flick - Digital Guitar Pedal

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
- DFU bootloader entry via long-press

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

Three reverb algorithms selectable via DIP switches (Funbox only) or compile-time default:

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
- Delay time: 50ms to 2 seconds (logarithmic)
- Feedback: 0-100%
- Dry/wet mix: 0-100%
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

### Operational Modes

[flick.cpp](src/flick.cpp:86-103)

**Normal Mode** (`PEDAL_MODE_NORMAL`)
- Standard pedal operation
- Controls mapped to effect parameters
- Footswitches toggle effects on/off

**Reverb Edit Mode** (`PEDAL_MODE_EDIT_REVERB`)
- Activated by double-press of Footswitch 1
- Both LEDs flash together
- Knobs control reverb parameters:
  - Knob 2: Pre-delay
  - Knob 3: Decay
  - Knob 4: Tank diffusion
  - Knob 5: Input high-cut frequency
  - Knob 6: Tank high-cut frequency
- Toggle switches control modulation (speed/depth/shape)
- Footswitch 1: Cancel (restore previous)
- Footswitch 2: Save to flash

**Mono-Stereo Edit Mode** (`PEDAL_MODE_EDIT_MONO_STEREO`)
- Activated by long-press of Footswitch 2
- LEDs flash alternately
- Toggle Switch 3 selects mode:
  - LEFT: Mono In, Mono Out (MIMO)
  - MIDDLE: Mono In, Stereo Out (MISO)
  - RIGHT: Stereo In, Stereo Out (SISO)
- Footswitch 1: Cancel
- Footswitch 2: Save to flash

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
  float tank_mod_speed;     // LFO speed
  float tank_mod_depth;     // LFO depth
  float tank_mod_shape;     // LFO shape
  float pre_delay;          // Pre-delay amount
  int mono_stereo_mode;     // I/O routing mode
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

| Control | Normal Mode | Reverb Edit | Mono-Stereo Edit |
|---------|-------------|-------------|------------------|
| Knob 1  | Reverb amount | Reverb amount | - |
| Knob 2  | Trem speed | Pre-delay | - |
| Knob 3  | Trem depth | Decay | - |
| Knob 4  | Delay time | Diffusion | - |
| Knob 5  | Delay feedback | Input cut | - |
| Knob 6  | Delay amount | Tank cut | - |
| Switch 1 | Reverb mode | Mod speed | - |
| Switch 2 | Trem type | Mod depth | - |
| Switch 3 | Makeup gain | Mod shape | Mono/Stereo |
| FSW 1 | Reverb on/off | Cancel | Cancel |
| FSW 2 | Delay on/off | Save | Save |

**LED Indicators:**
- Left LED: Reverb on/off
- Right LED:
  - Solid: Delay only
  - 40% pulsing: Tremolo only
  - 100% pulsing: Both active

## Platform Differences

### Pin Mapping

Platform-specific via preprocessor:
- Switch positions read differently
- Funbox has DIP switches for reverb type selection
- Logical position mapping unifies both platforms

### Switch Orientation

- **Funbox**: Horizontal (LEFT/MIDDLE/RIGHT)
- **HotHouse**: Vertical (DOWN/MIDDLE/UP)

Abstraction layer maps both to (RIGHT/MIDDLE/LEFT) = (0/1/2)

## Code Organization

```
src/
├── flick.cpp                    # Main program, audio callback
├── daisy_hardware.h/cpp         # Hardware abstraction layer
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
SETTINGS_VERSION = 4  // Increment on Settings struct change
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
- DFU mode: Hold Footswitch 1 for 2 seconds
- Factory reset: Footswitch 2 during boot

## Dependencies (Not Analyzed)

- **libDaisy**: Daisy Seed hardware drivers
- **DaisySP**: DSP library (delay, filters, etc.)
- **PlateauNEVersio**: Third-party Dattorro reverb implementation

These are included as git submodules and documented separately.
