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

/**
 * flick.cpp - Main orchestrator for Flick digital guitar pedal
 *
 * ARCHITECTURE OVERVIEW:
 *
 * This file serves as the UX orchestrator - it handles hardware I/O, user
 * interactions, operational modes, and coordinates the effects pipeline.
 * The actual DSP processing is delegated to modular, hardware-independent
 * effect classes:
 *
 * EFFECTS MODULES (hardware-independent DSP):
 * - DelayEffect:      Stereo delay with feedback (delay_effect.h/cpp)
 * - TremoloEffect:    Base class for tremolo algorithms (tremolo_effect.h/cpp)
 *   - SineTremolo:    Smooth amplitude modulation
 *   - SquareTremolo:  Choppy opto-style tremolo
 *   - HarmonicTremolo: Band-split with opposite-phase modulation + EQ
 * - ReverbEffect:     Base class for reverb algorithms (reverb_effect.h)
 *   - PlateReverb:    Dattorro algorithm (plate_reverb.h/cpp)
 *   - HallReverb:     Schroeder algorithm (hall_reverb.h/cpp)
 *   - SpringReverb:   Digital waveguide (spring_reverb.h/cpp)
 *
 * ORCHESTRATOR RESPONSIBILITIES:
 * - Read hardware controls (knobs, switches, footswitches)
 * - Manage operational modes (normal, tap tempo, edit reverb, device settings)
 * - Handle parameter capture (soft takeover for edit modes)
 * - Calculate derived values (delay timing from taps, tremolo depth scaling)
 * - Manage bypass states and LED feedback
 * - Mix dry/wet signals (effect modules return wet-only)
 * - Persist settings to flash
 * - Coordinate the audio processing pipeline
 *
 * AUDIO SIGNAL FLOW:
 * Input → Notch Filters → Delay → Tremolo → Reverb → Output
 *
 * Each effect is called via its polymorphic interface (e.g., current_tremolo->ProcessSample()).
 * Effects have no knowledge of knobs, switches, or UI - they're pure DSP.
 */

#include "daisy.h"
#include "daisysp.h"
#include "flick_oscillator.h"
#include "flick_filters.hpp"
#include "daisy_hardware.h"
#include "delay_effect.h"
#include "tremolo_effect.h"
#include "reverb_effect.h"
#include "plate_reverb.h"
#include "hall_reverb.h"
#include "spring_reverb.h"
#include "parameter_capture.h"
#include <math.h>

#if !defined(PLATFORM_funbox) && !defined(PLATFORM_hothouse)
#error "PLATFORM must be set to funbox or hothouse"
#endif

using flick::DelayEffect;
using flick::TremoloEffect;
using flick::SineTremolo;
using flick::SquareTremolo;
using flick::HarmonicTremolo;
using flick::ReverbEffect;
using flick::PlateReverb;
using flick::HallReverb;
using flick::SpringReverb;
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

// ============================================================================
// CONSTANTS
// ============================================================================

/// Increment this when changing the settings struct so the software will know
/// to reset to defaults if this ever changes.
#define SETTINGS_VERSION 8

// Audio configuration
constexpr float SAMPLE_RATE = 48000.0f;
constexpr size_t MAX_DELAY = static_cast<size_t>(SAMPLE_RATE * 2.0f);

// Filter frequency constants (notch filters always active)
constexpr float NOTCH_1_FREQ = 6020.0f;   // Daisy Seed resonance notch
constexpr float NOTCH_2_FREQ = 12278.0f;  // Daisy Seed resonance notch

// Reverb constants (Dattorro plate reverb scaling)
constexpr float PLATE_PRE_DELAY_SCALE = 0.25f;
constexpr float PLATE_DAMP_SCALE = 10.0f;

constexpr float PLATE_TANK_MOD_SPEED_VALUES[] = {0.5f, 0.25f, 0.1f};
constexpr float PLATE_TANK_MOD_DEPTH_VALUES[] = {0.5f, 0.25f, 0.1f};
constexpr float PLATE_TANK_MOD_SHAPE_VALUES[] = {0.5f, 0.25f, 0.1f};

constexpr float PLATE_TANK_MOD_SPEED_SCALE = 8.0f;  // Multiplier for tank modulation speed
constexpr float PLATE_TANK_MOD_DEPTH_SCALE = 15.0f; // Multiplier for tank modulation depth

// Tremolo constants
constexpr float TREMOLO_SPEED_MIN = 0.2f;              // Minimum tremolo speed in Hz
constexpr float TREMOLO_SPEED_MAX = 16.0f;             // Maximum tremolo speed in Hz
constexpr float TREMOLO_DEPTH_SCALE = 1.0f;            // Scale factor for tremolo depth
constexpr float TREMOLO_LED_BRIGHTNESS = 0.4f;         // LED brightness when only tremolo is active

// Harmonic tremolo filter cutoffs (taken from Fender 6G12-A schematic)
constexpr float HARMONIC_TREMOLO_LPF_CUTOFF = 144.0f; // 220K and 5nF LPF
constexpr float HARMONIC_TREMOLO_HPF_CUTOFF = 636.0f; // 1M and 250pF HPF

// EQ-shaping filters for harmonic tremolo
constexpr float HARMONIC_TREM_EQ_HPF1_CUTOFF = 63.0f;
constexpr float HARMONIC_TREM_EQ_LPF1_CUTOFF = 11200.0f;
constexpr float HARMONIC_TREM_EQ_PEAK1_FREQ = 7500.0f;
constexpr float HARMONIC_TREM_EQ_PEAK1_GAIN = -3.37f; // in dB
constexpr float HARMONIC_TREM_EQ_PEAK1_Q = 0.263f;
constexpr float HARMONIC_TREM_EQ_PEAK2_FREQ = 254.0f;
constexpr float HARMONIC_TREM_EQ_PEAK2_GAIN = 2.0f; // in dB
constexpr float HARMONIC_TREM_EQ_PEAK2_Q = 0.707f;
constexpr float HARMONIC_TREM_EQ_LOW_SHELF_FREQ = 37.0f;
constexpr float HARMONIC_TREM_EQ_LOW_SHELF_GAIN = -10.5f; // in dB
constexpr float HARMONIC_TREM_EQ_LOW_SHELF_Q = 1.0f;      // Shelf slope

// Delay constants
constexpr float DELAY_TIME_MIN_SECONDS = 0.05f;
constexpr float DELAY_WET_MIX_ATTENUATION = 0.333f; // Attenuation for wet delay signal
constexpr float DELAY_DRY_WET_PERCENT_MAX = 100.0f; // Max value for dry/wet percentage

// Tap tempo constants
constexpr uint32_t TAP_TEMPO_TIMEOUT_MS = 4000; // Auto-exit after 4 seconds of no taps
constexpr int TAP_FLASH_CALLBACKS = 300;         // ~50ms LED flash at 6000 callbacks/sec

// Audio signal levels
constexpr float MINUS_18DB_GAIN = 0.12589254f;
constexpr float MINUS_20DB_GAIN = 0.1f;

// ============================================================================
// ENUMS AND SWITCH MAPPINGS
// ============================================================================
// Toggle switch orientation note: Hothouse = vertical UP/DOWN, Funbox = horizontal LEFT/RIGHT
// Logical positions: index 0 = UP/RIGHT, index 1 = MIDDLE, index 2 = DOWN/LEFT

// Pedal operational modes
enum PedalMode {
  PEDAL_MODE_NORMAL,
  PEDAL_MODE_EDIT_REVERB,           // Activated by long-press of left footswitch
  PEDAL_MODE_EDIT_DEVICE_SETTINGS,  // Activated by long-press of right footswitch
  PEDAL_MODE_TAP_TEMPO              // Activated by double-press of left footswitch
};

// Mono/Stereo I/O mode (Toggle Switch 3 in device settings)
enum MonoStereoMode {
  MS_MODE_MIMO, // Mono In, Mono Out        // TOGGLESWITCH_LEFT
  MS_MODE_MISO, // Mono In, Stereo Out      // TOGGLESWITCH_MIDDLE
  MS_MODE_SISO, // Stereo In, Stereo Out    // TOGGLESWITCH_RIGHT
};

constexpr MonoStereoMode kMonoStereoMap[] = {
  MS_MODE_SISO,  // UP (Hothouse) / RIGHT (Funbox)
  MS_MODE_MISO,  // MIDDLE
  MS_MODE_MIMO,  // DOWN (Hothouse) / LEFT (Funbox)
};

// Reverb algorithm selection (Toggle Switch 1 in normal mode)
enum ReverbType {
  REVERB_PLATE,
  REVERB_SPRING,
  REVERB_HALL,
  REVERB_DEFAULT = REVERB_PLATE
};

constexpr ReverbType kReverbTypeMap[] = {
  REVERB_SPRING,  // UP (Hothouse) / RIGHT (Funbox)
  REVERB_PLATE,   // MIDDLE
  REVERB_HALL,    // DOWN (Hothouse) / LEFT (Funbox)
};

// Reverb dry/wet knob behavior (Toggle Switch 1 in device settings)
enum ReverbKnobMode {
  REVERB_KNOB_ALL_DRY,
  REVERB_KNOB_DRY_WET_MIX,
  REVERB_KNOB_ALL_WET,
};

constexpr ReverbKnobMode kReverbKnobMap[] = {
  REVERB_KNOB_ALL_DRY,     // UP (Hothouse) / RIGHT (Funbox)
  REVERB_KNOB_DRY_WET_MIX, // MIDDLE
  REVERB_KNOB_ALL_WET,      // DOWN (Hothouse) / LEFT (Funbox)
};

// Tremolo algorithm selection (Toggle Switch 2 in normal mode)
enum TremoloMode {
  TREMOLO_SINE,      // Sine wave tremolo (LEFT)
  TREMOLO_HARMONIC,  // Harmonic tremolo (MIDDLE)
  TREMOLO_SQUARE,    // Opto/Square wave tremolo (RIGHT)
};

constexpr TremoloMode kTremoloModeMap[] = {
  TREMOLO_SQUARE,    // UP (Hothouse) / RIGHT (Funbox)
  TREMOLO_HARMONIC,  // MIDDLE
  TREMOLO_SINE,      // DOWN (Hothouse) / LEFT (Funbox)
};

// Makeup gain for delay/tremolo interaction
enum TremDelMakeUpGain {
  MAKEUP_GAIN_NONE,
  MAKEUP_GAIN_NORMAL,
};

// Delay timing subdivision (Toggle Switch 3 in normal mode)
enum DelayTimingMode {
  DELAY_TIMING_DOTTED_EIGHTH,  // 3/4 of knob value
  DELAY_TIMING_QUARTER,        // 1x knob value (straight)
  DELAY_TIMING_TRIPLET,        // 1/3 of knob value (3 echoes per beat)
};

constexpr DelayTimingMode kDelayTimingMap[] = {
  DELAY_TIMING_DOTTED_EIGHTH,  // UP (Hothouse) / RIGHT (Funbox)
  DELAY_TIMING_QUARTER,        // MIDDLE
  DELAY_TIMING_TRIPLET,        // DOWN (Hothouse) / LEFT (Funbox)
};

constexpr float kDelayTimingMultiplier[] = {
  0.75f,    // DELAY_TIMING_DOTTED_EIGHTH (index 0)
  1.0f,     // DELAY_TIMING_QUARTER (index 1)
  0.6666f,  // DELAY_TIMING_TRIPLET (index 2, 2/3 — quarter note triplets)
};

// Phase inversion mode (Toggle Switch 2 in device settings)
enum PolarityMode {
  POLARITY_INVERT_RIGHT,
  POLARITY_NORMAL,
  POLARITY_INVERT_LEFT,
};

constexpr PolarityMode kPolarityMap[] = {
  POLARITY_INVERT_RIGHT,  // UP (Hothouse) / RIGHT (Funbox)
  POLARITY_NORMAL,        // MIDDLE
  POLARITY_INVERT_LEFT,   // DOWN (Hothouse) / LEFT (Funbox)
};

// ============================================================================
// STRUCTS
// ============================================================================

// Persistent settings stored in QSPI flash
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
  int polarity_mode;
  int reverb_knob_mode;
  bool bypass_reverb;
  bool bypass_tremolo;
  bool bypass_delay;
  float tapped_delay_samples; // Persisted tap tempo delay time in samples (0 = use knob)

  // Overloading the != operator
  // This is necessary as this operator is used in the PersistentStorage source code
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
      a.polarity_mode == polarity_mode &&
      a.reverb_knob_mode == reverb_knob_mode &&
      a.bypass_reverb == bypass_reverb &&
      a.bypass_tremolo == bypass_tremolo &&
      a.bypass_delay == bypass_delay &&
      a.tapped_delay_samples == tapped_delay_samples
    );
  }
};

// Effect bypass states grouped into a single struct
struct BypassState {
  bool reverb  = true;
  bool tremolo = true;
  bool delay   = true;
};

// ============================================================================
// REVERB ORCHESTRATOR STATE
// ============================================================================
// The reverb effects (PlateReverb, HallReverb, SpringReverb) are DSP-only
// modules with no knowledge of hardware. This orchestrator structure manages
// UI state, mixing, current algorithm selection, and plate reverb parameter
// values that are passed to the effects.

struct ReverbOrchestrator {
  // Current algorithm selection
  ReverbType current_type = REVERB_PLATE;

  // Reverb knob mode (device setting - affects dry/wet mixing behavior)
  ReverbKnobMode knob_mode = REVERB_KNOB_DRY_WET_MIX;

  // Mixing control (orchestrator responsibility - dry/wet balance)
  float dry = 1.0f;
  float wet = 0.5f;

  // Plate reverb parameters (editable in reverb edit mode, saved to flash)
  // These are UI-level values that get passed to PlateReverb via setters
  float plate_pre_delay = 0.0f;           // Pre-delay (scaled before passing to effect)
  float plate_decay = 0.8f;               // Decay amount
  float plate_diffusion = 0.85f;          // Tank diffusion
  float plate_input_damp_high = 7.25f / PLATE_DAMP_SCALE;  // Input high-cut (~3000Hz)
  float plate_tank_damp_high = 7.25f / PLATE_DAMP_SCALE;   // Tank high-cut (~3520Hz)

  // Tank modulation switch positions (0, 1, or 2) - mapped to values when applied
  int plate_mod_speed_pos = 2;  // Position 2 = 0.1 (from PLATE_TANK_MOD_SPEED_VALUES)
  int plate_mod_depth_pos = 2;  // Position 2 = 0.1 (from PLATE_TANK_MOD_DEPTH_VALUES)
  int plate_mod_shape_pos = 1;  // Position 1 = 0.25 (from PLATE_TANK_MOD_SHAPE_VALUES)
};

// ============================================================================
// HARDWARE + PERSISTENT STORAGE
// ============================================================================

Funbox hw;

// Persistent Storage Declaration. Using type Settings and passed the device's qspi handle
PersistentStorage<Settings> SavedSettings(hw.seed.qspi);

// ============================================================================
// EFFECT INSTANCES — Hardware-Independent DSP Modules
// ============================================================================

// SDRAM delay buffers (externally allocated, passed to DelayEffect)
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delMemL;
DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS delMemR;

// Reverb effects (polymorphic - algorithm selected at runtime via toggle switch)
// current_reverb points to whichever reverb is active (plate/hall/spring)
ReverbEffect* current_reverb = nullptr;
PlateReverb plate_reverb;    // Dattorro algorithm (lush, complex)
HallReverb hall_reverb;      // Schroeder algorithm (spacious)
SpringReverb spring_reverb;  // Digital waveguide (vintage character)

// Tremolo effects (polymorphic - switch at runtime)
TremoloEffect* current_tremolo = nullptr;
SineTremolo sine_tremolo;
SquareTremolo square_tremolo;
HarmonicTremolo harmonic_tremolo;

// Delay effect
DelayEffect delay_effect;

// Notch filters to remove Daisy Seed resonant frequencies (always active)
PeakingEQ notch1_L;
PeakingEQ notch1_R;
PeakingEQ notch2_L;
PeakingEQ notch2_R;

// ============================================================================
// UI HARDWARE
// ============================================================================

Led led_left, led_right;

// ============================================================================
// GLOBAL STATE
// ============================================================================

// Operational modes
PedalMode pedal_mode = PEDAL_MODE_NORMAL;
MonoStereoMode mono_stereo_mode = MS_MODE_MIMO;
PolarityMode polarity_mode = POLARITY_NORMAL;

// Effect bypass states
BypassState bypass;

// Reverb orchestrator
ReverbOrchestrator reverb;

// Delay state
float delay_time_target = 0.0f;  // Track delay time for tap tempo extraction
int delay_drywet;

// Reverb mixing scale factors (updated when mono/stereo mode changes)
float reverb_dry_scale_factor = 1.0f;
float reverb_reverse_scale_factor = 1.0f;

// Audio signal temporaries (used in the per-sample loop in AudioCallback)
float left_input = 0.f;
float right_input = 0.f;
float left_output = 0.f;
float right_output = 0.f;

float input_amplification = 1.0f; // This isn't really used yet

// Control flags for main loop
bool trigger_settings_save = false;
bool trigger_dfu_mode = false;

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

// ============================================================================
// PARAMETER OBJECTS
// ============================================================================

Parameter p_verb_amt;
Parameter p_trem_speed, p_trem_depth;
Parameter p_delay_time, p_delay_feedback, p_delay_amt;

// Raw knob parameters (0.0-1.0 linear) used in reverb edit mode
Parameter p_knob_1, p_knob_2, p_knob_3, p_knob_4, p_knob_5, p_knob_6;

// Reverb edit mode parameter captures (soft takeover)
KnobCapture p_knob_2_capture(p_knob_2);
KnobCapture p_knob_3_capture(p_knob_3);
KnobCapture p_knob_4_capture(p_knob_4);
KnobCapture p_knob_5_capture(p_knob_5);
KnobCapture p_knob_6_capture(p_knob_6);
SwitchCapture p_sw1_capture(hw, Funbox::TOGGLESWITCH_1);
SwitchCapture p_sw2_capture(hw, Funbox::TOGGLESWITCH_2);
SwitchCapture p_sw3_capture(hw, Funbox::TOGGLESWITCH_3);

// ============================================================================
// TAP TEMPO STATE
// ============================================================================
// TapTempoState is defined and instantiated here (after p_knob_4) because
// the inline constructor references p_knob_4 in its member initializer list,
// requiring p_knob_4 to be visible at the point of the struct definition.

struct TapTempoState {
  uint32_t tap_timestamps[3] = {0, 0, 0}; // Last 3 tap timestamps for averaging
  int tap_count = 0;                       // Number of taps recorded (0-3)
  float tapped_delay_samples = 0.0f;      // Calculated delay time from taps (in samples)
  float tapped_tempo_ms = 0.0f;           // Calculated tempo in milliseconds
  uint32_t last_tap_time = 0;             // Timestamp of last tap (for timeout)
  int tap_flash_counter = 0;              // LED flash animation counter
  bool just_exited_tap_tempo = false;     // Flag to prevent spurious footswitch events on exit
  float knob_baseline = -1.0f;            // Delay knob position when entering tap tempo mode
                                          // (used to detect manual override)
  KnobCapture delay_knob_capture;         // Soft takeover for delay knob in tap tempo mode

  TapTempoState() : delay_knob_capture(p_knob_4) {}
};

TapTempoState tap_tempo;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/// @brief Reverse-lookup: find the switch position (0/1/2) for a given value
/// in a 3-element mapping array.
template<typename T>
int switchPosForValue(const T (&map)[3], T value) {
  for (int i = 0; i < 3; i++) {
    if (map[i] == value) return i;
  }
  return 1; // default to MIDDLE
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

// ============================================================================
// REVERB PARAMETER MANAGEMENT
// ============================================================================

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

/**
 * @brief Updates plate reverb parameters (now encapsulated in PlateReverb class).
 * This function is kept for compatibility but delegates to PlateReverb setters.
 */
void updatePlateReverbParameters() {
  plate_reverb.SetDecay(reverb.plate_decay);
  plate_reverb.SetDiffusion(reverb.plate_diffusion);
  plate_reverb.SetInputHighCut(reverb.plate_input_damp_high);
  plate_reverb.SetTankHighCut(reverb.plate_tank_damp_high);
  plate_reverb.SetTankModSpeed(PLATE_TANK_MOD_SPEED_VALUES[reverb.plate_mod_speed_pos]);
  plate_reverb.SetTankModDepth(PLATE_TANK_MOD_DEPTH_VALUES[reverb.plate_mod_depth_pos]);
  plate_reverb.SetTankModShape(PLATE_TANK_MOD_SHAPE_VALUES[reverb.plate_mod_shape_pos]);
  plate_reverb.SetPreDelay(reverb.plate_pre_delay);
}

// ============================================================================
// SETTINGS MANAGEMENT
// ============================================================================

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

  reverb.plate_decay = local_settings.decay;
  reverb.plate_diffusion = local_settings.diffusion;
  reverb.plate_input_damp_high = local_settings.input_cutoff_freq;
  reverb.plate_tank_damp_high = local_settings.tank_cutoff_freq;
  reverb.plate_mod_speed_pos = local_settings.tank_mod_speed_pos;
  reverb.plate_mod_depth_pos = local_settings.tank_mod_depth_pos;
  reverb.plate_mod_shape_pos = local_settings.tank_mod_shape_pos;
  reverb.plate_pre_delay = local_settings.pre_delay;
  mono_stereo_mode = static_cast<MonoStereoMode>(local_settings.mono_stereo_mode);
  polarity_mode = static_cast<PolarityMode>(local_settings.polarity_mode);
  reverb.knob_mode = static_cast<ReverbKnobMode>(local_settings.reverb_knob_mode);
  updateReverbScales(mono_stereo_mode);

  bypass.reverb = local_settings.bypass_reverb;
  bypass.tremolo = local_settings.bypass_tremolo;
  bypass.delay = local_settings.bypass_delay;
  tap_tempo.tapped_delay_samples = local_settings.tapped_delay_samples;

  updatePlateReverbParameters();
}

void saveSettings() {
  // Reference to local copy of settings stored in flash
  Settings &local_settings = SavedSettings.GetSettings();

  local_settings.version = SETTINGS_VERSION;
  local_settings.decay = reverb.plate_decay;
  local_settings.diffusion = reverb.plate_diffusion;
  local_settings.input_cutoff_freq = reverb.plate_input_damp_high;
  local_settings.tank_cutoff_freq = reverb.plate_tank_damp_high;
  local_settings.tank_mod_speed_pos = reverb.plate_mod_speed_pos;
  local_settings.tank_mod_depth_pos = reverb.plate_mod_depth_pos;
  local_settings.tank_mod_shape_pos = reverb.plate_mod_shape_pos;
  local_settings.pre_delay = reverb.plate_pre_delay;

  trigger_settings_save = true;
}

void saveDeviceSettings() {
  Settings &local_settings = SavedSettings.GetSettings();

  local_settings.mono_stereo_mode = mono_stereo_mode;
  local_settings.polarity_mode = polarity_mode;
  local_settings.reverb_knob_mode = reverb.knob_mode;

  trigger_settings_save = true;
}

void saveBypassStates() {
  Settings &local_settings = SavedSettings.GetSettings();

  local_settings.bypass_reverb = bypass.reverb;
  local_settings.bypass_tremolo = bypass.tremolo;
  local_settings.bypass_delay = bypass.delay;
  local_settings.tapped_delay_samples = tap_tempo.tapped_delay_samples;

  trigger_settings_save = true;
}

/// @brief Restore the reverb settings from the saved settings.
void restoreReverbSettings() {
  Settings &local_settings = SavedSettings.GetSettings();

  reverb.plate_decay = local_settings.decay;
  reverb.plate_diffusion = local_settings.diffusion;
  reverb.plate_input_damp_high = local_settings.input_cutoff_freq;
  reverb.plate_tank_damp_high = local_settings.tank_cutoff_freq;
  reverb.plate_mod_speed_pos = local_settings.tank_mod_speed_pos;
  reverb.plate_mod_depth_pos = local_settings.tank_mod_depth_pos;
  reverb.plate_mod_shape_pos = local_settings.tank_mod_shape_pos;
  reverb.plate_pre_delay = local_settings.pre_delay;

  updatePlateReverbParameters();
}

/// @brief Restore the device settings from the saved settings.
void restoreDeviceSettings() {
  Settings &local_settings = SavedSettings.GetSettings();

  mono_stereo_mode = static_cast<MonoStereoMode>(local_settings.mono_stereo_mode);
  polarity_mode = static_cast<PolarityMode>(local_settings.polarity_mode);
  reverb.knob_mode = static_cast<ReverbKnobMode>(local_settings.reverb_knob_mode);
  updateReverbScales(mono_stereo_mode);
}

// ============================================================================
// TAP TEMPO
// ============================================================================

void enterTapTempo() {
  // Warm up p_knob_4's smoothing filter before capturing. p_knob_4 is not
  // called during normal mode, so its internal filter may be stale. Without
  // this, the filter "catches up" on subsequent Process() calls, drifting
  // past the 5% threshold and unfreezing the capture immediately.
  for (int i = 0; i < 32; i++) {
    p_knob_4.Process();
  }

  // Capture delay time knob so it's ignored until physically moved
  tap_tempo.delay_knob_capture.Capture(p_knob_4.Process());

  // Preserve the current delay time as the quarter-note base so the delay
  // doesn't jump when re-entering tap tempo. Undo the timing multiplier
  // since the audio callback re-applies it from toggle switch 3.
  DelayTimingMode current_timing = kDelayTimingMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_3)];
  tap_tempo.tapped_delay_samples = delay_time_target / kDelayTimingMultiplier[current_timing];

  // Reset tap state (but NOT tap_tempo.tapped_delay_samples — we just set it above)
  tap_tempo.tap_count = 0;
  tap_tempo.tap_timestamps[0] = tap_tempo.tap_timestamps[1] = tap_tempo.tap_timestamps[2] = 0;
  tap_tempo.tap_flash_counter = 0;

  // Initialize LED tempo from the quarter-note base (not the subdivided delay_target)
  tap_tempo.tapped_tempo_ms = tap_tempo.tapped_delay_samples / SAMPLE_RATE * 1000.0f;

  // Start the auto-exit timer
  tap_tempo.last_tap_time = System::GetNow();

  // Enable delay if it's currently off (so taps produce audible delay)
  if (bypass.delay) {
    bypass.delay = false;
    saveBypassStates();
  }

  pedal_mode = PEDAL_MODE_TAP_TEMPO;
}

void exitTapTempo() {
  bool knob_was_moved = !tap_tempo.delay_knob_capture.IsFrozen();
  tap_tempo.delay_knob_capture.Reset();

  if (knob_was_moved || tap_tempo.tapped_delay_samples == 0.0f) {
    // Knob was moved during tap tempo or no delay set — clear tapped delay
    tap_tempo.tapped_delay_samples = 0.0f;
    tap_tempo.knob_baseline = -1.0f;
  } else {
    // Set baseline for normal mode knob movement detection using the
    // hw knob value (always up-to-date, no stale filter issues)
    tap_tempo.knob_baseline = hw.GetKnobValue(Funbox::KNOB_4);
  }

  saveBypassStates(); // Persist the tapped delay state
  pedal_mode = PEDAL_MODE_NORMAL;
}

void registerTap() {
  uint32_t now = System::GetNow();

  // Shift timestamps (newest at index 0)
  tap_tempo.tap_timestamps[2] = tap_tempo.tap_timestamps[1];
  tap_tempo.tap_timestamps[1] = tap_tempo.tap_timestamps[0];
  tap_tempo.tap_timestamps[0] = now;
  if (tap_tempo.tap_count < 3) tap_tempo.tap_count++;

  if (tap_tempo.tap_count >= 2) {
    // Average the intervals between available taps
    uint32_t total = 0;
    int intervals = tap_tempo.tap_count - 1;
    for (int i = 0; i < intervals; i++) {
      total += tap_tempo.tap_timestamps[i] - tap_tempo.tap_timestamps[i + 1];
    }
    float avg_ms = (float)total / intervals;

    // Convert to samples and clamp to valid delay range
    tap_tempo.tapped_delay_samples = avg_ms * SAMPLE_RATE / 1000.0f;
    float min_delay = SAMPLE_RATE * DELAY_TIME_MIN_SECONDS;
    if (tap_tempo.tapped_delay_samples < min_delay) tap_tempo.tapped_delay_samples = min_delay;
    if (tap_tempo.tapped_delay_samples > MAX_DELAY) tap_tempo.tapped_delay_samples = MAX_DELAY;

    tap_tempo.tapped_tempo_ms = avg_ms;
    saveBypassStates(); // Persist the new tapped delay
  }

  tap_tempo.last_tap_time = now;
  tap_tempo.tap_flash_counter = TAP_FLASH_CALLBACKS;  // Trigger brief LED flash
}

// ============================================================================
// FOOTSWITCH HANDLERS
// ============================================================================

void handleNormalPress(Funbox::Switches footswitch) {
  if (pedal_mode == PEDAL_MODE_TAP_TEMPO) {
    if (footswitch == Funbox::FOOTSWITCH_1) {
      exitTapTempo();
      tap_tempo.just_exited_tap_tempo = true;
    } else {
      registerTap();
    }
    return;
  }

  // Clear the tap tempo exit guard
  tap_tempo.just_exited_tap_tempo = false;

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
  } else if (pedal_mode == PEDAL_MODE_EDIT_DEVICE_SETTINGS) {
    // Only save the settings if the RIGHT footswitch is pressed in device
    // settings mode. The LEFT footswitch is used to exit device settings mode
    // without saving.
    if (footswitch == Funbox::FOOTSWITCH_2) {
      // Save the device settings
      saveDeviceSettings();
    } else {
      // Cancel: restore device settings AND bypass states
      restoreDeviceSettings();
    }

    // Reset all switch captures when exiting device settings mode
    p_sw1_capture.Reset();
    p_sw2_capture.Reset();
    p_sw3_capture.Reset();

    pedal_mode = PEDAL_MODE_NORMAL;
  } else {
    if (footswitch == Funbox::FOOTSWITCH_1) {
      bypass.reverb = !bypass.reverb;

      if (bypass.reverb) {
        // Clear the reverb tails when the reverb is bypassed so if you
        // turn it back on, it starts fresh and doesn't sound weird.
        if (current_reverb != nullptr) {
          current_reverb->Clear();
        }
      }
    } else {
      // FOOTSWITCH_2: Toggle tremolo on/off
      bypass.tremolo = !bypass.tremolo;
    }

    saveBypassStates();
  }
}

void handleDoublePress(Funbox::Switches footswitch) {
  // Guard: if we just exited tap tempo, consume this event silently
  if (tap_tempo.just_exited_tap_tempo) {
    tap_tempo.just_exited_tap_tempo = false;
    return;
  }

  // In tap tempo mode, FS2 double press is just another tap
  if (pedal_mode == PEDAL_MODE_TAP_TEMPO) {
    if (footswitch == Funbox::FOOTSWITCH_2) {
      registerTap();
    }
    return;
  }

  // Ignore double presses in edit modes
  if (pedal_mode == PEDAL_MODE_EDIT_REVERB ||
      pedal_mode == PEDAL_MODE_EDIT_DEVICE_SETTINGS) {
    return;
  }

  // When double press is detected, a normal press was already detected and
  // processed, so reverse that right off the bat.
  handleNormalPress(footswitch);

  if (footswitch == Funbox::FOOTSWITCH_1) {
    // FOOTSWITCH_1 double press: Enter tap tempo mode
    enterTapTempo();
  } else if (footswitch == Funbox::FOOTSWITCH_2) {
    // FOOTSWITCH_2 double press: Toggle delay on/off
    bypass.delay = !bypass.delay;

    saveBypassStates();
  }
}

void handleLongPress(Funbox::Switches footswitch) {
  // Guard: if we just exited tap tempo, consume this event silently
  if (tap_tempo.just_exited_tap_tempo) {
    tap_tempo.just_exited_tap_tempo = false;
    return;
  }

  // Ignore long presses in edit modes or tap tempo
  if (pedal_mode == PEDAL_MODE_EDIT_REVERB ||
      pedal_mode == PEDAL_MODE_EDIT_DEVICE_SETTINGS ||
      pedal_mode == PEDAL_MODE_TAP_TEMPO) {
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
    p_knob_2_capture.Capture(reverb.plate_pre_delay);
    p_knob_3_capture.Capture(reverb.plate_decay);
    p_knob_4_capture.Capture(reverb.plate_diffusion);
    p_knob_5_capture.Capture(reverb.plate_input_damp_high);
    p_knob_6_capture.Capture(reverb.plate_tank_damp_high);
    p_sw1_capture.Capture(reverb.plate_mod_speed_pos);
    p_sw2_capture.Capture(reverb.plate_mod_depth_pos);
    p_sw3_capture.Capture(reverb.plate_mod_shape_pos);

    bypass.reverb = false; // Make sure that reverb is ON
    pedal_mode = PEDAL_MODE_EDIT_REVERB;
  } else if (footswitch == Funbox::FOOTSWITCH_2) {
    // FOOTSWITCH_2 long press: Enter device settings.
    p_sw1_capture.Capture(switchPosForValue(kReverbKnobMap, reverb.knob_mode));
    p_sw2_capture.Capture(switchPosForValue(kPolarityMap, polarity_mode));
    p_sw3_capture.Capture(switchPosForValue(kMonoStereoMap, mono_stereo_mode));
    pedal_mode = PEDAL_MODE_EDIT_DEVICE_SETTINGS;
  }
}

// ============================================================================
// AUDIO CALLBACK
// ============================================================================

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  static float trem_val;
  hw.ProcessAllControls();

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
  } else if (pedal_mode == PEDAL_MODE_EDIT_DEVICE_SETTINGS) {
    // Device settings mode
    // Blink the left & right LEDs alternately to indicate device settings mode
    static uint32_t device_settings_edit_count = 0;
    static bool led_state = true;
    if (++device_settings_edit_count >= hw.AudioCallbackRate() / 2) {
      device_settings_edit_count = 0;
      led_state = !led_state;
      led_left.Set(led_state ? 1.0f : 0.0f);
      led_right.Set(led_state ? 0.0f : 1.0f);
    }
  } else {
    // Normal mode (and tap tempo)
    led_left.Set(bypass.reverb ? 0.0f : 1.0f);

    if (pedal_mode == PEDAL_MODE_TAP_TEMPO) {
      // Tap tempo right LED: rhythmic flash at tapped tempo
      static uint32_t tap_led_counter = 0;

      if (tap_tempo.tap_flash_counter > 0) {
        // Brief flash on each tap (overrides rhythmic flash)
        tap_tempo.tap_flash_counter--;
        led_right.Set(1.0f);
        tap_led_counter = 0;  // Sync rhythmic flash to tap
      } else if (tap_tempo.tapped_tempo_ms > 0.0f) {
        // Continuous rhythmic flash at tapped tempo
        uint32_t period = (uint32_t)(tap_tempo.tapped_tempo_ms * hw.AudioCallbackRate() / 1000.0f);
        if (period > 0) {
          tap_led_counter = (tap_led_counter + 1) % period;
          led_right.Set(tap_led_counter < (period / 10) ? 1.0f : 0.0f);  // 10% duty cycle
        }
      } else {
        // No tempo established yet, LED off
        led_right.Set(0.0f);
      }
    } else {
      // Normal mode right LED (existing pulsing trem/delay logic)
      static int count = 0;
      // set led 100 times/sec
      if (++count == hw.AudioCallbackRate() / 100) {
        count = 0;
        // If just delay is on, show full-strength LED
        // If just trem is on, show 40% pulsing LED
        // If both are on, show 100% pulsing LED
        led_right.Set(bypass.tremolo ? bypass.delay ? 0.0f : 1.0 : bypass.delay ? trem_val * TREMOLO_LED_BRIGHTNESS : trem_val);
      }
    }
  }
  led_left.Update();
  led_right.Update();

  reverb.wet = p_verb_amt.Process();

  TremDelMakeUpGain makeup_gain = (!bypass.delay || !bypass.tremolo) ? MAKEUP_GAIN_NORMAL : MAKEUP_GAIN_NONE;

  if (pedal_mode == PEDAL_MODE_NORMAL || pedal_mode == PEDAL_MODE_TAP_TEMPO) {
    // Common processing for normal and tap tempo modes

    // Reverb type from SW1
    reverb.current_type = kReverbTypeMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_1)];

    // Tremolo: select algorithm based on switch position
    TremoloMode tremMode = kTremoloModeMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_2)];
    switch (tremMode) {
      case TREMOLO_SINE:
        current_tremolo = &sine_tremolo;
        break;
      case TREMOLO_SQUARE:
        current_tremolo = &square_tremolo;
        break;
      case TREMOLO_HARMONIC:
        current_tremolo = &harmonic_tremolo;
        break;
    }

    // Update tremolo parameters (depth scaling handled internally by each class)
    current_tremolo->SetSpeed(p_trem_speed.Process());
    current_tremolo->SetDepth(daisysp::fclamp(p_trem_depth.Process(), 0.f, 1.f));

    //
    // Delay
    //
    // Determine the base delay time (quarter-note value, before timing subdivision).
    // The timing multiplier (triplet, quarter, dotted eighth) is applied once at the
    // end, regardless of whether the base comes from tap tempo, persisted tapped
    // delay, or the delay knob.
    //
    DelayTimingMode delay_timing = kDelayTimingMap[hw.GetToggleswitchPosition(Funbox::TOGGLESWITCH_3)];
    float base_delay_time;

    if (pedal_mode == PEDAL_MODE_TAP_TEMPO) {
      // Process knob capture to detect movement
      tap_tempo.delay_knob_capture.Process();

      if (!tap_tempo.delay_knob_capture.IsFrozen()) {
        // Knob was physically moved — use knob value
        base_delay_time = p_delay_time.Process();
      } else if (tap_tempo.tapped_delay_samples > 0.0f) {
        // Use tapped tempo
        base_delay_time = tap_tempo.tapped_delay_samples;
      } else {
        // No taps yet — use current knob value
        base_delay_time = p_delay_time.Process();
      }

      // Auto-exit after 4 seconds of no taps
      if (System::GetNow() - tap_tempo.last_tap_time > TAP_TEMPO_TIMEOUT_MS) {
        exitTapTempo();
      }
    } else {
      // Normal mode delay processing
      // If tapped tempo is active, use it until the delay knob is physically moved.
      // Uses hw.GetKnobValue() for movement detection — this is always up-to-date
      // (updated by hw.ProcessAllControls() every callback) unlike p_knob_4 which
      // has a stale smoothing filter when not actively called.
      if (tap_tempo.tapped_delay_samples > 0.0f) {
        float current_knob = hw.GetKnobValue(Funbox::KNOB_4);
        if (tap_tempo.knob_baseline < 0.0f) {
          // First callback with tapped delay (e.g. after boot) — record baseline
          tap_tempo.knob_baseline = current_knob;
        }
        if (fabs(current_knob - tap_tempo.knob_baseline) > 0.05f) {
          // Knob moved >5% from baseline — switch to knob control
          tap_tempo.tapped_delay_samples = 0.0f;
          tap_tempo.knob_baseline = -1.0f;
          saveBypassStates(); // Persist the cleared tapped delay
        }
      }

      if (tap_tempo.tapped_delay_samples > 0.0f) {
        base_delay_time = tap_tempo.tapped_delay_samples;
      } else {
        base_delay_time = p_delay_time.Process();
      }
    }

    // Apply timing subdivision (triplet, quarter, dotted eighth) in one place
    delay_time_target = base_delay_time * kDelayTimingMultiplier[delay_timing];
    delay_effect.SetDelayTime(delay_time_target);
    delay_effect.SetFeedback(p_delay_feedback.Process());
    delay_drywet = (int)p_delay_amt.Process();

    // Reverb dry/wet mode (from saved setting)
    switch (reverb.knob_mode) {
      case REVERB_KNOB_ALL_DRY:
        reverb.dry = 1.0;
        break;
      case REVERB_KNOB_DRY_WET_MIX:
        reverb.dry = 1.0 - reverb.wet;
        break;
      case REVERB_KNOB_ALL_WET:
        reverb.dry = 0.0f;
        break;
    }
  } else if (pedal_mode == PEDAL_MODE_EDIT_REVERB) {
    // Edit mode with parameter capture
    reverb.dry = 1.0; // Always use dry 100% in edit mode

    // Use capture objects - pass calculated values, they return frozen or current based on movement
    reverb.plate_pre_delay = p_knob_2_capture.Process();
    reverb.plate_decay = p_knob_3_capture.Process();
    reverb.plate_diffusion = p_knob_4_capture.Process();
    reverb.plate_input_damp_high = p_knob_5_capture.Process();
    reverb.plate_tank_damp_high = p_knob_6_capture.Process();
    reverb.plate_mod_speed_pos = p_sw1_capture.Process();
    reverb.plate_mod_depth_pos = p_sw2_capture.Process();
    reverb.plate_mod_shape_pos = p_sw3_capture.Process();

    updatePlateReverbParameters();

  } else if (pedal_mode == PEDAL_MODE_EDIT_DEVICE_SETTINGS) {
    // Device settings mode with switch capture (soft takeover)

    // SW1: Reverb wet/dry mode
    reverb.knob_mode = kReverbKnobMap[p_sw1_capture.Process()];

    // Apply reverb dry/wet so changes are audible in settings mode
    switch (reverb.knob_mode) {
      case REVERB_KNOB_ALL_DRY:
        reverb.dry = 1.0;
        break;
      case REVERB_KNOB_DRY_WET_MIX:
        reverb.dry = 1.0 - reverb.wet;
        break;
      case REVERB_KNOB_ALL_WET:
        reverb.dry = 0.0f;
        break;
    }

    // SW2: Polarity mode
    polarity_mode = kPolarityMap[p_sw2_capture.Process()];

    // SW3: Mono/stereo mode
    mono_stereo_mode = kMonoStereoMap[p_sw3_capture.Process()];
    updateReverbScales(mono_stereo_mode);
  }

  float polarity_L = (polarity_mode == POLARITY_INVERT_LEFT) ? -1.0f : 1.0f;
  float polarity_R = (polarity_mode == POLARITY_INVERT_RIGHT) ? -1.0f : 1.0f;

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

    if (!bypass.delay) {
      float fdrywet = delay_drywet / 100.0f;
      float delay_make_up_gain = makeup_gain == MAKEUP_GAIN_NONE ? 1.0f : 1.66f;

      // Process delay effect (returns wet signal only)
      float wet_L, wet_R;
      delay_effect.ProcessSample(s_L, s_R, &wet_L, &wet_R);

      // Mix dry and wet signals with makeup gain
      // Wet signal is attenuated by 0.333 to prevent clipping when feedback is high
      s_L = fdrywet * wet_L * 0.333f + (1.0f - fdrywet) * s_L * delay_make_up_gain;
      s_R = fdrywet * wet_R * 0.333f + (1.0f - fdrywet) * s_R * delay_make_up_gain;
    }

    if (!bypass.tremolo) {
      float trem_make_up_gain = makeup_gain == MAKEUP_GAIN_NONE ? 1.0f : 1.2f;

      // Process tremolo effect
      float trem_out_L, trem_out_R;
      current_tremolo->ProcessSample(s_L, s_R, &trem_out_L, &trem_out_R);

      // Apply makeup gain
      s_L = trem_out_L * trem_make_up_gain;
      s_R = trem_out_R * trem_make_up_gain;

      // Store LFO value for LED pulsing
      trem_val = current_tremolo->GetLastLFOValue();
    }

    // Keep sending input to the reverb even if bypassed so that when it's
    // enabled again it will already have the current input signal already
    // being processed.

    left_input = hardLimit100_(s_L) * reverb_dry_scale_factor;
    right_input = hardLimit100_(s_R) * reverb_dry_scale_factor;

    float gain = MINUS_18DB_GAIN * MINUS_20DB_GAIN * (1.0f + input_amplification * 7.0f) * clearPopCancelValue;
    float rev_l, rev_r;

    // Switch active reverb algorithm based on toggle switch
    switch (reverb.current_type) {
      case REVERB_PLATE:
        current_reverb = &plate_reverb;
        updatePlateReverbParameters(); // Update plate-specific parameters
        break;
      case REVERB_SPRING:
        current_reverb = &spring_reverb;
        break;
      case REVERB_HALL:
        current_reverb = &hall_reverb;
        break;
    }

    // Process reverb via polymorphic interface
    current_reverb->ProcessSample(left_input * gain, right_input * gain, &rev_l, &rev_r);

    // Apply algorithm-specific gain adjustments
    if (reverb.current_type == REVERB_HALL) {
      // Make hall reverb louder to match the mix knob expectations
      rev_l *= 4.0f;
      rev_r *= 4.0f;
    }

    if (!bypass.reverb) {
      // left_output = ((left_input * reverb.dry * 0.1) + (verb.getLeftOutput() * reverb.wet * clearPopCancelValue));
      // right_output = ((right_input * reverb.dry * 0.1) + (verb.getRightOutput() * reverb.wet * clearPopCancelValue));
      left_output = ((left_input * reverb.dry * reverb_reverse_scale_factor) + (rev_l * reverb.wet * clearPopCancelValue));
      right_output = ((right_input * reverb.dry * reverb_reverse_scale_factor) + (rev_r * reverb.wet * clearPopCancelValue));

      s_L = left_output;
      s_R = right_output;
    }

    if (mono_stereo_mode == MS_MODE_MIMO) {
      out[0][i] = ((s_L * 0.5) + (s_R * 0.5)) * polarity_L;
      out[1][i] = 0.0f;
    } else {
      out[0][i] = s_L * polarity_L;
      out[1][i] = s_R * polarity_R;
    }
  }
}

// ============================================================================
// FACTORY RESET
// ============================================================================

/// @brief Handles the factory reset interaction loop.
///
/// Called from main() when is_factory_reset_mode is true. The user must rotate
/// Knob 1 through a sequence (100% → 0% → 100% → 0%) to confirm the reset.
/// Each completed stage speeds up the LED blinking as visual feedback.
/// Completing the sequence restores defaults and starts normal pedal operation.
void runFactoryResetLoop() {
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
    bypass.delay = true;
    bypass.tremolo = true;
    pedal_mode = PEDAL_MODE_NORMAL;
    is_factory_reset_mode = false;
  }
}

// ============================================================================
// MAIN
// ============================================================================

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

  // Initialize delay effect
  delay_effect.Init(hw.AudioSampleRate(), &delMemL, &delMemR);

  // Initialize tremolo effects
  sine_tremolo.Init(hw.AudioSampleRate());
  square_tremolo.Init(hw.AudioSampleRate());
  harmonic_tremolo.Init(hw.AudioSampleRate());
  current_tremolo = &sine_tremolo;  // Default

  // Initialize notch filters to remove resonant frequencies (always active)
  notch1_L.Init(NOTCH_1_FREQ, -30.0f, 40.0f, hw.AudioSampleRate());
  notch1_R.Init(NOTCH_1_FREQ, -30.0f, 40.0f, hw.AudioSampleRate());
  notch2_L.Init(NOTCH_2_FREQ, -30.0f, 40.0f, hw.AudioSampleRate());
  notch2_R.Init(NOTCH_2_FREQ, -30.0f, 40.0f, hw.AudioSampleRate());

  //
  // Reverb Initialization (all three types)
  //
  // Zero out the InterpDelay buffers used by the plate reverb (Dattorro SDRAM)
  for(int i = 0; i < 50; i++) {
      for(int j = 0; j < 144000; j++) {
          sdramData[i][j] = 0.;
      }
  }
  // Set this to 1.0 or plate reverb won't work. This is defined in Dattorro's
  // InterpDelay.cpp file.
  hold = 1.;

  // Initialize Plate Reverb (Dattorro)
  plate_reverb.Init(hw.AudioSampleRate());
  updatePlateReverbParameters();

  // Initialize Hall Reverb (Schroeder)
  hall_reverb.Init(hw.AudioSampleRate());
  hall_reverb.SetDecay(0.95f); // Higher feedback for longer hall decay

  // Initialize Spring Reverb (Digital Waveguide)
  spring_reverb.Init(hw.AudioSampleRate());
  spring_reverb.SetDecay(0.7f); // Spring decay
  spring_reverb.SetMix(1.0f);   // 100% wet - it'll be mixed with Knob 1
  spring_reverb.SetDamping(7000.0f); // High-frequency damping

  // Set default active reverb
  current_reverb = &plate_reverb;

  Settings defaultSettings = {
    SETTINGS_VERSION,               // version
    reverb.plate_decay,             // decay
    reverb.plate_diffusion,         // diffusion
    reverb.plate_input_damp_high,   // input_cutoff_freq
    reverb.plate_tank_damp_high,    // tank_cutoff_freq
    reverb.plate_mod_speed_pos,     // tank_mod_speed_pos
    reverb.plate_mod_depth_pos,     // tank_mod_depth_pos
    reverb.plate_mod_shape_pos,     // tank_mod_shape_pos
    reverb.plate_pre_delay,         // pre_delay
    MS_MODE_MIMO,                   // mono_stereo_mode
    POLARITY_NORMAL,                // polarity_mode
    REVERB_KNOB_DRY_WET_MIX,       // reverb_knob_mode
    true,                           // bypass_reverb
    true,                           // bypass_tremolo
    true,                           // bypass_delay
    0.0f,                           // tapped_delay_samples
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
  hw.DelayMs(5); // Wait for ADC DMA to provide valid data
  // Warm up the knob smoothing filters so they converge to the actual knob
  // positions before the audio callback starts. Without this, the filters
  // start at 0 and ramp up over ~30 callbacks, which can cause the tap tempo
  // knob baseline to drift past the 5% threshold and clear the tapped delay.
  for (int i = 0; i < 100; i++) {
    hw.ProcessAnalogControls();
  }
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
      runFactoryResetLoop();
    }
    hw.DelayMs(10);
  }
  return 0;
}
