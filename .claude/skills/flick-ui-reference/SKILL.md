---
name: flick-ui-reference
description: Flick pedal UX reference — footswitch gestures, operational modes (reverb edit, device settings, tap tempo), the full knob/switch control-mapping table, LED indicator behavior, factory reset, and on-device debugging. Use when changing or reasoning about pedal controls, mode transitions, or LED feedback.
---

# Flick UX Reference

Controls, modes, and LED behavior for the Flick pedal. Implementation lives in
[flick.cpp](../../../src/flick.cpp); this file is the behavioral spec.

## Operational Modes

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

## Control Mapping

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

## LED Indicators

- Left LED: Reverb on/off
- Right LED:
  - Normal: Solid (delay only), 40% pulsing (tremolo only), 100% pulsing (both)
  - Tap Tempo: Rhythmic flash at tapped tempo (10% duty cycle), brief flash on each tap

## Factory Reset

Initiated by holding Footswitch 2 during boot:
1. LEDs blink alternately
2. Rotate Knob 1: 0% → 100% → 0% → 100% → 0%
3. Each stage increases blink rate
4. Final step restores defaults and starts pedal

## Debugging

- USB Serial: `hw.seed.PrintLine()`
- DFU mode: Hold both footswitches simultaneously for 2 seconds
- Factory reset: Footswitch 2 during boot
