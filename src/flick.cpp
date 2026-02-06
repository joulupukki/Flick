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
#include <math.h>

#if !defined(PLATFORM_funbox) && !defined(PLATFORM_hothouse)
#error "PLATFORM must be set to funbox or hothouse"
#endif

using flick::FlickOscillator;
using flick::Funbox;
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
#define SETTINGS_VERSION 5

Funbox hw;

// Audio config constants
constexpr float SAMPLE_RATE = 48000.0f;
constexpr size_t MAX_DELAY = static_cast<size_t>(SAMPLE_RATE * 2.0f);

// Tremolo constants
constexpr float TREMOLO_SPEED_MIN = 0.2f;   // Minimum tremolo speed in Hz
constexpr float TREMOLO_SPEED_MAX = 16.0f;  // Maximum tremolo speed in Hz
constexpr float TREMOLO_DEPTH_SCALE = 1.0f; // Scale factor for tremolo depth
constexpr float TREMOLO_LED_BRIGHTNESS = 0.4f; // LED brightness when only tremolo is active

// LED constants
constexpr float TAP_TEMPO_BLINK_DUTY_CYCLE = 0.1f; // 10% duty cycle for tap tempo LED

// Delay constants
constexpr float DELAY_TIME_MIN_SECONDS = 0.05f;     // Minimum delay time
constexpr float DELAY_WET_MIX_ATTENUATION = 0.333f; // Attenuation for wet delay signal
constexpr float DELAY_DRY_WET_PERCENT_MAX = 100.0f; // Max value for dry/wet percentage

// Tap tempo constants
constexpr uint32_t TAP_TEMPO_TIMEOUT_MS = 5000;     // Exit tap tempo after 5 seconds
constexpr uint32_t TAP_TEMPO_MIN_INTERVAL_MS = 20;  // Min 20ms = 3000 BPM (enables doubling/slapback)
constexpr uint32_t TAP_TEMPO_MAX_INTERVAL_MS = 4000; // Max 4 seconds = 15 BPM
constexpr float MS_PER_SECOND = 1000.0f;            // Milliseconds per second conversion
constexpr float TAP_TEMPO_SAMPLES_MIN = (TAP_TEMPO_MIN_INTERVAL_MS / MS_PER_SECOND) * SAMPLE_RATE;  // 20ms
constexpr float TAP_TEMPO_SAMPLES_MAX = (TAP_TEMPO_MAX_INTERVAL_MS / MS_PER_SECOND) * SAMPLE_RATE;  // 4s

// DFU mode - both switches
constexpr uint32_t DFU_BOTH_SWITCHES_HOLD_TIME_MS = 5000;  // 5 seconds

// Knob takeover threshold
constexpr float KNOB_TAKEOVER_THRESHOLD = 0.05f;  // 5% movement required for takeover

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
  PEDAL_MODE_EDIT_REVERB,     // Edit mode activated by long-press of the left foot switch
  PEDAL_MODE_EDIT_MONO_STEREO,// Edit mode activated by long-press of the right foot switch
  PEDAL_MODE_TAP_TEMPO        // Tap tempo mode activated by double-press of left foot switch
};

enum DelaySubdivision {
  DELAY_SUBDIV_QUARTER_TRIPLET,  // 0.6666x multiplier (DOWN/LEFT) - 2/3 of quarter note
  DELAY_SUBDIV_NORMAL,            // 1.0x multiplier (MIDDLE) - quarter note
  DELAY_SUBDIV_DOTTED_EIGHTH,    // 0.75x multiplier (UP/RIGHT) - 3/4 of quarter note
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
  float tank_mod_speed;
  float tank_mod_depth;
  float tank_mod_shape;
  float pre_delay;
  int mono_stereo_mode;
  int makeup_gain_mode;       // Makeup gain setting
  bool bypass_reverb;         // Reverb bypass state (true = bypassed)
  bool bypass_delay;          // Delay bypass state (true = bypassed)
  bool bypass_tremolo;        // Tremolo bypass state (true = bypassed)

	//Overloading the != operator
	//This is necessary as this operator is used in the PersistentStorage source code
	bool operator!=(const Settings& a) const {
    return !(
      a.version == version &&
      a.decay == decay &&
      a.diffusion == diffusion &&
      a.input_cutoff_freq == input_cutoff_freq &&
      a.tank_cutoff_freq == tank_cutoff_freq &&
      a.tank_mod_speed == tank_mod_speed &&
      a.tank_mod_depth == tank_mod_depth &&
      a.tank_mod_shape == tank_mod_shape &&
      a.pre_delay == pre_delay &&
      a.mono_stereo_mode == mono_stereo_mode &&
      a.makeup_gain_mode == makeup_gain_mode &&
      a.bypass_reverb == bypass_reverb &&
      a.bypass_delay == bypass_delay &&
      a.bypass_tremolo == bypass_tremolo
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
  MAKEUP_GAIN_HEAVY,
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

constexpr TremDelMakeUpGain kMakeupGainMap[] = {
  MAKEUP_GAIN_HEAVY,                       // UP (Hothouse) / RIGHT (Funbox)
  MAKEUP_GAIN_NORMAL,                      // MIDDLE
  MAKEUP_GAIN_NONE,                        // DOWN (Hothouse) / LEFT (Funbox)
};

constexpr TremoloMode kTremoloModeMap[] = {
    TREMOLO_SQUARE,     // UP (Hothouse) / RIGHT (Funbox)
    TREMOLO_HARMONIC,   // MIDDLE
    TREMOLO_SINE,       // DOWN (Hothouse) / LEFT (Funbox)
};

constexpr DelaySubdivision kDelaySubdivisionMap[] = {
  DELAY_SUBDIV_DOTTED_EIGHTH,     // UP/RIGHT (0.75x - 3/4 of quarter note)
  DELAY_SUBDIV_NORMAL,            // MIDDLE (1.0x - quarter note)
  DELAY_SUBDIV_QUARTER_TRIPLET,   // DOWN/LEFT (0.6666x - 2/3 of quarter note)
};

constexpr MonoStereoMode kMonoStereoModeMap[] = {
  MS_MODE_SISO,  // UP (Hothouse) / RIGHT (Funbox) - Stereo In, Stereo Out
  MS_MODE_MISO,  // MIDDLE - Mono In, Stereo Out
  MS_MODE_MIMO,  // DOWN (Hothouse) / LEFT (Funbox) - Mono In, Mono Out
};

// Helper structures for soft takeover functionality
// Used to prevent parameter jumps when entering edit modes or switching control sources

// Tracks knob position and implements soft takeover with movement threshold
struct KnobTakeover {
  daisy::AnalogControl* knob;    // Pointer to the analog control (knob)
  float entry_value;              // Knob position when control was suspended
  bool taken_over;                // Whether knob has moved enough to take control

  // Default constructor
  KnobTakeover() : knob(nullptr), entry_value(0.0f), taken_over(false) {}

  // Initialize with direct reference to the analog control
  void init(daisy::AnalogControl& knobRef) {
    knob = &knobRef;
  }

  // Reset takeover state and capture current knob position
  void capture() {
    if (knob) {
      entry_value = knob->Value();
      taken_over = false;
    }
  }

  // Check if knob has moved enough to take over control
  // Returns true if knob is actively controlling (either already taken over or just took over)
  bool checkTakeover(float threshold = KNOB_TAKEOVER_THRESHOLD) {
    if (!knob) return false;

    float currentValue = knob->Value();
    if (!taken_over) {
      if (fabs(currentValue - entry_value) > threshold) {
        taken_over = true;
        return true;  // Just taken over - knob now controls
      }
      return false;   // Not yet taken over - knob doesn't control
    }
    return true;      // Already taken over - knob controls
  }
};

// Tracks switch position and detects changes
struct SwitchChangeDetector {
  Funbox* hw_ptr;                        // Pointer to hardware interface
  Funbox::Toggleswitch switch_index;     // Which toggleswitch this monitors
  int entry_position;                    // Switch position when tracking started
  bool changed;                          // Whether switch has been moved from entry position

  // Default constructor
  SwitchChangeDetector() : hw_ptr(nullptr), switch_index(Funbox::TOGGLESWITCH_1), entry_position(0), changed(false) {}

  // Initialize with hardware reference and toggleswitch index
  void init(Funbox& hwRef, Funbox::Toggleswitch switchIdx) {
    hw_ptr = &hwRef;
    switch_index = switchIdx;
  }

  // Reset change state and capture current switch position
  void capture() {
    if (hw_ptr) {
      entry_position = hw_ptr->GetToggleswitchPosition(switch_index);
      changed = false;
    }
  }

  // Check if switch position has changed from entry position
  // Returns true if switch has been moved (either just changed or previously changed)
  bool checkChange() {
    if (!hw_ptr) return false;

    int currentPosition = hw_ptr->GetToggleswitchPosition(switch_index);
    if (!changed && currentPosition != entry_position) {
      changed = true;
      return true;    // Just changed
    }
    return changed;   // Return current changed state
  }
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

// Tap tempo state
bool tap_tempo_active = false;
uint32_t tap_tempo_last_tap_time = 0;
uint32_t tap_tempo_interval_ms = 0;
float tap_tempo_delay_samples = 0.0f;
bool tap_tempo_controls_delay = false;  // True when tap tempo overrides knob
float tap_tempo_tremolo_freq_hz = 0.0f;
bool tap_tempo_controls_tremolo = false;  // True when tap tempo overrides tremolo knob

// Tap tempo knob takeover for KNOB_4 (delay time) and KNOB_2 (tremolo speed)
KnobTakeover tap_tempo_delay_knob_takeover;    // KNOB_4: Delay time
KnobTakeover tap_tempo_tremolo_knob_takeover;  // KNOB_2: Tremolo speed

// Reverb edit mode soft takeover
// Prevents parameters from jumping when entering edit mode
KnobTakeover reverb_edit_wet_amount_knob;    // Reverb wet amount (preview only, not saved)
KnobTakeover reverb_edit_pre_delay_knob;     // Pre-delay time (0-250ms)
KnobTakeover reverb_edit_decay_knob;         // Reverb decay time
KnobTakeover reverb_edit_diffusion_knob;     // Tank diffusion amount
KnobTakeover reverb_edit_input_cut_knob;     // Input high-cut filter frequency
KnobTakeover reverb_edit_tank_cut_knob;      // Tank high-cut filter frequency
SwitchChangeDetector reverb_edit_mod_speed_switch;  // Tank modulation speed
SwitchChangeDetector reverb_edit_mod_depth_switch;  // Tank modulation depth
SwitchChangeDetector reverb_edit_mod_shape_switch;  // Tank modulation shape

// Master delay time (before subdivision multiplier)
float master_delay_time_samples = 0.0f;

// DFU mode detection
uint32_t both_switches_press_start_time = 0;
bool both_switches_pressed = false;

// Current makeup gain setting (persisted)
TremDelMakeUpGain current_makeup_gain = MAKEUP_GAIN_NORMAL;

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
float plate_pre_delay = 0.;

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
float plate_input_damp_low = 2.87; // approx 100Hz
float plate_input_damp_high = 7.25;

float plate_tank_damp_low = 2.87; // approx 100Hz
float plate_tank_damp_high = 7.25;

float plate_tank_mod_speed = 0.1;
float plate_tank_mod_depth = 0.1;
float plate_tank_mod_shape = 0.25;

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
  plate_tank_mod_speed = local_settings.tank_mod_speed;
  plate_tank_mod_depth = local_settings.tank_mod_depth;
  plate_tank_mod_shape = local_settings.tank_mod_shape;
  plate_pre_delay = local_settings.pre_delay;

  // Validate and load mono-stereo mode
  if (local_settings.mono_stereo_mode < MS_MODE_MIMO ||
      local_settings.mono_stereo_mode > MS_MODE_SISO) {
    mono_stereo_mode = MS_MODE_MIMO;  // Default to MIMO if invalid
  } else {
    mono_stereo_mode = static_cast<MonoStereoMode>(local_settings.mono_stereo_mode);
  }
  updateReverbScales(mono_stereo_mode);

  // Load makeup gain setting
  current_makeup_gain = static_cast<TremDelMakeUpGain>(local_settings.makeup_gain_mode);

  // Validate makeup gain value
  if (current_makeup_gain < MAKEUP_GAIN_NONE ||
      current_makeup_gain > MAKEUP_GAIN_HEAVY) {
    current_makeup_gain = MAKEUP_GAIN_NORMAL;
  }

  // Load bypass states - defensive: default to bypassed (true) on any doubt
  bypass_verb = local_settings.bypass_reverb;
  bypass_delay = local_settings.bypass_delay;
  bypass_trem = local_settings.bypass_tremolo;

  verb.setPreDelay(plate_pre_delay);
  verb.setInputFilterHighCutoffPitch(plate_input_damp_high);
  verb.setDecay(plate_decay);
  verb.setTankDiffusion(plate_tank_diffusion);
  verb.setTankFilterHighCutFrequency(plate_tank_damp_high);
  verb.setTankModSpeed(plate_tank_mod_speed * 8);
  verb.setTankModDepth(plate_tank_mod_depth * 15);
  verb.setTankModShape(plate_tank_mod_shape);
}

void saveSettings() {
	//Reference to local copy of settings stored in flash
	Settings &local_settings = SavedSettings.GetSettings();

  local_settings.version = SETTINGS_VERSION;
  local_settings.decay = plate_decay;
  local_settings.diffusion = plate_tank_diffusion;
  local_settings.input_cutoff_freq = plate_input_damp_high;
  local_settings.tank_cutoff_freq = plate_tank_damp_high;
  local_settings.tank_mod_speed = plate_tank_mod_speed;
  local_settings.tank_mod_depth = plate_tank_mod_depth;
  local_settings.tank_mod_shape = plate_tank_mod_shape;
  local_settings.pre_delay = plate_pre_delay;

	trigger_settings_save = true;
}

void saveMonoStereoSettings() {
  Settings &LocalSettings = SavedSettings.GetSettings();

  LocalSettings.mono_stereo_mode = mono_stereo_mode;
  LocalSettings.makeup_gain_mode = current_makeup_gain;  // Save makeup gain

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
  plate_tank_mod_speed = local_settings.tank_mod_speed;
  plate_tank_mod_depth = local_settings.tank_mod_depth;
  plate_tank_mod_shape = local_settings.tank_mod_shape;
  plate_pre_delay = local_settings.pre_delay;

  verb.setDecay(plate_decay);
  verb.setTankDiffusion(plate_tank_diffusion);
  verb.setInputFilterHighCutoffPitch(plate_input_damp_high);
  verb.setTankFilterHighCutFrequency(plate_tank_damp_high);

  verb.setTankModSpeed(plate_tank_mod_speed * 8);
  verb.setTankModDepth(plate_tank_mod_depth * 15);
  verb.setTankModShape(plate_tank_mod_shape);
  verb.setPreDelay(plate_pre_delay);    
}

/// @brief Restore the mono-stereo settings from the saved settings.
void restoreMonoStereoSettings() {
  Settings &local_settings = SavedSettings.GetSettings();

  mono_stereo_mode = static_cast<MonoStereoMode>(local_settings.mono_stereo_mode);
  current_makeup_gain = static_cast<TremDelMakeUpGain>(local_settings.makeup_gain_mode);  // Restore makeup gain
  updateReverbScales(mono_stereo_mode);
}

// Forward declarations for tap tempo functions
void enterTapTempoMode();
void exitTapTempoMode();
void handleTapTempoTap();
void checkTapTempoTimeout();
void checkDfuModeBothSwitches();
void applyDelaySubdivisionAndSetTargets(float masterDelaySamples);

void handleNormalPress(Funbox::Switches footswitch) {
  // Handle tap tempo mode
  if (pedal_mode == PEDAL_MODE_TAP_TEMPO) {
    if (footswitch == Funbox::FOOTSWITCH_1) {
      // Exit tap tempo mode
      exitTapTempoMode();
      return;
    } else if (footswitch == Funbox::FOOTSWITCH_2) {
      // Tap the tempo
      handleTapTempoTap();
      return;
    }
  }

  // Handle edit reverb mode
  if (pedal_mode == PEDAL_MODE_EDIT_REVERB) {
    // Only save the settings if the RIGHT footswitch is pressed in edit mode.
    // The LEFT footswitch is used to exit edit mode without saving.
    if (footswitch == Funbox::FOOTSWITCH_2) {
      // Save the settings
      saveSettings();
    } else {
      restoreReverbSettings();
    }
    pedal_mode = PEDAL_MODE_NORMAL;
    return;
  }
  
  // Handle mono-stereo edit mode
  if (pedal_mode == PEDAL_MODE_EDIT_MONO_STEREO) {
    // Only save the settings if the RIGHT footswitch is pressed in mono-stereo
    // edit mode. The LEFT footswitch is used to exit mono-stereo edit mode
    // without saving.
    if (footswitch == Funbox::FOOTSWITCH_2) {
      // Save the mono-stereo settings
      saveMonoStereoSettings();
    } else {
      restoreMonoStereoSettings();
    }
    pedal_mode = PEDAL_MODE_NORMAL;
    return;
  }

  // Normal mode bypass toggles
  if (footswitch == Funbox::FOOTSWITCH_1) {
    bypass_verb = !bypass_verb;

    if (bypass_verb) {
      // Clear the reverb tails when the reverb is bypassed so if you
      // turn it back on, it starts fresh and doesn't sound weird.
      verb.clear();
    }
  } else {
    bypass_delay = !bypass_delay;
  }

  // Save bypass state to persistent storage
  saveBypassStates();
}

void handleDoublePress(Funbox::Switches footswitch) {
  // Ignore double presses in edit modes
  if (pedal_mode != PEDAL_MODE_NORMAL) {
    return;
  }

  // When double press is detected, a normal press was already detected and
  // processed, so reverse that right off the bat.
  handleNormalPress(footswitch);

  if (footswitch == Funbox::FOOTSWITCH_1) {
    // Enter tap tempo mode (changed from: reverb edit mode)
    enterTapTempoMode();
  } else if (footswitch == Funbox::FOOTSWITCH_2) {
    // Toggle the trem bypass
    bypass_trem = !bypass_trem;

    // Save bypass state to persistent storage
    saveBypassStates();
  }
}

void handleLongPress(Funbox::Switches footswitch) {
  if (footswitch == Funbox::FOOTSWITCH_1) {
    // Long-press on left footswitch: Enter reverb edit mode
    bypass_verb = false;  // Make sure reverb is ON

    // Initialize soft takeover - capture current knob/switch positions
    // CRITICAL: Must reset state BEFORE changing mode to avoid race condition
    // with audio interrupt seeing new mode but stale takeover state
    reverb_edit_wet_amount_knob.capture();
    reverb_edit_pre_delay_knob.capture();
    reverb_edit_decay_knob.capture();
    reverb_edit_diffusion_knob.capture();
    reverb_edit_input_cut_knob.capture();
    reverb_edit_tank_cut_knob.capture();
    reverb_edit_mod_speed_switch.capture();
    reverb_edit_mod_depth_switch.capture();
    reverb_edit_mod_shape_switch.capture();

    // Change mode LAST - after all state is initialized
    pedal_mode = PEDAL_MODE_EDIT_REVERB;
  } else if (footswitch == Funbox::FOOTSWITCH_2) {
    // Long-press on right footswitch: Enter mono-stereo config

    // Turn on reverb and turn off the other effects
    bypass_verb = false;
    bypass_delay = true;
    bypass_trem = true;
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

void enterTapTempoMode() {
  // CRITICAL: Initialize all state BEFORE changing mode to avoid race condition
  // with audio interrupt seeing new mode but stale state

  tap_tempo_active = true;
  tap_tempo_last_tap_time = System::GetNow();
  // Don't clear existing tap tempo data - allow refinement

  // Set tap tempo control flags based on which effects are currently active
  bool delay_active = !bypass_delay;
  bool tremolo_active = !bypass_trem;

  if (!delay_active && !tremolo_active) {
    // Neither effect active: set tempo for both
    tap_tempo_controls_delay = true;
    tap_tempo_controls_tremolo = true;
  } else if (delay_active && tremolo_active) {
    // Both effects active: set tempo for both
    tap_tempo_controls_delay = true;
    tap_tempo_controls_tremolo = true;
  } else if (delay_active && !tremolo_active) {
    // Only delay active: set tempo for delay only
    tap_tempo_controls_delay = true;
    tap_tempo_controls_tremolo = false;
  } else if (!delay_active && tremolo_active) {
    // Only tremolo active: set tempo for tremolo only
    tap_tempo_controls_delay = false;
    tap_tempo_controls_tremolo = true;
  }

  // Initialize knob takeover - capture current positions
  // Knobs won't take back control until moved >5%
  tap_tempo_delay_knob_takeover.capture();
  tap_tempo_tremolo_knob_takeover.capture();

  // Change mode LAST - after all state is initialized
  pedal_mode = PEDAL_MODE_TAP_TEMPO;
}

void exitTapTempoMode() {
  // Set state before changing mode for consistency
  tap_tempo_active = false;
  pedal_mode = PEDAL_MODE_NORMAL;
}

void handleTapTempoTap() {
  uint32_t currentTime = System::GetNow();

  // Calculate interval from last tap
  if (tap_tempo_last_tap_time > 0) {
    uint32_t interval = currentTime - tap_tempo_last_tap_time;

    // Validate interval is in reasonable range
    if (interval >= TAP_TEMPO_MIN_INTERVAL_MS &&
        interval <= TAP_TEMPO_MAX_INTERVAL_MS) {

      tap_tempo_interval_ms = interval;

      // Convert to delay samples at 48kHz
      tap_tempo_delay_samples = (interval / MS_PER_SECOND) * SAMPLE_RATE;

      // Clamp to valid delay range
      tap_tempo_delay_samples = daisysp::fclamp(tap_tempo_delay_samples,
                                                 TAP_TEMPO_SAMPLES_MIN,
                                                 TAP_TEMPO_SAMPLES_MAX);

      // Convert to tremolo frequency (Hz)
      tap_tempo_tremolo_freq_hz = MS_PER_SECOND / interval;

      // Clamp to tremolo speed range
      tap_tempo_tremolo_freq_hz = daisysp::fclamp(tap_tempo_tremolo_freq_hz, TREMOLO_SPEED_MIN, TREMOLO_SPEED_MAX);

      // Tap tempo control flags are set in enterTapTempoMode() based on which
      // effects were active when entering tap tempo mode. They remain set until
      // the user manually takes control by moving the relevant knob.
      master_delay_time_samples = tap_tempo_delay_samples;
    }
  }

  tap_tempo_last_tap_time = currentTime;
}

void checkTapTempoTimeout() {
  if (pedal_mode == PEDAL_MODE_TAP_TEMPO) {
    uint32_t currentTime = System::GetNow();

    // Exit if no activity for 5 seconds
    if ((currentTime - tap_tempo_last_tap_time) >= TAP_TEMPO_TIMEOUT_MS) {
      exitTapTempoMode();
    }
  }
}

void applyDelaySubdivisionAndSetTargets(float masterDelaySamples) {
  // Get delay subdivision from SWITCH_3
  DelaySubdivision subdivision = kDelaySubdivisionMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_3)];

  // Calculate subdivision multiplier
  float subdivision_multiplier = 1.0f;
  switch (subdivision) {
    case DELAY_SUBDIV_DOTTED_EIGHTH:
      subdivision_multiplier = 0.75f;  // 3/4 of quarter note
      break;
    case DELAY_SUBDIV_QUARTER_TRIPLET:
      subdivision_multiplier = 2.0f / 3.0f;  // 2/3 of quarter note (more precise)
      break;
    case DELAY_SUBDIV_NORMAL:
    default:
      subdivision_multiplier = 1.0f;
      break;
  }

  // Apply subdivision to master time
  float final_delay_time = masterDelaySamples * subdivision_multiplier;

  // Clamp to valid range
  final_delay_time = daisysp::fclamp(final_delay_time, TAP_TEMPO_SAMPLES_MIN, (float)MAX_DELAY);

  // Set delay targets
  delayL.delay_target = final_delay_time;
  delayR.delay_target = final_delay_time;
}

void checkDfuModeBothSwitches() {
  // Check if both footswitches are currently pressed
  bool fs1_pressed = hw.switches[Funbox::FOOTSWITCH_1].Pressed();
  bool fs2_pressed = hw.switches[Funbox::FOOTSWITCH_2].Pressed();

  if (fs1_pressed && fs2_pressed) {
    if (!both_switches_pressed) {
      // Just started pressing both
      both_switches_press_start_time = System::GetNow();
      both_switches_pressed = true;
    } else {
      // Check how long both have been held
      uint32_t hold_duration = System::GetNow() - both_switches_press_start_time;

      if (hold_duration >= DFU_BOTH_SWITCHES_HOLD_TIME_MS) {
        // Enter DFU mode - flash LEDs to indicate
        for (int i = 0; i < 5; i++) {
          led_left.Set(1.0f);
          led_right.Set(0.0f);
          led_left.Update();
          led_right.Update();
          System::Delay(100);

          led_left.Set(0.0f);
          led_right.Set(1.0f);
          led_left.Update();
          led_right.Update();
          System::Delay(100);
        }

        System::ResetToBootloader();
      }
    }
  } else {
    // Reset tracking
    both_switches_pressed = false;
  }
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
  } else if (pedal_mode == PEDAL_MODE_TAP_TEMPO) {
    // Tap tempo mode
    // LED_1: Slow pulse to indicate tap tempo mode
    uint32_t slow_pulse = System::GetNow() % 1000;
    led_left.Set(slow_pulse < 500 ? 1.0f : 0.1f);

    // LED_2: Blink at current tempo (if tempo set)
    if (tap_tempo_interval_ms > 0) {
      uint32_t blink_phase = System::GetNow() % tap_tempo_interval_ms;
      float blink_threshold = tap_tempo_interval_ms * TAP_TEMPO_BLINK_DUTY_CYCLE;

      if (blink_phase < blink_threshold) {
        led_right.Set(1.0f);
      } else {
        led_right.Set(0.1f);  // Dim when off
      }
    } else {
      // No tempo set yet - slow pulse (reuse slow_pulse from above)
      led_right.Set(slow_pulse < 500 ? 1.0f : 0.1f);
    }

    // Apply tap tempo delay time immediately while in tap tempo mode
    applyDelaySubdivisionAndSetTargets(tap_tempo_delay_samples);

    // Also apply tremolo frequency in tap tempo mode
    if (tap_tempo_controls_tremolo) {
      osc.SetFreq(tap_tempo_tremolo_freq_hz);
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

  if (pedal_mode == PEDAL_MODE_NORMAL) {
    // Tremolo speed with tap tempo support and soft takeover
    if (tap_tempo_controls_tremolo) {
      // Tap tempo is controlling - check if knob has taken back control
      if (tap_tempo_tremolo_knob_takeover.checkTakeover()) {
        // Knob has moved >5% - take back control from tap tempo
        tap_tempo_controls_tremolo = false;
        osc.SetFreq(p_trem_speed.Process());
      } else {
        // Knob hasn't moved enough - tap tempo still controls
        osc.SetFreq(tap_tempo_tremolo_freq_hz);
      }
    } else {
      // Normal knob control
      osc.SetFreq(p_trem_speed.Process());
    }

    // Get tremolo mode from SWITCH_2
    TremoloMode tremMode = kTremoloModeMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_2)];

    static float depth = 0;
    depth = daisysp::fclamp(p_trem_depth.Process(), 0.f, 1.f);

    if (tremMode == TREMOLO_HARMONIC) {
      // Harmonic tremolo requires different depth scale to keep it similar to
      // the other modes.
      depth *= 1.25f;
    } else {
      depth *= 0.5f;
    }

    osc.SetAmp(depth);
    dc_offset = 1.f - depth;

    // Set oscillator waveform based on mode (not used for harmonic)
    if (tremMode == TREMOLO_SQUARE) {
      osc.SetWaveform(FlickOscillator::WAVE_SQUARE_ROUNDED);
    } else if (tremMode == TREMOLO_SINE || tremMode == TREMOLO_HARMONIC) {
      osc.SetWaveform(FlickOscillator::WAVE_SIN);
    }
    // For harmonic mode, waveform doesn't matter much (use sine)

    //
    // Delay with subdivision and tap tempo support
    //

    // Determine master delay time source with soft takeover
    if (tap_tempo_controls_delay) {
      // Tap tempo is controlling - check if knob has taken back control
      if (tap_tempo_delay_knob_takeover.checkTakeover()) {
        // Knob has moved >5% - take back control from tap tempo
        tap_tempo_controls_delay = false;
        master_delay_time_samples = p_delay_time.Process();
      } else {
        // Knob hasn't moved enough - tap tempo still controls
        master_delay_time_samples = tap_tempo_delay_samples;
      }
    } else {
      // Normal knob control
      master_delay_time_samples = p_delay_time.Process();
    }

    // Apply subdivision and set delay targets
    applyDelaySubdivisionAndSetTargets(master_delay_time_samples);

    // Feedback unchanged
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
    // Edit mode with soft takeover - parameters only change when controls are moved
    plate_dry = 1.0; // Always use dry 100% in edit mode

    // KNOB_1: Reverb wet amount (not saved, just for preview)
    if (reverb_edit_wet_amount_knob.checkTakeover()) {
      plate_wet = p_verb_amt.Process();
    }

    // KNOB_2: Pre-delay (0-250ms)
    if (reverb_edit_pre_delay_knob.checkTakeover()) {
      plate_pre_delay = p_knob_2.Process() * 0.25;
    }

    // KNOB_3: Decay time
    if (reverb_edit_decay_knob.checkTakeover()) {
      plate_decay = p_knob_3.Process();
    }

    // KNOB_4: Tank diffusion
    if (reverb_edit_diffusion_knob.checkTakeover()) {
      plate_tank_diffusion = p_knob_4.Process();
    }

    // KNOB_5: Input high-cut frequency (0-10 pitch scale)
    if (reverb_edit_input_cut_knob.checkTakeover()) {
      plate_input_damp_high = p_knob_5.Process() * 10.0; // Dattorro takes values for this between 0 and 10
    }

    // KNOB_6: Tank high-cut frequency (0-10 pitch scale)
    if (reverb_edit_tank_cut_knob.checkTakeover()) {
      plate_tank_damp_high = p_knob_6.Process() * 10.0; // Dattorro takes values for this between 0 and 10
    }

    // SWITCH_1: Tank Mod Speed
    static const float tank_mod_speed_values[] = {0.5f, 0.25f, 0.1f};
    if (reverb_edit_mod_speed_switch.checkChange()) {
      int switch1Pos = hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_1);
      plate_tank_mod_speed = tank_mod_speed_values[switch1Pos];
    }

    // SWITCH_2: Tank Mod Depth
    static const float tank_mod_depth_values[] = {0.5f, 0.25f, 0.1f};
    if (reverb_edit_mod_depth_switch.checkChange()) {
      int switch2Pos = hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_2);
      plate_tank_mod_depth = tank_mod_depth_values[switch2Pos];
    }

    // SWITCH_3: Tank Mod Shape
    static const float tank_mod_shape_values[] = {0.5f, 0.25f, 0.1f};
    if (reverb_edit_mod_shape_switch.checkChange()) {
      int switch3Pos = hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_3);
      plate_tank_mod_shape = tank_mod_shape_values[switch3Pos];
    }

    // Always apply current parameter values to reverb engine
    verb.setDecay(plate_decay);
    verb.setTankDiffusion(plate_tank_diffusion);
    verb.setInputFilterHighCutoffPitch(plate_input_damp_high);
    verb.setTankFilterHighCutFrequency(plate_tank_damp_high);

    verb.setTankModSpeed(plate_tank_mod_speed * 8);
    verb.setTankModDepth(plate_tank_mod_depth * 15);
    verb.setTankModShape(plate_tank_mod_shape);
    verb.setPreDelay(plate_pre_delay);    
  } else if (pedal_mode == PEDAL_MODE_EDIT_MONO_STEREO) {
    // Mono-Stereo edit mode
    // SWITCH_3: Read mono-stereo mode using position-indexed map
    mono_stereo_mode = kMonoStereoModeMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_3)];
    updateReverbScales(mono_stereo_mode);

    // SWITCH_2: Read makeup gain setting (moved from normal mode SWITCH_3)
    current_makeup_gain = kMakeupGainMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_2)];
  }

  for (size_t i = 0; i < size; ++i) {
    float dry_L = in[0][i];
    float dry_R = in[1][i];
    float s_L, s_R;
    s_L = dry_L;
    if (mono_stereo_mode == MS_MODE_MIMO || mono_stereo_mode == MS_MODE_MISO) {
      // Use the mono signal (L) for both channels in MIMO and MISO modes
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

    // Get makeup gain values (now from global variable)
    float trem_make_up_gain = 1.0f;
    float delay_make_up_gain = 1.0f;

    switch (current_makeup_gain) {
      case MAKEUP_GAIN_HEAVY:
        trem_make_up_gain = 1.6f;   // +4dB for tremolo
        delay_make_up_gain = 2.0f;  // +6dB for delay
        break;
      case MAKEUP_GAIN_NORMAL:
        trem_make_up_gain = 1.2f;   // +1.6dB for tremolo
        delay_make_up_gain = 1.66f; // +4.4dB for delay
        break;
      case MAKEUP_GAIN_NONE:
      default:
        trem_make_up_gain = 1.0f;
        delay_make_up_gain = 1.0f;
        break;
    }

    if (!bypass_delay) {
      float mixL = 0;
      float mixR = 0;
      float fdrywet = delay_drywet / DELAY_DRY_WET_PERCENT_MAX;

      // update delayline with feedback
      float sigL = delayL.Process(s_L);
      float sigR = delayR.Process(s_R);
      mixL += sigL;
      mixR += sigR;

      // apply drywet and attenuate
      s_L = fdrywet * mixL * DELAY_WET_MIX_ATTENUATION + (1.0f - fdrywet) * s_L * delay_make_up_gain;
      s_R = fdrywet * mixR * DELAY_WET_MIX_ATTENUATION + (1.0f - fdrywet) * s_R * delay_make_up_gain;
    }

    if (!bypass_trem) {
      // Get tremolo mode from SWITCH_2 (in normal mode)
      TremoloMode tremMode = TREMOLO_SINE;  // Default
      if (pedal_mode == PEDAL_MODE_NORMAL) {
        tremMode = kTremoloModeMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_2)];
      }

      // Generate LFO sample
      float lfoSample = osc.Process();

      // DC offset to make LFO unipolar (0 to peak) - for LED display
      trem_val = dc_offset + lfoSample;

      // Apply tremolo based on mode
      if (tremMode == TREMOLO_HARMONIC) {
        // === HARMONIC TREMOLO ===

        // Process left channel
        float lowL = harmonic_trem_lpf_L.Process(s_L);
        float highL = harmonic_trem_hpf_L.Process(s_L);  // 90° phase difference

        // Apply tremolo with opposite phase to each band
        float lowModL = lowL * (1.0f + lfoSample);
        float highModL = highL * (1.0f - lfoSample);  // Inverted phase
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
        verb.setDecay(plate_decay);
        verb.setTankDiffusion(plate_tank_diffusion);
        verb.setInputFilterHighCutoffPitch(plate_input_damp_high);
        verb.setTankFilterHighCutFrequency(plate_tank_damp_high);
        verb.setTankModSpeed(plate_tank_mod_speed * 8);
        verb.setTankModDepth(plate_tank_mod_depth * 15);
        verb.setTankModShape(plate_tank_mod_shape);
        verb.setPreDelay(plate_pre_delay);
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

  p_trem_speed.Init(hw.knobs[Funbox::KNOB_2], TREMOLO_SPEED_MIN, TREMOLO_SPEED_MAX, Parameter::LOGARITHMIC);
  p_trem_depth.Init(hw.knobs[Funbox::KNOB_3], 0.0f, TREMOLO_DEPTH_SCALE, Parameter::LINEAR);

  p_delay_time.Init(hw.knobs[Funbox::KNOB_4], hw.AudioSampleRate() * DELAY_TIME_MIN_SECONDS, MAX_DELAY, Parameter::LOGARITHMIC);
  p_delay_feedback.Init(hw.knobs[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
  p_delay_amt.Init(hw.knobs[Funbox::KNOB_6], 0.0f, 100.0f, Parameter::LINEAR);

  delMemL.Init();
  delMemR.Init();
  delayL.del = &delMemL;
  delayR.del = &delMemR;

  // Initialize knob takeover and switch change detection for soft takeover
  tap_tempo_delay_knob_takeover.init(hw.knobs[Funbox::KNOB_4]);
  tap_tempo_tremolo_knob_takeover.init(hw.knobs[Funbox::KNOB_2]);
  reverb_edit_wet_amount_knob.init(hw.knobs[Funbox::KNOB_1]);
  reverb_edit_pre_delay_knob.init(hw.knobs[Funbox::KNOB_2]);
  reverb_edit_decay_knob.init(hw.knobs[Funbox::KNOB_3]);
  reverb_edit_diffusion_knob.init(hw.knobs[Funbox::KNOB_4]);
  reverb_edit_input_cut_knob.init(hw.knobs[Funbox::KNOB_5]);
  reverb_edit_tank_cut_knob.init(hw.knobs[Funbox::KNOB_6]);
  reverb_edit_mod_speed_switch.init(hw, Funbox::TOGGLESWITCH_1);
  reverb_edit_mod_depth_switch.init(hw, Funbox::TOGGLESWITCH_2);
  reverb_edit_mod_shape_switch.init(hw, Funbox::TOGGLESWITCH_3);

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
  verb.setInputFilterLowCutoffPitch(plate_input_damp_low);
  verb.setTankFilterLowCutFrequency(plate_tank_damp_low);

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
    plate_tank_mod_speed, // tank_mod_speed
    plate_tank_mod_depth, // tank_mod_depth
    plate_tank_mod_shape, // tank_mod_shape
    plate_pre_delay, // pre_delay
    MS_MODE_MIMO,               // mono_stereo_mode
    MAKEUP_GAIN_NORMAL,         // makeup_gain_mode
    true,                       // bypass_reverb (defensive default: bypassed)
    true,                       // bypass_delay (defensive default: bypassed)
    true                        // bypass_tremolo (defensive default: bypassed)
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
    // Check for tap tempo timeout
    checkTapTempoTimeout();

    // Check for DFU mode (both switches held)
    checkDfuModeBothSwitches();

    if(trigger_settings_save) {
			SavedSettings.Save(); // Writing locally stored settings to the external flash
			trigger_settings_save = false;
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

    // Call System::ResetToBootloader() if FOOTSWITCH_1 is pressed for 2 seconds
    if (pedal_mode == PEDAL_MODE_NORMAL) {
      hw.CheckResetToBootloader();
    }
  }
  return 0;
}
