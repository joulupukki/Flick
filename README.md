# Flick

The Flick is a reverb, tremolo, and delay pedal. The original goal of this pedal was to replace the Strymon Flint (Reverb and Tremolo) on a small pedal board and also include delay.

### Effects

**Reverb:** Three reverb algorithms selectable via toggle switch:
- **Ambient** (CloudSeed algorithm, Rubi-Ka Fields preset) - spacious, lush, long ambient tail
- **Plate** (Dattorro algorithm) - classic plate reverb with modulation
- **Room** (CloudSeed algorithm, Small Room preset) - room/hall reverb with prominent early reflections

All three reverb types have editable parameters (pre-delay, decay, tone, modulation, diffusion) via Reverb Edit Mode, with separate saved settings per reverb type.

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
| SWITCH 1 | Reverb Type | **LOW** - Room<br/>**MID** - Plate<br/>**HIGH** - Ambient |
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

Edits the **currently selected** reverb type (Ambient, Plate, or Room). The reverb type is locked when entering edit mode — toggle switch 1 changes are ignored. Each reverb type has its own saved parameter set.

**Parameter Capture:** When entering Reverb Edit Mode, all knobs freeze at their current values. To prevent sudden parameter jumps, each knob remains frozen until you move it beyond a 5% threshold. This allows you to adjust controls smoothly without parameter jumps when switching between normal and edit modes.

| CONTROL | DESCRIPTION | NOTES |
|-|-|-|
| KNOB 1 | Reverb Amount (Wet) | Not saved. Just here for convenience. |
| KNOB 2 | Pre-delay | Delay before reverb starts<br/>**Frozen until moved >5%** |
| KNOB 3 | Decay | Reverb tail length<br/>**Frozen until moved >5%** |
| KNOB 4 | Tone | Brightness of the reverb tail (dark to bright)<br/>**Frozen until moved >5%** |
| KNOB 5 | Modulation | Movement/shimmer in the reverb<br/>**Frozen until moved >5%** |
| KNOB 6 | Diffusion | Density/smearing of the reverb tail<br/>**Frozen until moved >5%** |
| SWITCHES | *(ignored)* | Toggle switches have no function in edit mode |
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

### Factory Reset (Restore default settings for all reverb types)

To enter factory reset mode, **press and hold** **Footswitch #2** when powering the pedal. The LED lights will alternatively blink slowly.

1. Rotate Knob #1 to 100%. The LEDs will quickly flash simultaneously and start blinking faster.
2. Rotate Knob #1 to 0%. The LEDs will quickly flash simultaneously and start blinking faster.
3. Rotate Knob #1 to 100%. The LEDs will quickly flash simultaneously and start blinking faster.
4. Rotate Knob #1 to 0%. The LEDs will quickly flash simultaneously, defaults will be restored, and the pedal will resume normal pedal mode.

To exit factory reset mode without resetting. Power off the pedal and power it back on.

### Enter Program DFU Mode

Press and hold **both footswitches simultaneously** for 5 seconds to enter Program DFU mode. The lights will alternately flash 3 times when DFU mode is entered.

### Build the Software

```
# Clone the repository
$ git clone https://github.com/joulupukki/Flick.git
$ cd Flick

# Initialize and set up submodules
$ git submodule update --init --recursive

# Build the daisy libraries (after installing the Daisy Toolchain):
#
# IMPORTANT: If you are planning to build this for FunBox, replace the daisy_petal files in `libDaisy/src` with the files in the `platforms/funbox/required_daisy_mods/` directory to properly map controls on Funbox.

$ make -C libDaisy
$ make -C DaisySP

# Build the Flick pedal firmware
$ cd src

# Build for FunBox
$ make

# Build for Hothouse
$ make PLATFORM=hothouse
```

If you have an ST-Link, you can install the software easily like this:
```
$ make program
```

If you only have USB, you'll need to put the Flick into DFU mode first and with it connected with a USB cable, you can then install the firmware by running:
```
$ make program-dfu
```