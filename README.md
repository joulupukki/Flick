# Flick

The Flick is a reverb, tremolo, and delay pedal. The original goal of this pedal was to replace the Strymon Flint (Reverb and Tremolo) on a small pedal board and also include delay.

### Effects

**Platerra Reverb:** This is a plate reverb based on the Dattorro reverb.

**Hall Reverb:** An 8-line feedback delay network with Hadamard mixing for dense, spacious hall sounds.

**Spring Reverb:** Uses impulse response (IR) convolution to faithfully reproduce the sound of a real spring reverb tank. The IR is processed through a partitioned overlap-save FFT convolution engine adapted from [MuleBox](https://github.com/optilude/MuleBox), which keeps the CPU cost manageable on the STM32H750.

**Tremolo:** Tremolo with smooth sine wave, harmonic tremolo, and square wave (opto-like) settings.

**Delay:** Basic digital delay.

### Demo

Feature demo video (28 June 2025):

[![Demo Video](https://img.youtube.com/vi/pWW68mqj2iQ/0.jpg)](https://www.youtube.com/watch?v=pWW68mqj2iQ)

### Controls (Normal Mode)

| CONTROL | DESCRIPTION | NOTES |
|-|-|-|
| KNOB 1 | Reverb Dry/Wet Amount |  |
| KNOB 2 | Tremolo Speed |  |
| KNOB 3 | Tremolo Depth |  |
| KNOB 4 | Delay Time |  |
| KNOB 5 | Delay Feedback |  |
| KNOB 6 | Delay Dry/Wet Amount |  |
| SWITCH 1 | Reverb Type | **LOW** - Hall<br/>**MID** - Plate<br/>**HIGH** - Spring |
| SWITCH 2 | Tremolo Type | **LOW** - Smooth<br/>**MID** - Harmonic<br/>**HIGH** - Opto |
| SWITCH 3 | Delay Timing | **LOW** - Triplet (1/3)<br/>**MID** - Quarter (straight)<br/>**HIGH** - Dotted Eighth (3/4) |
| FOOTSWITCH 1 | Reverb On/Off | Normal press toggles reverb on/off.<br/>Double press enters Tap Tempo mode (see below).<br/>Long press toggles reverb edit mode (see below). |
| FOOTSWITCH 2 | Tremolo/Delay On/Off | Normal press toggles tremolo.<br/>Double press toggles delay.<br/><br/>**LED:**<br/>- 100% when only delay is active<br/>- 40% pulsing when only tremolo is active<br/>- 100% pulsing when both are active<br/>Long press for Device Settings (see below). |

### Controls (Tap Tempo Mode)
*Right LED flashes at tapped tempo. Left LED shows reverb status as normal.*

Entering Tap Tempo mode automatically enables delay if it is currently off.

| CONTROL | DESCRIPTION | NOTES |
|-|-|-|
| KNOB 4 | Delay Time | Ignored until physically rotated. Overrides tapped tempo when moved. |
| SWITCH 3 | Delay Timing | Still applies timing subdivision to tapped tempo. |
| FOOTSWITCH 1 | Exit Tap Tempo | Immediately returns to Normal Mode. |
| FOOTSWITCH 2 | Register Tap | Each press registers a tap. LED flashes briefly on each tap.<br/>Tempo is averaged from the last 3 taps.<br/>Auto-exits after 4 seconds of no taps. |

### Controls (Reverb Edit Mode)
*Both LEDs flash when in Reverb Edit Mode.*

**Parameter Capture:** When entering Reverb Edit Mode, all knobs and switches freeze at their current values. To prevent sudden parameter jumps, each control remains frozen until you move it beyond a threshold (5% for knobs, any position change for switches). This allows you to adjust controls smoothly without parameter jumps when switching between normal and edit modes.

| CONTROL | DESCRIPTION | NOTES |
|-|-|-|
| KNOB 1 | Reverb Amount (Wet) | Not saved. Just here for convenience. |
| KNOB 2 | Pre Delay | 0 for Off, up to 0.25<br/>**Frozen until moved >5%** |
| KNOB 3 | Decay | **Frozen until moved >5%** |
| KNOB 4 | Tank Diffusion | **Frozen until moved >5%** |
| KNOB 5 | Input High Cutoff Frequency | **Frozen until moved >5%** |
| KNOB 6 | Tank High Cutoff Frequency | **Frozen until moved >5%** |
| SWITCH 1 | Tank Mod Speed | **LOW** - Low<br/>**MID** - Medium<br/>**HIGH** - High<br/>**Frozen until position changes** |
| SWITCH 2 | Tank Mod Depth | **LOW** - Low<br/>**MID** - Medium<br/>**HIGH** - High<br/>**Frozen until position changes** |
| SWITCH 3 | Tank Mod Shape | **LOW** - Low<br/>**MID** - Medium<br/>**HIGH** - High<br/>**Frozen until position changes** |
| FOOTSWITCH 1 | **CANCEL** & Exit | Discards parameter changes and exits Reverb Edit Mode. |
| FOOTSWITCH 2 | **SAVE** & Exit | Saves all parameters and exits Reverb Edit Mode. |

### Controls (Settings Edit Mode)
*Both LEDs flash alternatively when in Settings Edit Mode.*

**Parameter Capture:** When entering Settings Edit Mode, the switches freeze at their current values. You must actively move a switch to get a change to happen. This helps prevent unexpected changes when jumping between normal and edit mode.

| CONTROL | DESCRIPTION | NOTES |
|-|-|-|
| SWITCH 1 | Reverb Knob Function | **LOW** - 0% Dry, 0-100% Wet<br/>**MID** - Dry/Wet Mix<br/>**HIGH** - 100% Dry, 0-100% Wet |
| SWITCH 2 | Polarity | **LOW** - Invert Right<br/>**MID** - Normal<br/>**HIGH** - Invert Left |
| SWITCH 3 | Mono/Stereo Mode | **LOW** - Mono in, Mono Out<br/>**MID** - Mono in, Stereo Out<br/>**HIGH** - Stereo In, Stereo Out |
| FOOTSWITCH 1 | **CANCEL** & Exit | Discards parameter changes and exits Settings Edit Mode. |
| FOOTSWITCH 2 | **SAVE** & Exit | Saves all parameters and exits Settings Edit Mode. |

### Factory Reset (Restore default reverb parameters)

To enter factory reset mode, **press and hold** **Footswitch #2** when powering the pedal. The LED lights will alternatively blink slowly.

1. Rotate Knob #1 to 100%. The LEDs will quickly flash simultaneously and start blinking faster.
2. Rotate Knob #1 to 0%. The LEDs will quickly flash simultaneously and start blinking faster.
3. Rotate Knob #1 to 100%. The LEDs will quickly flash simultaneously and start blinking faster.
4. Rotate Knob #1 to 0%. The LEDs will quickly flash simultaneously, defaults will be restored, and the pedal will resume normal pedal mode.

To exit factory reset mode without resetting. Power off the pedal and power it back on.

### Enter Program DFU Mode

Press and hold **both footswitches simultaneously** for 5 seconds to enter Program DFU mode. The lights will alternately flash 3 times when DFU mode is entered.

## Building the Firmware

### Prerequisites

- [Daisy Toolchain](https://github.com/electro-smith/DaisyWiki/wiki/1.-Setting-Up-Your-Development-Environment) (ARM GCC, Make)
- Python 3 (for IR header generation)
- SoX (for WAV resampling, only needed when changing the IR)

### Initial Setup

```bash
# Clone the repository
git clone https://github.com/joulupukki/Flick.git
cd Flick

# Initialize and set up submodules
git submodule update --init --recursive

# Build the daisy libraries:
#
# IMPORTANT: If you are planning to build this for FunBox, replace the
# daisy_petal files in `libDaisy/src` with the files in the
# `platforms/funbox/required_daisy_mods/` directory to properly map
# controls on Funbox.

make -C libDaisy OPT=-Os
make -C DaisySP
```

### Build

```bash
cd src

# Build for FunBox (default)
make

# Build for Hothouse
make PLATFORM=hothouse
```

### First-Time Bootloader Setup

Flick uses the Daisy bootloader (`BOOT_SRAM` mode) so that the firmware
and IR data can be stored in the 8 MB QSPI flash. The bootloader must
be flashed once per device.

**Option A - USB only:**
1. Connect Daisy Seed via USB
2. Enter STM DFU mode: hold the BOOT button on the Daisy Seed and press RESET, then release both
3. Run `make program-boot`

**Option B - Debug probe (no buttons needed):**
1. Connect an STLINK debug probe
2. Run `make program-boot-probe`

### Flashing Firmware

**Option A - USB only:**
1. Enter Daisy bootloader: press RESET on the Daisy Seed (or hold both footswitches for 2 seconds)
2. Within 2.5 seconds, run `make program-dfu` (or `make flash`)

**Option B - Debug probe (no USB or buttons needed):**
1. Connect STLINK debug probe (power the pedal via 9V or USB)
2. Run `make program`

### Changing the Spring Reverb IR

The spring reverb uses a pre-recorded impulse response. To use a
different IR:

1. Place your `.wav` file in `src/irs/`. It should be mono, 48 kHz. If
   it is a different sample rate, resample it first:
   ```bash
   sox input.wav -r 48000 src/irs/spring_reverb_48k.wav
   ```

2. Regenerate the IR data header:
   ```bash
   cd src
   make update-irs
   ```
   This runs `tools/wav_to_ir_header.py` which trims the IR to 750 ms,
   normalizes the frequency response, and generates `src/ir_data.h`.

3. Rebuild and flash:
   ```bash
   make clean && make
   make flash   # or make program
   ```

**IR length limit:** The IR data is stored in SRAM (copied from QSPI at
boot). With the current firmware size, the maximum practical IR length
is about 750 ms. Longer IRs can be used by reducing other code or
by placing the IR data in a QSPI-resident section (requires custom
binary generation).

## Spring Reverb: Technical Approach

The spring reverb uses **partitioned overlap-save FFT convolution** to
apply a real spring reverb impulse response in real time. This is the
same approach used in the [MuleBox](https://github.com/optilude/MuleBox)
cabinet simulator project.

### How it works

1. A spring reverb impulse response (captured from a real spring tank)
   is converted to a C++ header file at build time by
   `tools/wav_to_ir_header.py`.

2. At firmware startup, the IR is split into 128-sample partitions and
   each partition is pre-FFT'd into SDRAM buffers.

3. During audio processing, each 128-sample input block is FFT'd and
   multiplied with all IR partition FFTs in the frequency domain. The
   results are accumulated and inverse-FFT'd to produce the convolved
   output.

4. This approach reduces the CPU cost from ~122% (direct FIR) to
   ~30-40% for a 750 ms IR, leaving headroom for the delay and tremolo
   effects.

### Key constraints

- **Audio block size is 128 samples** (2.67 ms at 48 kHz). This is
  required by the convolution engine's FFT partition size. The latency
  is imperceptible for guitar effects.

- **SRAM budget** limits the IR length. The firmware, IR data, and
  runtime state must all fit within 480 KB of SRAM.

- **SDRAM** is used for the convolution engine's working buffers
  (pre-computed IR FFTs and frequency-domain delay line).
