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
| KNOB 2 | Tremolo Speed | When using tap tempo, this knob is locked until moved >5% |
| KNOB 3 | Tremolo Depth |  |
| KNOB 4 | Delay Time | When using tap tempo, this knob is locked until moved >5% |
| KNOB 5 | Delay Feedback |  |
| KNOB 6 | Delay Dry/Wet Amount |  |
| SWITCH 1 | Reverb knob function | **LOW** - 100% Dry, 0-100% Wet<br/>**MID** - Dry/Wet Mix<br/>**HIGH** - 0% Dry, 0-100% Wet |
| SWITCH 2 | Tremolo Type | **LOW** - Smooth (Sine)<br/>**MID** - Harmonic<br/>**HIGH** - Opto (Square) |
| SWITCH 3 | Delay Subdivision | **LOW** - Quarter Note Triplet (⅔x)<br/>**MID** - Normal (1x)<br/>**HIGH** - Dotted Eighth (¾x) |
| FOOTSWITCH 1 | Reverb On/Off | Normal press toggles reverb on/off.<br/>**Double press enters Tap Tempo mode.**<br/>Long press for Reverb Edit mode (see below). |
| FOOTSWITCH 2 | Delay/Tremolo On/Off | Normal press toggles delay.<br/>Double press toggles tremolo.<br/><br/>**LED:**<br/>- 100% when only delay is active<br/>- 40% pulsing when only tremolo is active<br/>- 100% pulsing when both are active<br/>Long press for Persistent Settings Edit mode (see below). |

### Controls (Tap Tempo Mode)
*LED 1 pulses slowly. LED 2 blinks at the current tempo (if set).*

Double-press Footswitch 1 to enter Tap Tempo mode. The pedal determines which effects to control based on what's currently active:
- If **only delay** is active: tap tempo controls delay time only
- If **only tremolo** is active: tap tempo controls tremolo speed only  
- If **both** or **neither** are active: tap tempo controls both

| CONTROL | DESCRIPTION | NOTES |
|-|-|-|
| FOOTSWITCH 1 | Exit Tap Tempo | Returns to normal mode |
| FOOTSWITCH 2 | Tap Tempo | Tap to set tempo. Delay time and/or tremolo speed are set based on the interval between taps. Delay subdivision (SWITCH 3) is applied. |
| SWITCH 3 | Delay Subdivision | Works the same as normal mode - applied to tapped tempo |

Tap tempo mode automatically exits after 5 seconds of inactivity.

When returning to normal mode, the knobs remain "locked" at the tap tempo value until you move them more than 5%, preventing accidental parameter jumps.

### Controls (Reverb Edit Mode)
*Both LEDs flash together when in edit mode. Enter by long-pressing Footswitch 1.*

**Soft Takeover:** When entering edit mode, all knobs and switches are "captured" at their current positions. Parameters only change when you actually move a control, preventing accidental jumps.

| CONTROL | DESCRIPTION | NOTES |
|-|-|-|
| KNOB 1 | Reverb Amount (Wet) | Not saved. Just here for preview convenience. |
| KNOB 2 | Pre Delay | 0 for Off, up to 0.25 seconds |
| KNOB 3 | Decay |  |
| KNOB 4 | Tank Diffusion |  |
| KNOB 5 | Input High Cutoff Frequency |  |
| KNOB 6 | Tank High Cutoff Frequency |  |
| SWITCH 1 | Tank Mod Speed | **LOW** - Low<br/>**MID** - Medium<br/>**HIGH** - High |
| SWITCH 2 | Tank Mod Depth | **LOW** - Low<br/>**MID** - Medium<br/>**HIGH** - High |
| SWITCH 3 | Tank Mod Shape | **LOW** - Low<br/>**MID** - Medium<br/>**HIGH** - High |
| FOOTSWITCH 1 | **CANCEL** & Exit | Discards parameter changes and exits Reverb Edit Mode. |
| FOOTSWITCH 2 | **SAVE** & Exit | Saves all parameters and exits Reverb Edit Mode. |

### Controls (Persistent Settings Edit Mode)
*Both LEDs flash alternately when in persistent settings edit mode. Enter by long-pressing Footswitch 2.*

| CONTROL | DESCRIPTION | NOTES |
|-|-|-|
| SWITCH 2 | Trem & Delay Makeup Gain | **LOW** - None<br/>**MID** - Normal<br/>**HIGH** - Heavy (Plus) |
| SWITCH 3 | Mono-Stereo Mode | **LOW** - Mono In, Mono Out<br/>**MID** - Mono In, Stereo Out<br/>**HIGH** - Stereo In, Stereo Out |
| FOOTSWITCH 1 | **CANCEL** & Exit | Discards changes and exits. |
| FOOTSWITCH 2 | **SAVE** & Exit | Saves makeup gain and mono-stereo settings. |

### Persisted Settings

The following settings are saved to flash memory and restored when the pedal powers on:

- **Reverb parameters** - All reverb edit mode settings (decay, diffusion, etc.)
- **Mono/Stereo mode** - Input/output configuration
- **Makeup gain** - Tremolo and delay makeup gain level
- **Bypass states** - On/off state for reverb, tremolo, and delay

This means the pedal will power up in the same state you left it.

### Factory Reset (Restore default reverb parameters)

To enter factory reset mode, **press and hold** **Footswitch #2** when powering the pedal. The LED lights will alternatively blink slowly.

1. Rotate Knob #1 to 100%. The LEDs will quickly flash simultaneously and start blinking faster.
2. Rotate Knob #1 to 0%. The LEDs will quickly flash simultaneously and start blinking faster.
3. Rotate Knob #1 to 100%. The LEDs will quickly flash simultaneously and start blinking faster.
4. Rotate Knob #1 to 0%. The LEDs will quickly flash simultaneously, defaults will be restored, and the pedal will resume normal pedal mode.

To exit factory reset mode without resetting. Power off the pedal and power it back on.

### Enter Program DFU Mode

There are two ways to enter DFU (firmware programming) mode:

1. **Long-press Footswitch 1** (in normal mode) - Hold for 2 seconds. The lights will alternately flash quickly when DFU mode is entered.

2. **Hold both Footswitches** - Press and hold both footswitches for 5 seconds. This works in any mode and is useful if you need to reflash the firmware.

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