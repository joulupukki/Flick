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

#pragma once

#include "daisy_seed.h"
#include "optional"

using daisy::AdcChannelConfig;
using daisy::AnalogControl;
using daisy::AudioHandle;
using daisy::DaisySeed;
using daisy::Led;
using daisy::Pin;
using daisy::SaiHandle;
using daisy::Switch;

namespace flick {
class DaisyHardware {
 public:
  /** Switches */
  enum Switches {
#if defined(PLATFORM_funbox)
    SWITCH_1_LEFT,  /** Left position of toggleswitch 1 */
    SWITCH_1_RIGHT, /** Right position of toggleswitch 1 */
    SWITCH_2_LEFT,  /** Left position of toggleswitch 2 */
    SWITCH_2_RIGHT, /** Right position of toggleswitch 2 */
    SWITCH_3_LEFT,  /** Left position of toggleswitch 3 */
    SWITCH_3_RIGHT, /** Right position of toggleswitch 3 */
    FOOTSWITCH_1,   /** Footswitch 1 */
    FOOTSWITCH_2,   /** Footswitch 2 */
    DIP_SWITCH_1,   /**< Dip Switch 1 */
    DIP_SWITCH_2,   /**< Dip Switch 2 */
    DIP_SWITCH_3,   /**< Dip Switch 3 */
    DIP_SWITCH_4,   /**< Dip Switch 4 */
    SWITCH_LAST,    /**< Last enum item */
#else
    SWITCH_1_UP,   /** Up position of toggleswitch 1 */
    SWITCH_1_DOWN, /** Down position of toggleswitch 1 */
    SWITCH_2_UP,   /** Up position of toggleswitch 2 */
    SWITCH_2_DOWN, /** Down position of toggleswitch 2 */
    SWITCH_3_UP,   /** Up position of toggleswitch 3 */
    SWITCH_3_DOWN, /** Down position of toggleswitch 3 */
    FOOTSWITCH_1,  /** Footswitch 1 */
    FOOTSWITCH_2,  /** Footswitch 2 */
    SWITCH_LAST,   /**< Last enum item */
#endif
  };

  /** Knobs */
  enum Knob {
    KNOB_1,    /**< & */
    KNOB_2,    /**< & */
    KNOB_3,    /**< & */
    KNOB_4,    /**< & */
    KNOB_5,    /**< & */
    KNOB_6,    /**< & */
    KNOB_LAST, /**< & */
  };

  /** footswitch leds */
  enum Led {
    LED_1 = 22, /**< & */
    LED_2 = 23, /**< & */
    LED_LAST,   /**< & */
  };

  // Logical toggleswitch positions: RIGHT/HIGH = 0, MIDDLE = 1, LEFT/LOW = 2.
  // This aligns both platforms so UP (Hothouse) and RIGHT (Funbox) map to index 0.
  enum ToggleswitchPosition {
    TOGGLESWITCH_RIGHT = 0,
    TOGGLESWITCH_MIDDLE = 1,
    TOGGLESWITCH_LEFT = 2,
    TOGGLESWITCH_UNKNOWN = 3,
  };

  enum Toggleswitch {
    TOGGLESWITCH_1,
    TOGGLESWITCH_2,
    TOGGLESWITCH_3,
  };

  struct FootswitchCallbacks {
    /** Called when a single footswitch press is detected. */
    void (*HandleNormalPress)(Switches footswitch);

    /** Called when a double footswitch press is detected. */
    void (*HandleDoublePress)(Switches footswitch);

    /** Called when a long footswitch press is detected. */
    void (*HandleLongPress)(Switches footswitch);
  };

  // Constructor and Destructor
  DaisyHardware() = default;
  ~DaisyHardware() = default;

  /** Initialize hardware */
  void Init(bool boost = false);

  /**
     Wait before moving on.
     \param del Delay time in ms.
   */
  void DelayMs(size_t del);

  /** Starts the callback
  \param cb Interleaved callback function
  */
  void StartAudio(AudioHandle::InterleavingAudioCallback cb);

  /** Starts the callback
  \param cb multichannel callback function
  */
  void StartAudio(AudioHandle::AudioCallback cb);

  /**
     Switch callback functions
     \param cb New interleaved callback function.
  */
  void ChangeAudioCallback(AudioHandle::InterleavingAudioCallback cb);

  /**
     Switch callback functions
     \param cb New multichannel callback function.
  */
  void ChangeAudioCallback(AudioHandle::AudioCallback cb);

  /** Stops the audio if it is running. */
  void StopAudio();

  /** Updates the Audio Sample Rate, and reinitializes.
   ** Audio must be stopped for this to work.
   */
  void SetAudioSampleRate(SaiHandle::Config::SampleRate samplerate);

  /** Returns the audio sample rate in Hz as a floating point number.
   */
  float AudioSampleRate();

  /** Sets the number of samples processed per channel by the audio callback.
     \param size Audio block size
   */
  void SetAudioBlockSize(size_t size);

  /** Returns the number of samples per channel in a block of audio. */
  size_t AudioBlockSize();

  /** Returns the rate in Hz that the Audio callback is called */
  float AudioCallbackRate();

  /** Start analog to digital conversion. */
  void StartAdc();

  /** Stops Transfering data from the ADC */
  void StopAdc();

  /** Call at the same frequency as controls are read for stable readings.*/
  void ProcessAnalogControls();

  /** Process Analog and Digital Controls */
  inline void ProcessAllControls() {
    ProcessAnalogControls();
    ProcessDigitalControls();
  }

  /** Get value per knobs.
  \param k Which knobs to get
  \return Floating point knobs position.
  */
  float GetKnobValue(Knob k);

  /** Process digital controls */
  void ProcessDigitalControls();

  /** Get the current position of a toggleswitch (right/high, middle, left/low).
  \param tsw Which toggleswitch to interogate (TOGGLESWITCH_1, TOGGLESWITCH_2,
  or TOGGLESWITCH_3) \return TOGGLESWITCH_RIGHT (0), TOGGLESWITCH_MIDDLE (1), or
  TOGGLESWITCH_LEFT (2). \note If the toggleswitch in question is ON-ON (rather
  than ON-OFF-ON), TOGGLESWITCH_MIDDLE can never be the return value. Write
  your code with this in mind.
  */
  ToggleswitchPosition GetToggleswitchPosition(Toggleswitch tsw);

  /** Check whether FOOTSWITCH_1 (the left foot switch) has been held down for 2
   * seconds and, if it has, call System::ResetToBootloader(). This has the same
   * effect as pressing the Daisy Seed RESET and BOOT buttons to get into
   * flashable (DFU)) mode. \note The LEDs will alternately flash three times
   * before the reset. */
  void CheckResetToBootloader();

  /** Register/Deregister footswitch press callbacks. This provides an
   * alternative way of handling foot switch presses and allows effects to make
   * use of double and long presses.
   * \param callbacks A pointer to the struct that defines the callbacks or NULL
   * to deregister all callbacks.
   */
  void RegisterFootswitchCallbacks(FootswitchCallbacks *callbacks);

  DaisySeed seed; /**< & */

  AnalogControl knobs[KNOB_LAST]; /**< & */
  Switch switches[SWITCH_LAST];   /**< & */

 private:
  void SetHidUpdateRates();
  void InitSwitches();
  void InitAnalogControls();
  ToggleswitchPosition GetLogicalSwitchPosition(Switch high, Switch low);
  void ProcessFootswitchPresses(Switches footswitch);

  uint32_t footswitch_start_time[2] = {0, 0};  // Store footswitch start time
  uint32_t footswitch_last_press_time[2] = {0, 0};
  bool footswitch_last_state[2] = {false, false};
  uint8_t footswitch_press_count[2] = {0, 0};
  bool footswitch_long_press_triggered[2] = {false, false};
  static const uint32_t HOLD_THRESHOLD_MS = 2000;  // 2 second hold time
  static const uint32_t DOUBLE_PRESS_THRESHOLD_MS = 600;

  inline uint16_t* adc_ptr(const uint8_t chn) { return seed.adc.GetPtr(chn); }

  FootswitchCallbacks *footswitchCallbacks = NULL;
};

using Funbox = DaisyHardware;

}  // namespace flick
