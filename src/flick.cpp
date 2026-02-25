// Flick for Funbox DIY DSP Platform
// Copyright (C) 2025-2026 Boyd Timothy <btimothy@gmail.com>
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

#include "daisy.h"
#include "daisysp.h"
#include "flick_oscillator.h"
#include "flick_filters.hpp"
#include "daisy_hardware.h"
#include "hall_reverb.h"
#include "spring_reverb.h"
#include "Dattorro.hpp"
#include "parameter_capture.h"
#include <math.h>

#if !defined(PLATFORM_funbox) && !defined(PLATFORM_hothouse)
#error "PLATFORM must be set to funbox or hothouse"
#endif

using flick::FlickOscillator;
using flick::Funbox;
using flick::KnobCapture;
using flick::SwitchCapture;
using daisy::AudioHandle;
using daisy::Led;
using daisy::Parameter;
using daisy::PersistentStorage;
using daisy::SaiHandle;
using daisy::System;
using daisysp::DelayLine;
using daisysp::fonepole;

/// Increment this when changing the settings struct so the software will know
/// to reset to defaults if this ever changes.
#define SETTINGS_VERSION 6

Funbox hw;

// Audio config constants
constexpr float SAMPLE_RATE = 48000.0f;
constexpr size_t MAX_DELAY = static_cast<size_t>(SAMPLE_RATE * 2.0f);

// Tremolo constants
constexpr float TREMOLO_SPEED_MIN = 0.2f;   // Minimum tremolo speed in Hz
constexpr float TREMOLO_SPEED_MAX = 16.0f;  // Maximum tremolo speed in Hz
constexpr float TREMOLO_DEPTH_SCALE = 1.0f; // Scale factor for tremolo depth
constexpr float TREMOLO_LED_BRIGHTNESS = 0.4f; // LED brightness when only tremolo is active

// Delay constants
constexpr float DELAY_TIME_MIN_SECONDS = 0.05f;
constexpr float DELAY_WET_MIX_ATTENUATION = 0.333f; // Attenuation for wet delay signal
constexpr float DELAY_DRY_WET_PERCENT_MAX = 100.0f; // Max value for dry/wet percentage

// Reverb constants (Dattorro plate reverb scaling)

constexpr float PLATE_PRE_DELAY_SCALE = 0.25f;
constexpr float PLATE_DAMP_SCALE = 10.0f;

constexpr float PLATE_TANK_MOD_SPEED_VALUES[] = {0.5f, 0.25f, 0.1f};
constexpr float PLATE_TANK_MOD_DEPTH_VALUES[] = {0.5f, 0.25f, 0.1f};
constexpr float PLATE_TANK_MOD_SHAPE_VALUES[] = {0.5f, 0.25f, 0.1f};

constexpr float PLATE_TANK_MOD_SPEED_SCALE = 8.0f;  // Multiplier for tank modulation speed
constexpr float PLATE_TANK_MOD_DEPTH_SCALE = 15.0f; // Multiplier for tank modulation depth

// Filter Frequency constants
constexpr float NOTCH_1_FREQ = 6020.0f; // Daisy Seed resonance notch
constexpr float NOTCH_2_FREQ = 12278.0f; // Daisy Seed resonance notch

// Harmonic tremolo state (filter cutoffs taken from Fender 6G12-A schematic)
constexpr float HARMONIC_TREMOLO_LPF_CUTOFF = 144.0f; // 220K and 5nF LPF
constexpr float HARMONIC_TREMOLO_HPF_CUTOFF = 636.0f; // 1M and 250pF HPF

// EQ-Shaping EQ Filters for Harmonic Tremolo
constexpr float HARMONIC_TREM_EQ_HPF1_CUTOFF = 63.0f;
constexpr float HARMONIC_TREM_EQ_LPF1_CUTOFF = 11200.0f;
constexpr float HARMONIC_TREM_EQ_PEAK1_FREQ = 7500.0f;
constexpr float HARMONIC_TREM_EQ_PEAK1_GAIN = -3.37f; // in dB
constexpr float HARMONIC_TREM_EQ_PEAK1_Q = 0.263f;
constexpr float HARMONIC_TREM_EQ_PEAK2_FREQ = 254.0f;
constexpr float HARMONIC_TREM_EQ_PEAK2_GAIN = 2.0f; // in dBz
constexpr float HARMONIC_TREM_EQ_PEAK2_Q = 0.707f;
constexpr float HARMONIC_TREM_EQ_LOW_SHELF_FREQ = 37.0f;
constexpr float HARMONIC_TREM_EQ_LOW_SHELF_GAIN = -10.5f; // in dB
constexpr float HARMONIC_TREM_EQ_LOW_SHELF_Q = 1.0f; // Shelf slope

enum PedalMode {
  PEDAL_MODE_NORMAL,
  PEDAL_MODE_EDIT_REVERB,     // Edit mode activated by double-press of the left foot switch
  PEDAL_MODE_EDIT_MONO_STEREO // Edit mode activated by long-press of the right foot switch
};

enum MonoStereoMode {                       // Controlled by Toggle Switch 3
  MS_MODE_MIMO, // Mono In, Mono Out        // TOGGLESWITCH_LEFT
  MS_MODE_MISO, // Mono In, Stereo Out      // TOGGLESWITCH_MIDDLE
  MS_MODE_SISO, // Stereo In, Stereo Out    // TOGGLESWITCH_RIGHT
};

enum ReverbType {
  REVERB_PLATE,
  REVERB_SPRING,
  REVERB_HALL,
  REVERB_DEFAULT = REVERB_PLATE  // For the 4th combination
};

// Persistent Settings
struct Settings {
  int version; // Version of the settings struct
  float decay;
  float diffusion;
  float input_cutoff_freq;
  float tank_cutoff_freq;
  int tank_mod_speed_pos;    // Switch position (0, 1, or 2)
  int tank_mod_depth_pos;    // Switch position (0, 1, or 2)
  int tank_mod_shape_pos;    // Switch position (0, 1, or 2)
  float pre_delay;
  int mono_stereo_mode;
  bool bypass_reverb;
  bool bypass_tremolo;
  bool bypass_delay;

	//Overloading the != operator
	//This is necessary as this operator is used in the PersistentStorage source code
	bool operator!=(const Settings& a) const {
    return !(
      a.version == version &&
      a.decay == decay &&
      a.diffusion == diffusion &&
      a.input_cutoff_freq == input_cutoff_freq &&
      a.tank_cutoff_freq == tank_cutoff_freq &&
      a.tank_mod_speed_pos == tank_mod_speed_pos &&
      a.tank_mod_depth_pos == tank_mod_depth_pos &&
      a.tank_mod_shape_pos == tank_mod_shape_pos &&
      a.pre_delay == pre_delay &&
      a.mono_stereo_mode == mono_stereo_mode &&
      a.bypass_reverb == bypass_reverb &&
      a.bypass_tremolo == bypass_tremolo &&
      a.bypass_delay == bypass_delay
    );
  }
};

//Persistent Storage Declaration. Using type Settings and passed the devices qspi handle
PersistentStorage<Settings> SavedSettings(hw.seed.qspi);

FlickOscillator osc;
float dc_offset = 0;

DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delMemL;
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delMemR;

Dattorro verb(48000, 16, 4.0);
flick::HallReverb hall_reverb;
flick::SpringReverb spring_reverb;
ReverbType current_reverb_type = REVERB_PLATE;
PedalMode pedal_mode = PEDAL_MODE_NORMAL;
MonoStereoMode mono_stereo_mode = MS_MODE_MIMO;

Parameter p_verb_amt;
Parameter p_trem_speed, p_trem_depth;
Parameter p_delay_time, p_delay_feedback, p_delay_amt;

Parameter p_knob_1, p_knob_2, p_knob_3, p_knob_4, p_knob_5, p_knob_6;

// Reverb edit mode parameter captures
KnobCapture p_knob_2_capture(p_knob_2);
KnobCapture p_knob_3_capture(p_knob_3);
KnobCapture p_knob_4_capture(p_knob_4);
KnobCapture p_knob_5_capture(p_knob_5);
KnobCapture p_knob_6_capture(p_knob_6);
SwitchCapture p_sw1_capture(hw, Funbox::TOGGLESWITCH_1);
SwitchCapture p_sw2_capture(hw, Funbox::TOGGLESWITCH_2);
SwitchCapture p_sw3_capture(hw, Funbox::TOGGLESWITCH_3);

struct Delay {
  DelayLine<float, MAX_DELAY> *del;
  float current_delay;
  float delay_target;
  float feedback;

  float Process(float in) {
    // set delay times
    fonepole(current_delay, delay_target, 0.0002f);
    del->SetDelay(current_delay);

    float read = del->Read();
    del->Write((feedback * read) + in);

    return read;
  }
};

enum ReverbKnobMode {
  REVERB_KNOB_ALL_DRY,
  REVERB_KNOB_DRY_WET_MIX,
  REVERB_KNOB_ALL_WET,
};

enum TremDelMakeUpGain {
  MAKEUP_GAIN_NONE,
  MAKEUP_GAIN_NORMAL,
};

enum TremoloMode {
  TREMOLO_SINE,        // Sine wave tremolo (LEFT)
  TREMOLO_HARMONIC,    // Harmonic tremolo (MIDDLE)
  TREMOLO_SQUARE,      // Opto/Square wave tremolo (RIGHT)
};


// Toggle switch mappings (orientation: Hothouse vertical UP/DOWN, Funbox horizontal LEFT/RIGHT)
constexpr ReverbKnobMode kReverbKnobMap[] = {
  REVERB_KNOB_ALL_WET,                        // UP (Hothouse) / RIGHT (Funbox)
  REVERB_KNOB_DRY_WET_MIX,                    // MIDDLE
  REVERB_KNOB_ALL_DRY,                        // DOWN (Hothouse) / LEFT (Funbox)
};

constexpr TremoloMode kTremoloModeMap[] = {
    TREMOLO_SQUARE,     // UP (Hothouse) / RIGHT (Funbox)
    TREMOLO_HARMONIC,   // MIDDLE
    TREMOLO_SINE,       // DOWN (Hothouse) / LEFT (Funbox)
};

enum DelayTimingMode {
  DELAY_TIMING_DOTTED_EIGHTH,   // 3/4 of knob value
  DELAY_TIMING_TRIPLET,         // 2/3 of knob value
  DELAY_TIMING_QUARTER,         // 1x knob value (straight)
};

constexpr DelayTimingMode kDelayTimingMap[] = {
  DELAY_TIMING_DOTTED_EIGHTH,   // UP (Hothouse) / RIGHT (Funbox)
  DELAY_TIMING_QUARTER,         // MIDDLE
  DELAY_TIMING_TRIPLET,         // DOWN (Hothouse) / LEFT (Funbox)
};

constexpr float kDelayTimingMultiplier[] = {
  0.75f,    // DELAY_TIMING_DOTTED_EIGHTH
  0.6667f,  // DELAY_TIMING_TRIPLET
  1.0f,     // DELAY_TIMING_QUARTER
};

Delay delayL;
Delay delayR;
int delay_drywet;

float reverb_tone;
float reverb_feedback;
float reverb_sploodge;

// Bypass vars
Led led_left, led_right;
bool bypass_verb = true;
bool bypass_trem = true;
bool bypass_delay = true;

// DFU mode trigger
bool trigger_dfu_mode = false;

// Main Harmonic Tremolo Filters
LowPassFilter harmonic_trem_lpf_L;
LowPassFilter harmonic_trem_lpf_R;
HighPassFilter harmonic_trem_hpf_L;
HighPassFilter harmonic_trem_hpf_R;

// EQ Shaping Filters for Harmonic Tremolo
HighPassFilter harmonic_trem_eq_hpf1_L;
HighPassFilter harmonic_trem_eq_hpf1_R;
LowPassFilter harmonic_trem_eq_lpf1_L;
LowPassFilter harmonic_trem_eq_lpf1_R;
PeakingEQ harmonic_trem_eq_peak1_L;
PeakingEQ harmonic_trem_eq_peak1_R;
PeakingEQ harmonic_trem_eq_peak2_L;
PeakingEQ harmonic_trem_eq_peak2_R;
LowShelf harmonic_trem_eq_low_shelf_L;
LowShelf harmonic_trem_eq_low_shelf_R;

// General Notch Filters to remove Daisy Seed resonant frequencies
PeakingEQ notch1_L;
PeakingEQ notch1_R;
PeakingEQ notch2_L;
PeakingEQ notch2_R;

// Reverb vars

bool plate_diffusion_enabled = true;
float plate_pre_delay = 0.0 / PLATE_PRE_DELAY_SCALE;

float plate_delay = 0.0;

float plate_dry = 1.0;
float plate_wet = 0.5;

float plate_decay = 0.8;
float plate_time_scale = 1.007500;

float plate_tank_diffusion = 0.85;

  /**
   * Good Defaults
   * Lo Pitch: .287 (2.87) = 100Hz: 440 * (2^(2.87-5))
   * InputFilterHighCutoffPitch: 0.77 (7.77) is approx 3000Hz
   * TankFilterHighCutFrequency: 0.8 (8.0) is 3520Hz
   * 0.9507 is approx 10kHz
   * 
   * mod speed: 0.5
   * mod depth: 0.5
   * mod shape: 0.75
   */

// The damping values appear to be want to be between 0 and 10
float plate_input_damp_low = 2.87 / PLATE_DAMP_SCALE;  // approx 100Hz
float plate_input_damp_high = 7.25 / PLATE_DAMP_SCALE; // 7.25 is approx 3000Hz

float plate_tank_damp_low = 2.87 / PLATE_DAMP_SCALE;   // approx 100Hz
float plate_tank_damp_high = 7.25 / PLATE_DAMP_SCALE;  // 7.25 is approx 3520Hz

// Switch positions (0, 1, or 2) for tank modulation parameters
int plate_tank_mod_speed_pos = 2;  // Position 2 = 0.1 (from PLATE_TANK_MOD_SPEED_VALUES)
int plate_tank_mod_depth_pos = 2;  // Position 2 = 0.1 (from PLATE_TANK_MOD_DEPTH_VALUES)
int plate_tank_mod_shape_pos = 1;  // Position 1 = 0.25 (from PLATE_TANK_MOD_SHAPE_VALUES)

/**
 * @brief Updates the Dattorro plate reverb parameters based on the current global variables.
 */
void updatePlateReverbParameters() {
  verb.setDecay(plate_decay);
  verb.setTankDiffusion(plate_tank_diffusion);
  verb.setInputFilterHighCutoffPitch(plate_input_damp_high * PLATE_DAMP_SCALE);
  verb.setTankFilterHighCutFrequency(plate_tank_damp_high * PLATE_DAMP_SCALE);
  verb.setTankModSpeed(PLATE_TANK_MOD_SPEED_VALUES[plate_tank_mod_speed_pos] * PLATE_TANK_MOD_SPEED_SCALE);
  verb.setTankModDepth(PLATE_TANK_MOD_DEPTH_VALUES[plate_tank_mod_depth_pos] * PLATE_TANK_MOD_DEPTH_SCALE);
  verb.setTankModShape(PLATE_TANK_MOD_SHAPE_VALUES[plate_tank_mod_shape_pos]);
  verb.setPreDelay(plate_pre_delay * PLATE_PRE_DELAY_SCALE);
}

const float minus_18db_gain = 0.12589254;
const float minus_20db_gain = 0.1;

float left_input = 0.;
float right_input = 0.;
float left_output = 0.;
float right_output = 0.;
float reverb_dry_scale_factor = 1.0;
float reverb_reverse_scale_factor = 1.0;

float input_amplification = 1.0; // This isn't really used yet

bool trigger_settings_save = false;

/// @brief Used at startup to control a factory reset.
///
/// This gets set to true in `main()` if footswitch 2 is depressed at boot.
/// The LED lights will start flashing alternatively. To exit this mode without
/// making any changes, press either footswitch.
///
/// To reset, rotate knob_1 to 100%, to 0%, to 100%, and back to 0%. This will
/// restore all defaults and then go into normal pedal mode.
bool is_factory_reset_mode = false;

/// @brief Tracks the stage of knob_1 rotation in factory reset mode.
///
/// 0: User must rotate knob_1 to 100% to advance to the next stage.
/// 1: User must rotate knob_1 to 0% to advance to the next stage.
/// 2: User must rotate knob_1 to 100% to advance to the next stage.
/// 3: User must rotate knob_1 to 0% to complete the factory reset.
int factory_reset_stage = 0;

inline void updateReverbScales(MonoStereoMode mode) {
  switch (mode) {
    case MS_MODE_MIMO:
      reverb_dry_scale_factor = 5.0f; // Make the signal stronger for MIMO mode
      reverb_reverse_scale_factor = 0.2f;
      break;
    case MS_MODE_MISO:
    case MS_MODE_SISO:
      reverb_dry_scale_factor = 2.5f; // MISO and SISO modes
      reverb_reverse_scale_factor = 0.4f;
      break;
  }
}

void loadSettings() {

	// Reference to local copy of settings stored in flash
	Settings &local_settings = SavedSettings.GetSettings();

  int savedVersion = local_settings.version;

  if (savedVersion != SETTINGS_VERSION) {
    // Something has changed. Load defaults!
    SavedSettings.RestoreDefaults();
    loadSettings();
    return;
  }

  plate_decay = local_settings.decay;
  plate_tank_diffusion = local_settings.diffusion;
  plate_input_damp_high = local_settings.input_cutoff_freq;
  plate_tank_damp_high = local_settings.tank_cutoff_freq;
  plate_tank_mod_speed_pos = local_settings.tank_mod_speed_pos;
  plate_tank_mod_depth_pos = local_settings.tank_mod_depth_pos;
  plate_tank_mod_shape_pos = local_settings.tank_mod_shape_pos;
  plate_pre_delay = local_settings.pre_delay;
  mono_stereo_mode = static_cast<MonoStereoMode>(local_settings.mono_stereo_mode);
  updateReverbScales(mono_stereo_mode);

  bypass_verb = local_settings.bypass_reverb;
  bypass_trem = local_settings.bypass_tremolo;
  bypass_delay = local_settings.bypass_delay;

  updatePlateReverbParameters();
}

void saveSettings() {
	//Reference to local copy of settings stored in flash
	Settings &local_settings = SavedSettings.GetSettings();

  local_settings.version = SETTINGS_VERSION;
  local_settings.decay = plate_decay;
  local_settings.diffusion = plate_tank_diffusion;
  local_settings.input_cutoff_freq = plate_input_damp_high;
  local_settings.tank_cutoff_freq = plate_tank_damp_high;
  local_settings.tank_mod_speed_pos = plate_tank_mod_speed_pos;
  local_settings.tank_mod_depth_pos = plate_tank_mod_depth_pos;
  local_settings.tank_mod_shape_pos = plate_tank_mod_shape_pos;
  local_settings.pre_delay = plate_pre_delay;

	trigger_settings_save = true;
}

void saveMonoStereoSettings() {
  Settings &LocalSettings = SavedSettings.GetSettings();

  LocalSettings.mono_stereo_mode = mono_stereo_mode;

  trigger_settings_save = true;
}

void saveBypassStates() {
  Settings &local_settings = SavedSettings.GetSettings();

  local_settings.bypass_reverb = bypass_verb;
  local_settings.bypass_tremolo = bypass_trem;
  local_settings.bypass_delay = bypass_delay;

  trigger_settings_save = true;
}

/// @brief Restore the reverb settings from the saved settings.
void restoreReverbSettings() {
	Settings &local_settings = SavedSettings.GetSettings();

  plate_decay = local_settings.decay;
  plate_tank_diffusion = local_settings.diffusion;
  plate_input_damp_high = local_settings.input_cutoff_freq;
  plate_tank_damp_high = local_settings.tank_cutoff_freq;
  plate_tank_mod_speed_pos = local_settings.tank_mod_speed_pos;
  plate_tank_mod_depth_pos = local_settings.tank_mod_depth_pos;
  plate_tank_mod_shape_pos = local_settings.tank_mod_shape_pos;
  plate_pre_delay = local_settings.pre_delay;

  updatePlateReverbParameters();
}

/// @brief Restore the mono-stereo settings from the saved settings.
void restoreMonoStereoSettings() {
  Settings &local_settings = SavedSettings.GetSettings();

  mono_stereo_mode = static_cast<MonoStereoMode>(local_settings.mono_stereo_mode);
  updateReverbScales(mono_stereo_mode);
}

void handleNormalPress(Funbox::Switches footswitch) {
  if (pedal_mode == PEDAL_MODE_EDIT_REVERB) {
    // Only save the settings if the RIGHT footswitch is pressed in edit mode.
    // The LEFT footswitch is used to exit edit mode without saving.
    if (footswitch == Funbox::FOOTSWITCH_2) {
      saveSettings();
    } else {
      restoreReverbSettings();
    }

    // Reset all parameter captures when exiting reverb edit mode
    p_knob_2_capture.Reset();
    p_knob_3_capture.Reset();
    p_knob_4_capture.Reset();
    p_knob_5_capture.Reset();
    p_knob_6_capture.Reset();
    p_sw1_capture.Reset();
    p_sw2_capture.Reset();
    p_sw3_capture.Reset();

    pedal_mode = PEDAL_MODE_NORMAL;
  } else if (pedal_mode == PEDAL_MODE_EDIT_MONO_STEREO) {
    // Only save the settings if the RIGHT footswitch is pressed in mono-stereo
    // edit mode. The LEFT footswitch is used to exit mono-stereo edit mode
    // without saving.
    if (footswitch == Funbox::FOOTSWITCH_2) {
      // Save the mono-stereo settings
      saveMonoStereoSettings();
    } else {
      // Cancel: restore mono-stereo settings AND bypass states
      restoreMonoStereoSettings();
    }

    pedal_mode = PEDAL_MODE_NORMAL;
  } else {
    if (footswitch == Funbox::FOOTSWITCH_1) {
      bypass_verb = !bypass_verb;

      if (bypass_verb) {
        // Clear the reverb tails when the reverb is bypassed so if you
        // turn it back on, it starts fresh and doesn't sound weird.
        verb.clear();
      }
    } else {
      // FOOTSWITCH_2: Toggle tremolo on/off
      bypass_trem = !bypass_trem;
    }

    saveBypassStates();
  }
}

void handleDoublePress(Funbox::Switches footswitch) {
  // Ignore double presses in edit modes
  if (pedal_mode == PEDAL_MODE_EDIT_REVERB || pedal_mode == PEDAL_MODE_EDIT_MONO_STEREO) {
    return;
  }

  // When double press is detected, a normal press was already detected and
  // processed, so reverse that right off the bat.
  handleNormalPress(footswitch);

  if (footswitch == Funbox::FOOTSWITCH_2) {
    // FOOTSWITCH_2 double press: Toggle delay on/off
    bypass_delay = !bypass_delay;

    saveBypassStates();
  }
  // FOOTSWITCH_1 double press: No action (reserved for potential future use)
}

void handleLongPress(Funbox::Switches footswitch) {

  // Ignore long presses in edit modes
  if (pedal_mode == PEDAL_MODE_EDIT_REVERB || pedal_mode == PEDAL_MODE_EDIT_MONO_STEREO) {
    return;
  }

  // When long press is detected, a normal press was already detected and
  // processed, so reverse that right off the bat.
  handleNormalPress(footswitch);

  // Check if both footswitches are pressed simultaneously - enter DFU mode
  bool both_pressed = hw.switches[Funbox::FOOTSWITCH_1].Pressed() &&
                      hw.switches[Funbox::FOOTSWITCH_2].Pressed();

  if (both_pressed) {
    // Set flag to trigger DFU mode in main loop (where LED blinking works properly)
    trigger_dfu_mode = true;
  } else if (footswitch == Funbox::FOOTSWITCH_1) {
    // FOOTSWITCH_1 long press: Enter reverb edit mode
    p_knob_2_capture.Capture(plate_pre_delay);
    p_knob_3_capture.Capture(plate_decay);
    p_knob_4_capture.Capture(plate_tank_diffusion);
    p_knob_5_capture.Capture(plate_input_damp_high);
    p_knob_6_capture.Capture(plate_tank_damp_high);
    p_sw1_capture.Capture(plate_tank_mod_speed_pos);
    p_sw2_capture.Capture(plate_tank_mod_depth_pos);
    p_sw3_capture.Capture(plate_tank_mod_shape_pos);

    bypass_verb = false; // Make sure that reverb is ON
    pedal_mode = PEDAL_MODE_EDIT_REVERB;
  } else if (footswitch == Funbox::FOOTSWITCH_2) {
    // FOOTSWITCH_2 long press: Enter mono-stereo config.
    pedal_mode = PEDAL_MODE_EDIT_MONO_STEREO;
  }
}

inline float hardLimit100_(const float &x) {
    return (x > 1.) ? 1. : ((x < -1.) ? -1. : x);
}

void quickLedFlash() {
  led_left.Set(1.0f);
  led_right.Set(1.0f);
  led_left.Update();
  led_right.Update();
  hw.DelayMs(500);
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  static float trem_val;
  hw.ProcessAllControls();

  // Read DIP switches to determine reverb type (runtime update)
#if defined(PLATFORM_funbox)
  {
    bool dip1 = hw.switches[Funbox::DIP_SWITCH_1].RawState();
    bool dip2 = hw.switches[Funbox::DIP_SWITCH_2].RawState();
    if (!dip1 && !dip2) {
      current_reverb_type = REVERB_PLATE;
    } else if (!dip1 && dip2) {
      current_reverb_type = REVERB_SPRING;
    } else if (dip1 && !dip2) {
      current_reverb_type = REVERB_HALL;
    } else {
      current_reverb_type = REVERB_PLATE; // Default for both on
    }
  }
#else
  current_reverb_type = REVERB_PLATE;
#endif

  if (pedal_mode == PEDAL_MODE_EDIT_REVERB) {
    // Edit mode

    // Blink the left & right LEDs
    {
      static uint32_t edit_count = 0;
      static bool led_state = true;
      if (++edit_count >= hw.AudioCallbackRate() / 2) {
        edit_count = 0;
        led_state = !led_state;
        led_left.Set(led_state ? 1.0f : 0.0f);
        led_right.Set(led_state ? 1.0f : 0.0f);
      }
    }
  } else if (pedal_mode == PEDAL_MODE_EDIT_MONO_STEREO) {
    // Mono-Stereo edit mode
    // Blink the left & right LEDs alternately to indicate mono-stereo edit mode
    static uint32_t mono_stereo_edit_count = 0;
    static bool led_state = true;
    if (++mono_stereo_edit_count >= hw.AudioCallbackRate() / 2) {
      mono_stereo_edit_count = 0;
      led_state = !led_state;
      led_left.Set(led_state ? 1.0f : 0.0f);
      led_right.Set(led_state ? 0.0f : 1.0f);
    }
  } else {
    // Normal mode
    led_left.Set(bypass_verb ? 0.0f : 1.0f);

    // Reduce number of LED Updates for pulsing trem LED
    {
      static int count = 0;
      // set led 100 times/sec
      if (++count == hw.AudioCallbackRate() / 100) {
        count = 0;
        // If just delay is on, show full-strength LED
        // If just trem is on, show 40% pulsing LED
        // If both are on, show 100% pulsing LED
        led_right.Set(bypass_trem ? bypass_delay ? 0.0f : 1.0 : bypass_delay ? trem_val * TREMOLO_LED_BRIGHTNESS : trem_val);
      }
    }
  }
  led_left.Update();
  led_right.Update();

  plate_wet = p_verb_amt.Process();

  TremDelMakeUpGain makeup_gain = (!bypass_delay || !bypass_trem) ? MAKEUP_GAIN_NORMAL : MAKEUP_GAIN_NONE;

  if (pedal_mode == PEDAL_MODE_NORMAL) {
    osc.SetFreq(p_trem_speed.Process());
    static float depth = 0;
    depth = daisysp::fclamp(p_trem_depth.Process(), 0.f, 1.f);

    // Get tremolo mode from SWITCH_2
    TremoloMode tremMode = kTremoloModeMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_2)];
    if (tremMode == TREMOLO_HARMONIC) {
      depth *= 1.25f;
    } else {
      depth *= 0.5f;
    }

    if (tremMode == TREMOLO_SQUARE) {
      osc.SetWaveform(FlickOscillator::WAVE_SQUARE_ROUNDED);
    } else {
      // Everything else uses sine wave (harmonic trem doesn't care about waveform)
      osc.SetWaveform(FlickOscillator::WAVE_SIN);
    }
    osc.SetAmp(depth);
    dc_offset = 1.f - depth;

    //
    // Delay
    //
    DelayTimingMode delay_timing = kDelayTimingMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_3)];
    float delay_time_raw = p_delay_time.Process();
    delayL.delay_target = delayR.delay_target = delay_time_raw * kDelayTimingMultiplier[delay_timing];
    delayL.feedback = delayR.feedback = p_delay_feedback.Process();
    delay_drywet = (int)p_delay_amt.Process();

    // Reverb dry/wet mode
    switch (kReverbKnobMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_1)]) {
      case REVERB_KNOB_ALL_DRY:
        plate_dry = 1.0;
        break;
      case REVERB_KNOB_DRY_WET_MIX:
        plate_dry = 1.0 - plate_wet;
        break;
      case REVERB_KNOB_ALL_WET:
        plate_dry = 0.0f;
        break;
    }
  } else if (pedal_mode == PEDAL_MODE_EDIT_REVERB) {
    // Edit mode with parameter capture
    plate_dry = 1.0; // Always use dry 100% in edit mode

    // Use capture objects - pass calculated values, they return frozen or current based on movement
    plate_pre_delay = p_knob_2_capture.Process();
    plate_decay = p_knob_3_capture.Process();
    plate_tank_diffusion = p_knob_4_capture.Process();
    plate_input_damp_high = p_knob_5_capture.Process();
    plate_tank_damp_high = p_knob_6_capture.Process();
    plate_tank_mod_speed_pos = p_sw1_capture.Process();
    plate_tank_mod_depth_pos = p_sw2_capture.Process();
    plate_tank_mod_shape_pos = p_sw3_capture.Process();

    updatePlateReverbParameters();

  } else if (pedal_mode == PEDAL_MODE_EDIT_MONO_STEREO) {
    // Mono-Stereo edit mode
    // Read in the mono-stereo mode from toggle switch 3
    switch (hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_3)) {
      case Funbox::TOGGLESWITCH_MIDDLE:
        mono_stereo_mode = MS_MODE_MISO; // Mono In, Stereo Out
        break;
      case Funbox::TOGGLESWITCH_LEFT:
        mono_stereo_mode = MS_MODE_MIMO; // Mono In, Mono Out
        break;
      default: // TOGGLESWITCH_RIGHT
        mono_stereo_mode = MS_MODE_SISO; // Stereo In, Stereo Out
    }
    updateReverbScales(mono_stereo_mode);
  }

  for (size_t i = 0; i < size; ++i) {
    float dry_L = in[0][i];
    float dry_R = in[1][i];
    float s_L, s_R;
    s_L = dry_L;
    if (mono_stereo_mode == MS_MODE_MIMO || mono_stereo_mode == MS_MODE_MISO) {
      // Use the mono signel (L) for both channels in MIMO and MISO modes
      s_R = dry_L;
    } else {
      // Use both L & R inputs in SISO mode
      s_R = dry_R;
    }

    // Apply notch filters for resonant frequencies
    s_L = notch1_L.Process(s_L);
    s_R = notch1_R.Process(s_R);
    s_L = notch2_L.Process(s_L);
    s_R = notch2_R.Process(s_R);

    if (!bypass_delay) {
      float mixL = 0;
      float mixR = 0;
      float fdrywet = delay_drywet / 100.0f;

      // update delayline with feedback
      float sigL = delayL.Process(s_L);
      float sigR = delayR.Process(s_R);
      mixL += sigL;
      mixR += sigR;

      float delay_make_up_gain = makeup_gain == MAKEUP_GAIN_NONE ? 1.0f : 1.66f;

      // apply drywet and attenuate
      s_L = fdrywet * mixL * 0.333f + (1.0f - fdrywet) * s_L * delay_make_up_gain;
      s_R = fdrywet * mixR * 0.333f + (1.0f - fdrywet) * s_R * delay_make_up_gain;
    }

    if (!bypass_trem) {
      // Get tremolo mode from SWITCH_2 (in normal mode)
      TremoloMode tremMode = TREMOLO_SINE;
      if (pedal_mode == PEDAL_MODE_NORMAL) {
        tremMode = kTremoloModeMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_2)];
      }
      // trem_val gets used above for pulsing LED
      float lfoSample = osc.Process();
      trem_val = dc_offset + lfoSample;
      float trem_make_up_gain = makeup_gain == MAKEUP_GAIN_NONE ? 1.0f : 1.2f;

      // Apply tremolo based on mode
      if (tremMode == TREMOLO_HARMONIC) {
        // Process left channel
        float lowL = harmonic_trem_lpf_L.Process(s_L);
        float highL = harmonic_trem_hpf_L.Process(s_L);  // 90° phase difference

        // Apply tremolo with opposite phase to each band
        float lowModL = lowL * (1.0f + lfoSample);
        float highModL = highL * (1.0f - lfoSample);
        s_L = (lowModL + highModL) * trem_make_up_gain;

        // Process right channel
        float lowR = harmonic_trem_lpf_R.Process(s_R);
        float highR = harmonic_trem_hpf_R.Process(s_R);  // 90° phase difference

        float lowModR = lowR * (1.0f + lfoSample);
        float highModR = highR * (1.0f - lfoSample);
        s_R = (lowModR + highModR) * trem_make_up_gain;

        //
        // Add additional EQ filtering to get a better sound out of the
        // harmonic tremolo.

        // Apply additional high pass filter at 63Hz
        s_L = harmonic_trem_eq_hpf1_L.Process(s_L);
        s_R = harmonic_trem_eq_hpf1_R.Process(s_R);

        // Apply low pass filter at 11200Hz
        s_L = harmonic_trem_eq_lpf1_L.Process(s_L);
        s_R = harmonic_trem_eq_lpf1_R.Process(s_R);

        // Apply low shelf cut at 37 Hz
        s_L = harmonic_trem_eq_low_shelf_L.Process(s_L);
        s_R = harmonic_trem_eq_low_shelf_R.Process(s_R);

        // Apply peaking EQ boost at 254 Hz
        s_L = harmonic_trem_eq_peak2_L.Process(s_L);
        s_R = harmonic_trem_eq_peak2_R.Process(s_R);

        // Apply peaking EQ cut at 7500 Hz
        s_L = harmonic_trem_eq_peak1_L.Process(s_L);
        s_R = harmonic_trem_eq_peak1_R.Process(s_R);
      } else {
        // Standard tremolo (sine or square)
        s_L = s_L * trem_val * trem_make_up_gain;
        s_R = s_R * trem_val * trem_make_up_gain;
      }
    }

    // Keep sending input to the reverb even if bypassed so that when it's
    // enabled again it will already have the current input signal already
    // being processed.

    left_input = hardLimit100_(s_L) * reverb_dry_scale_factor;
    right_input = hardLimit100_(s_R) * reverb_dry_scale_factor;

    float gain = minus_18db_gain * minus_20db_gain * (1.0f + input_amplification * 7.0f) * clearPopCancelValue;
    float rev_l, rev_r;

    switch (current_reverb_type) {
      case REVERB_PLATE:
        updatePlateReverbParameters();
        verb.process(left_input * gain, right_input * gain);
        rev_l = verb.getLeftOutput();
        rev_r = verb.getRightOutput();
        break;
      case REVERB_SPRING:
        spring_reverb.ProcessSample(left_input * gain, right_input * gain, &rev_l, &rev_r);
        break;
      case REVERB_HALL:
        hall_reverb.ProcessSample(left_input * gain, right_input * gain, &rev_l, &rev_r);
        // Make hall reverb louder to match the mix knob expectations
        rev_l *= 4.0f;
        rev_r *= 4.0f;
        break;
    }

    if (!bypass_verb) {
      // left_output = ((left_input * plate_dry * 0.1) + (verb.getLeftOutput() * plate_wet * clearPopCancelValue));
      // right_output = ((right_input * plate_dry * 0.1) + (verb.getRightOutput() * plate_wet * clearPopCancelValue));
      left_output = ((left_input * plate_dry * reverb_reverse_scale_factor) + (rev_l * plate_wet * clearPopCancelValue));
      right_output = ((right_input * plate_dry * reverb_reverse_scale_factor) + (rev_r * plate_wet * clearPopCancelValue));

      s_L = left_output;
      s_R = right_output;
    }

    if (mono_stereo_mode == MS_MODE_MIMO) {
      out[0][i] = (s_L * 0.5) + (s_R * 0.5); // Sum the processed left and right channels
      out[1][i] = 0.0f; // Mute the unused channel
    } else {
      // Send stereo output in MISO and SISO
      out[0][i] = s_L;
      out[1][i] = s_R;
    }
  }
}

int main() {
  hw.Init(true); // Init the CPU at full speed
  hw.SetAudioBlockSize(8);  // Number of samples handled per callback
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
  
  // Initialize LEDs
  led_left.Init(hw.seed.GetPin(Funbox::LED_1), false);
  led_right.Init(hw.seed.GetPin(Funbox::LED_2), false);

  //
  // Initialize Potentiometers
  //

  // The p_knob_n parameters are used to process the potentiometers when in reverb edit mode.
  p_knob_1.Init(hw.knobs[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
  p_knob_2.Init(hw.knobs[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
  p_knob_3.Init(hw.knobs[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR);
  p_knob_4.Init(hw.knobs[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
  p_knob_5.Init(hw.knobs[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
  p_knob_6.Init(hw.knobs[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR);

  p_verb_amt.Init(hw.knobs[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);

  p_trem_speed.Init(hw.knobs[Funbox::KNOB_2], TREMOLO_SPEED_MIN, TREMOLO_SPEED_MAX, Parameter::LINEAR);
  p_trem_depth.Init(hw.knobs[Funbox::KNOB_3], 0.0f, TREMOLO_DEPTH_SCALE, Parameter::LINEAR);

  p_delay_time.Init(hw.knobs[Funbox::KNOB_4], hw.AudioSampleRate() * DELAY_TIME_MIN_SECONDS, MAX_DELAY, Parameter::LOGARITHMIC);
  p_delay_feedback.Init(hw.knobs[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
  p_delay_amt.Init(hw.knobs[Funbox::KNOB_6], 0.0f, 100.0f, Parameter::LINEAR);

  delMemL.Init();
  delMemR.Init();
  delayL.del = &delMemL;
  delayR.del = &delMemR;

  osc.Init(hw.AudioSampleRate());

  // Initialize notch filters to remove resonant frequencies (always active)
  notch1_L.Init(NOTCH_1_FREQ, -30.0f, 40.0f, hw.AudioSampleRate());
  notch1_R.Init(NOTCH_1_FREQ, -30.0f, 40.0f, hw.AudioSampleRate());
  notch2_L.Init(NOTCH_2_FREQ, -30.0f, 40.0f, hw.AudioSampleRate());
  notch2_R.Init(NOTCH_2_FREQ, -30.0f, 40.0f, hw.AudioSampleRate());

  // Initialize harmonic tremolo filters
  harmonic_trem_lpf_L.Init(HARMONIC_TREMOLO_LPF_CUTOFF, hw.AudioSampleRate());
  harmonic_trem_lpf_R.Init(HARMONIC_TREMOLO_LPF_CUTOFF, hw.AudioSampleRate());
  harmonic_trem_hpf_L.Init(HARMONIC_TREMOLO_HPF_CUTOFF, hw.AudioSampleRate());
  harmonic_trem_hpf_R.Init(HARMONIC_TREMOLO_HPF_CUTOFF, hw.AudioSampleRate());

  harmonic_trem_eq_hpf1_L.Init(HARMONIC_TREM_EQ_HPF1_CUTOFF, hw.AudioSampleRate());
  harmonic_trem_eq_hpf1_R.Init(HARMONIC_TREM_EQ_HPF1_CUTOFF, hw.AudioSampleRate());
  harmonic_trem_eq_lpf1_L.Init(HARMONIC_TREM_EQ_LPF1_CUTOFF, hw.AudioSampleRate());
  harmonic_trem_eq_lpf1_R.Init(HARMONIC_TREM_EQ_LPF1_CUTOFF, hw.AudioSampleRate());
  harmonic_trem_eq_peak1_L.Init(HARMONIC_TREM_EQ_PEAK1_FREQ, HARMONIC_TREM_EQ_PEAK1_GAIN, HARMONIC_TREM_EQ_PEAK1_Q, hw.AudioSampleRate());
  harmonic_trem_eq_peak1_R.Init(HARMONIC_TREM_EQ_PEAK1_FREQ, HARMONIC_TREM_EQ_PEAK1_GAIN, HARMONIC_TREM_EQ_PEAK1_Q, hw.AudioSampleRate());
  harmonic_trem_eq_peak2_L.Init(HARMONIC_TREM_EQ_PEAK2_FREQ, HARMONIC_TREM_EQ_PEAK2_GAIN, HARMONIC_TREM_EQ_PEAK2_Q, hw.AudioSampleRate());
  harmonic_trem_eq_peak2_R.Init(HARMONIC_TREM_EQ_PEAK2_FREQ, HARMONIC_TREM_EQ_PEAK2_GAIN, HARMONIC_TREM_EQ_PEAK2_Q, hw.AudioSampleRate());
  harmonic_trem_eq_low_shelf_L.Init(HARMONIC_TREM_EQ_LOW_SHELF_FREQ, HARMONIC_TREM_EQ_LOW_SHELF_GAIN, HARMONIC_TREM_EQ_LOW_SHELF_Q, hw.AudioSampleRate());
  harmonic_trem_eq_low_shelf_R.Init(HARMONIC_TREM_EQ_LOW_SHELF_FREQ, HARMONIC_TREM_EQ_LOW_SHELF_GAIN, HARMONIC_TREM_EQ_LOW_SHELF_Q, hw.AudioSampleRate());

  //
  // Dattorro Reverb Initialization
  //
  // Zero out the InterpDelay buffers used by the plate reverb
  for(int i = 0; i < 50; i++) {
      for(int j = 0; j < 144000; j++) {
          sdramData[i][j] = 0.;
      }
  }
  // Set this to 1.0 or plate reverb won't work. This is defined in Dattorro's
  // InterpDelay.cpp file.
  hold = 1.;

  verb.setSampleRate(48000);
  verb.setTimeScale(plate_time_scale);
  verb.enableInputDiffusion(plate_diffusion_enabled);
  verb.setInputFilterLowCutoffPitch(plate_input_damp_low * PLATE_DAMP_SCALE);
  verb.setTankFilterLowCutFrequency(plate_tank_damp_low * PLATE_DAMP_SCALE);
  
  updatePlateReverbParameters();

  // Initialize Hall Reverb
  hall_reverb.Init(hw.AudioSampleRate());
  hall_reverb.SetFeedback(0.95f); // Higher feedback for longer hall decay

  // Initialize Spring Reverb
  spring_reverb.Init(hw.AudioSampleRate());
  spring_reverb.SetDecay(0.7f); // Spring decay
  spring_reverb.SetMix(1.0f);   // 100% wet - it'll be mixed with Knob 1
  spring_reverb.SetDamping(7000.0f); // High-frequency damping

  Settings defaultSettings = {
    SETTINGS_VERSION, // version
    plate_decay, // decay
    plate_tank_diffusion, // diffusion
    plate_input_damp_high, // input_cutoff_freq
    plate_tank_damp_high, // tank_cutoff_freq
    plate_tank_mod_speed_pos, // tank_mod_speed_pos
    plate_tank_mod_depth_pos, // tank_mod_depth_pos
    plate_tank_mod_shape_pos, // tank_mod_shape_pos
    plate_pre_delay, // pre_delay
    MS_MODE_MIMO, // mono_stereo_mode
    true,         // bypass_reverb
    true,         // bypass_tremolo
    true,         // bypass_delay
  };
  SavedSettings.Init(defaultSettings);

  loadSettings();

  Funbox::FootswitchCallbacks callbacks = {
    .HandleNormalPress = handleNormalPress,
    .HandleDoublePress = handleDoublePress,
    .HandleLongPress = handleLongPress
  };
  hw.RegisterFootswitchCallbacks(&callbacks);

  hw.StartAdc();
  hw.ProcessAllControls();
  if (hw.switches[Funbox::FOOTSWITCH_2].RawState()) {
    is_factory_reset_mode = true;
  } else {
    hw.StartAudio(AudioCallback);
  }
  
  while (true) {
    if(trigger_settings_save) {
			SavedSettings.Save(); // Writing locally stored settings to the external flash
			trigger_settings_save = false;
		} else if (trigger_dfu_mode) {
      // Shut 'er down so the LEDs always flash
      hw.StopAdc();
      hw.StopAudio();

      Led _led_1, _led_2;
      _led_1.Init(hw.seed.GetPin(22), false);
      _led_2.Init(hw.seed.GetPin(23), false);

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
    } else if (is_factory_reset_mode) {
      hw.ProcessAllControls();

      static uint32_t last_led_toggle_time = 0;
      static bool led_toggle = false;
      static uint32_t blink_interval = 1000;
      uint32_t now = System::GetNow();
      uint32_t elapsed_time = now - last_led_toggle_time;
      if (elapsed_time >= blink_interval) {
        // Alternate the LED lights in factory reset mode
        last_led_toggle_time = now;
        led_toggle = !led_toggle;
        led_left.Set(led_toggle ? 1.0f : 0.0f);
        led_right.Set(led_toggle ? 0.0f : 1.0f);
        led_left.Update();
        led_right.Update();
      }

      float low_knob_threshold = 0.05;
      float high_knob_threshold = 0.95;
      float blink_faster_amount = 300; // each stage removes this many MS from the factory reset blinking
      float knob_1_value = p_knob_1.Process();
      if (factory_reset_stage == 0 && knob_1_value >= high_knob_threshold) {
        factory_reset_stage++;
        blink_interval -= blink_faster_amount; // make the blinking faster as a UI feedback that the stage has been met
        quickLedFlash();          
      } else if (factory_reset_stage == 1 && knob_1_value <= low_knob_threshold) {
        factory_reset_stage++;
        blink_interval -= blink_faster_amount; // make the blinking faster as a UI feedback that the stage has been met
        quickLedFlash();          
      } else if (factory_reset_stage == 2 && knob_1_value >= high_knob_threshold) {
        factory_reset_stage++;
        blink_interval -= blink_faster_amount; // make the blinking faster as a UI feedback that the stage has been met
        quickLedFlash();          
      } else if (factory_reset_stage == 3 && knob_1_value <= low_knob_threshold) {
        SavedSettings.RestoreDefaults();
        loadSettings();
        quickLedFlash();          

        hw.StartAudio(AudioCallback);
        factory_reset_stage = 0;
        bypass_delay = true;
        bypass_trem = true;
        pedal_mode = PEDAL_MODE_NORMAL;
        is_factory_reset_mode = false;
      }
    }
    hw.DelayMs(10);
  }
  return 0;
}
