# Flick

The Flick is a reverb, tremolo, and delay pedal. The original goal of this pedal was to replace the Strymon Flint (Reverb and Tremolo) on a small pedal board and also include delay.

### Effects

**Platerra Reverb:** This is a plate reverb based on the Dattorro reverb.

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
| SWITCH 1 | Reverb knob funcion | **LOW** - 100% Dry, 0-100% Wet<br/>**MID** - Dry/Wet Mix<br/>**HIGH** - 0% Dry, 0-100% Wet |
| SWITCH 2 | Tremolo Type | **LOW** - Smooth<br/>**MID** - Harmonic<br/>**HIGH** - Opto |
| SWITCH 3 | Trem & Delay Makeup Gain | **LOW** - None<br/>**MID** - Normal<br/>**HIGH** - Plus |
| FOOTSWITCH 1 | Reverb On/Off | Normal press toggles reverb on/off.<br/>Double press toggles reverb edit mode (see below).<br/>Long press for DFU mode. |
| FOOTSWITCH 2 | Delay/Tremolo On/Off | Normal press toggles delay.<br/>Double press toggles tremolo.<br/><br/>**LED:**<br/>- 100% when only relay is active<br/>- 40% pulsing when only tremolo is active<br/>- 100% pulsing when both are active<br/>Long press for Mono-Stereo Edit mode (see below). |

### Controls (Reverb Edit Mode)
*Both LEDs flash when in edit mode.*

**Parameter Capture:** When entering reverb edit mode, all knobs and switches freeze at their current values. To prevent sudden parameter jumps, each control remains frozen until you move it beyond a threshold (5% for knobs, any position change for switches). This allows you to adjust controls smoothly without parameter jumps when switching between normal and edit modes.

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
| FOOTSWITCH 1 | **CANCEL** & Exit | Discards parameter changes and exits Reverb Edit Mode.<br/>Long press for DFU mode. |
| FOOTSWITCH 2 | **SAVE** & Exit | Saves all parameters and exits Reverb Edit Mode. |

### Controls (Mono-Stereo Edit Mode)
*Both LEDs flash alternatively when in Mono-Stereo Edit mode.*

| CONTROL | DESCRIPTION | NOTES |
|-|-|-|
| SWITCH 3 | Mono-Stereo Mode | **LOW** - Mono in, Mono Out<br/>**MID** - Mono in, Stereo Out<br/>**HIGH** - Stereo In, Stereo Out |
| FOOTSWITCH 1 | **CANCEL** & Exit | Discards parameter changes and exits Mono-Stereo Edit Mode.<br/>Long press for DFU mode. |
| FOOTSWITCH 2 | **SAVE** & Exit | Saves all parameters and exits Mono-Stereo Edit Mode. |

### Factory Reset (Restore default reverb parameters)

To enter factory reset mode, **press and hold** **Footswitch #2** when powering the pedal. The LED lights will alternatively blink slowly.

1. Rotate Knob #1 to 100%. The LEDs will quickly flash simultaneously and start blinking faster.
2. Rotate Knob #1 to 0%. The LEDs will quickly flash simultaneously and start blinking faster.
3. Rotate Knob #1 to 100%. The LEDs will quickly flash simultaneously and start blinking faster.
4. Rotate Knob #1 to 0%. The LEDs will quickly flash simultaneously, defaults will be restored, and the pedal will resume normal pedal mode.

To exit factory reset mode without resetting. Power off the pedal and power it back on.

### Enter Program DFU Mode

Press and hold Footswitch 1 to enter Program DFU mode. The lights will alternately flash quickly when DFU mode is entered.

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