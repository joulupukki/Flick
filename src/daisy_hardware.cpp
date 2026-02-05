// Unified hardware proxy for Funbox and Hothouse DIY DSP Platforms
// Copyright (C) 2024  Cleveland Music Co.  <code@clevelandmusicco.com>
// Copyright (C) 2025-2026  Boyd Timothy <btimothy@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "daisy_hardware.h"

#include "optional"

using flick::DaisyHardware;
using daisy::System;

#ifndef SAMPLE_RATE
// #define SAMPLE_RATE DSY_AUDIO_SAMPLE_RATE
#define SAMPLE_RATE 48014.f
#endif

// Hardware related defines.
// Switches
#if defined(PLATFORM_funbox)
constexpr Pin PIN_SW_1_LEFT = daisy::seed::D14;
constexpr Pin PIN_SW_1_RIGHT = daisy::seed::D13;
constexpr Pin PIN_SW_2_LEFT = daisy::seed::D7;
constexpr Pin PIN_SW_2_RIGHT = daisy::seed::D10;
constexpr Pin PIN_SW_3_LEFT = daisy::seed::D2;
constexpr Pin PIN_SW_3_RIGHT = daisy::seed::D4;

constexpr Pin PIN_DIP_SW_1 = daisy::seed::D1;
constexpr Pin PIN_DIP_SW_2 = daisy::seed::D3;
constexpr Pin PIN_DIP_SW_3 = daisy::seed::D5;
constexpr Pin PIN_DIP_SW_4 = daisy::seed::D6;

constexpr Pin PIN_FSW_1 = daisy::seed::D25;
constexpr Pin PIN_FSW_2 = daisy::seed::D26;
#else
constexpr Pin PIN_SW_1_UP = daisy::seed::D9;
constexpr Pin PIN_SW_1_DOWN = daisy::seed::D10;
constexpr Pin PIN_SW_2_UP = daisy::seed::D7;
constexpr Pin PIN_SW_2_DOWN = daisy::seed::D8;
constexpr Pin PIN_SW_3_UP = daisy::seed::D5;
constexpr Pin PIN_SW_3_DOWN = daisy::seed::D6;
constexpr Pin PIN_FSW_1 = daisy::seed::D25;
constexpr Pin PIN_FSW_2 = daisy::seed::D26;
#endif

// Knobs
constexpr Pin PIN_KNOB_1 = daisy::seed::D16;
constexpr Pin PIN_KNOB_2 = daisy::seed::D17;
constexpr Pin PIN_KNOB_3 = daisy::seed::D18;
constexpr Pin PIN_KNOB_4 = daisy::seed::D19;
constexpr Pin PIN_KNOB_5 = daisy::seed::D20;
constexpr Pin PIN_KNOB_6 = daisy::seed::D21;

const uint32_t DaisyHardware::HOLD_THRESHOLD_MS;

void DaisyHardware::Init(bool boost) {
  // Initialize the hardware.
  seed.Configure();
  seed.Init(boost);
  InitSwitches();
  InitAnalogControls();
  SetAudioBlockSize(48);
}

void DaisyHardware::DelayMs(size_t del) { seed.DelayMs(del); }

void DaisyHardware::SetHidUpdateRates() {
  for (size_t i = 0; i < KNOB_LAST; i++) {
    knobs[i].SetSampleRate(AudioCallbackRate());
  }
}

void DaisyHardware::StartAudio(AudioHandle::InterleavingAudioCallback cb) {
  seed.StartAudio(cb);
}

void DaisyHardware::StartAudio(AudioHandle::AudioCallback cb) {
  seed.StartAudio(cb);
}

void DaisyHardware::ChangeAudioCallback(AudioHandle::InterleavingAudioCallback cb) {
  seed.ChangeAudioCallback(cb);
}

void DaisyHardware::ChangeAudioCallback(AudioHandle::AudioCallback cb) {
  seed.ChangeAudioCallback(cb);
}

void DaisyHardware::StopAudio() { seed.StopAudio(); }

void DaisyHardware::SetAudioBlockSize(size_t size) {
  seed.SetAudioBlockSize(size);
  SetHidUpdateRates();
}

size_t DaisyHardware::AudioBlockSize() { return seed.AudioBlockSize(); }

void DaisyHardware::SetAudioSampleRate(SaiHandle::Config::SampleRate samplerate) {
  seed.SetAudioSampleRate(samplerate);
  SetHidUpdateRates();
}

float DaisyHardware::AudioSampleRate() { return seed.AudioSampleRate(); }

float DaisyHardware::AudioCallbackRate() { return seed.AudioCallbackRate(); }

void DaisyHardware::StartAdc() { seed.adc.Start(); }

void DaisyHardware::StopAdc() { seed.adc.Stop(); }

void DaisyHardware::ProcessAnalogControls() {
  for (size_t i = 0; i < KNOB_LAST; i++) {
    knobs[i].Process();
  }
}

float DaisyHardware::GetKnobValue(Knob k) {
  size_t idx;
  idx = k < KNOB_LAST ? k : KNOB_1;
  return knobs[idx].Value();
}

void DaisyHardware::ProcessDigitalControls() {
  for (size_t i = 0; i < SWITCH_LAST; i++) {
    switches[i].Debounce();
  }
  ProcessFootswitchPresses(FOOTSWITCH_1);
  ProcessFootswitchPresses(FOOTSWITCH_2);
}

void DaisyHardware::InitSwitches() {
#if defined(PLATFORM_funbox)
  constexpr Pin pin_numbers[SWITCH_LAST] = {
      PIN_SW_1_LEFT, PIN_SW_1_RIGHT, PIN_SW_2_LEFT, PIN_SW_2_RIGHT,
      PIN_SW_3_LEFT, PIN_SW_3_RIGHT, PIN_FSW_1,   PIN_FSW_2,
      PIN_DIP_SW_1, PIN_DIP_SW_2, PIN_DIP_SW_3, PIN_DIP_SW_4};
#else
  constexpr Pin pin_numbers[SWITCH_LAST] = {
      PIN_SW_1_UP, PIN_SW_1_DOWN, PIN_SW_2_UP, PIN_SW_2_DOWN,
      PIN_SW_3_UP, PIN_SW_3_DOWN, PIN_FSW_1,   PIN_FSW_2};
#endif

  for (size_t i = 0; i < SWITCH_LAST; i++) {
    switches[i].Init(pin_numbers[i]);
  }
}

void DaisyHardware::InitAnalogControls() {
  constexpr Pin knob_pins[KNOB_LAST] = {PIN_KNOB_1, PIN_KNOB_2, PIN_KNOB_3,
                                        PIN_KNOB_4, PIN_KNOB_5, PIN_KNOB_6};

  // Set order of ADCs based on CHANNEL NUMBER
  AdcChannelConfig cfg[KNOB_LAST];

  // Initialize ADC configuration with Single Pins
  for (size_t i = 0; i < KNOB_LAST; ++i) {
    cfg[i].InitSingle(knob_pins[i]);
  }

  // Initialize ADC with configuration
  seed.adc.Init(cfg, KNOB_LAST);

  // Get the audio callback rate once
  float callback_rate = AudioCallbackRate();

  // Initialize knobs with ADC pointers and callback rate
  for (size_t i = 0; i < KNOB_LAST; ++i) {
    knobs[i].Init(seed.adc.GetPtr(i), callback_rate);
  }
}

// Public convenience function to get position of toggleswitches 1-3.
DaisyHardware::ToggleswitchPosition DaisyHardware::GetToggleswitchPosition(
    Toggleswitch tsw) {
  switch (tsw) {
    case (TOGGLESWITCH_1):
#if defined(PLATFORM_funbox)
      return GetLogicalSwitchPosition(switches[SWITCH_1_RIGHT],
                                      switches[SWITCH_1_LEFT]);
#else
      return GetLogicalSwitchPosition(switches[SWITCH_1_UP],
                                      switches[SWITCH_1_DOWN]);
#endif
    case (TOGGLESWITCH_2):
#if defined(PLATFORM_funbox)
      return GetLogicalSwitchPosition(switches[SWITCH_2_RIGHT],
                                      switches[SWITCH_2_LEFT]);
#else
      return GetLogicalSwitchPosition(switches[SWITCH_2_UP],
                                      switches[SWITCH_2_DOWN]);
#endif
    case (TOGGLESWITCH_3):
#if defined(PLATFORM_funbox)
      return GetLogicalSwitchPosition(switches[SWITCH_3_RIGHT],
                                      switches[SWITCH_3_LEFT]);
#else
      return GetLogicalSwitchPosition(switches[SWITCH_3_UP],
                                      switches[SWITCH_3_DOWN]);
#endif
    default:
      seed.PrintLine(
          "ERROR: Unexpected value provided for Toggleswitch 'tsw'. "
          "Returning TOGGLESWITCH_UNKNOWN by default.");
      return TOGGLESWITCH_UNKNOWN;
  }
}

void DaisyHardware::CheckResetToBootloader() {
  if (switches[DaisyHardware::FOOTSWITCH_1].Pressed()) {
    if (footswitch_start_time[0] == 0) {
      footswitch_start_time[0] = System::GetNow();
    } else if (System::GetNow() - footswitch_start_time[0] >= HOLD_THRESHOLD_MS) {
      // Shut 'er down so the LEDs always flash
      StopAdc();
      StopAudio();

      daisy::Led _led_1, _led_2;
      _led_1.Init(seed.GetPin(22), false);
      _led_2.Init(seed.GetPin(23), false);

      // Alternately flash the LEDs 3 times
      for (int i = 0; i < 3; i++) {
        _led_1.Set(1);
        _led_2.Set(0);
        _led_1.Update();
        _led_2.Update();
        System::Delay(100);

        _led_1.Set(0);
        _led_2.Set(1);
        _led_1.Update();
        _led_2.Update();
        System::Delay(100);
      }

      // Reset system to bootloader after LED flashing
      System::ResetToBootloader();
    }
  } else {
    // Reset the hold timer if the footswitch is released
    footswitch_start_time[0] = 0;
  }
}

DaisyHardware::ToggleswitchPosition DaisyHardware::GetLogicalSwitchPosition(
    Switch high,
    Switch low) {
  return high.Pressed()
             ? TOGGLESWITCH_RIGHT
             : (low.Pressed() ? TOGGLESWITCH_LEFT : TOGGLESWITCH_MIDDLE);
}

void DaisyHardware::RegisterFootswitchCallbacks(FootswitchCallbacks *callbacks) {
  footswitchCallbacks = callbacks;
}

// Watches for normal, double, and long presses of the footswitches.
void DaisyHardware::ProcessFootswitchPresses(Switches footswitch) {
  if (footswitchCallbacks == NULL) {
    return; // Nothing to do if callbacks have not been registered
  }
  bool is_pressed = switches[footswitch].RisingEdge();
  int footswitch_index = footswitch == DaisyHardware::FOOTSWITCH_1 ? 0 : 1;

  uint32_t now = System::GetNow();

  if (is_pressed == true && footswitch_last_state[footswitch_index] == false) {
    // Footswitch is pressed
    footswitch_start_time[footswitch_index] = now;

    if ((now - footswitch_last_press_time[footswitch_index]) <= DOUBLE_PRESS_THRESHOLD_MS) {
      footswitch_press_count[footswitch_index]++;
    } else {
      footswitch_press_count[footswitch_index] = 1;
    }

    footswitch_last_press_time[footswitch_index] = now;
    footswitch_long_press_triggered[footswitch_index] = false; // Reset long press trigger when pressed
  }

  uint32_t press_duration = now - footswitch_start_time[footswitch_index];

  if (switches[footswitch].Pressed() && press_duration >= HOLD_THRESHOLD_MS && !footswitch_long_press_triggered[footswitch_index]) {
    // Footswitch is being held down
    if (footswitchCallbacks->HandleLongPress != NULL) {
      footswitchCallbacks->HandleLongPress(footswitch);
    }
    footswitch_long_press_triggered[footswitch_index] = true; // Ensure long press is only triggered once
  }

  if (is_pressed == false && footswitch_last_state[footswitch_index] == true) {
    // Button released
    if (!footswitch_long_press_triggered[footswitch_index]) {
      if (footswitch_press_count[footswitch_index] >= 2) {
        if (footswitchCallbacks->HandleDoublePress != NULL) {
          footswitchCallbacks->HandleDoublePress(footswitch);
        }
        footswitch_press_count[footswitch_index] = 0;
      } else if (press_duration < HOLD_THRESHOLD_MS && footswitchCallbacks->HandleNormalPress != NULL) {
        footswitchCallbacks->HandleNormalPress(footswitch);
      }
    }
  }

  footswitch_last_state[footswitch_index] = is_pressed;
}
