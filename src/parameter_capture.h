// Parameter Capture - Soft Takeover for Edit Modes
// Copyright (C) 2026 Boyd Timothy <btimothy@gmail.com>
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

#include "daisy.h"
#include "daisy_hardware.h"

namespace flick {

/**
 * @brief Implements soft takeover for knob-based parameters in edit modes.
 *
 * When entering an edit mode, the current parameter value is frozen and the knob
 * position is recorded. The parameter remains frozen until the knob moves beyond
 * a threshold (default 5% of full range), preventing sudden jumps when knobs
 * control different parameters in different modes.
 *
 * Usage:
 * - Normal mode: Use p_knob.Process() directly
 * - Edit mode: Substitute with knob_capture.Process()
 * - On entering edit mode: Call Capture(current_parameter_value)
 * - On exiting edit mode: Call Reset()
 */
class KnobCapture {
public:
  /**
   * @brief Constructs a KnobCapture bound to a specific knob.
   *
   * @param knob Reference to the Parameter object representing the knob
   * @param threshold Movement threshold (0.0-1.0) required to activate, default 0.05
   */
  explicit KnobCapture(daisy::Parameter& knob, float threshold = 0.05f);

  /**
   * @brief Freezes the current parameter value and records knob position.
   *
   * Call this when entering an edit mode. The parameter value is frozen and
   * the current knob position is recorded as the baseline. Subsequent calls
   * to Process() will return the frozen value until the knob moves beyond
   * the threshold.
   */
  void Capture(float frozen_value);

  /**
   * @brief Returns the appropriate knob value based on capture state.
   *
   * In normal mode (not captured), returns the current knob position.
   * In capture mode, returns the frozen value until the knob moves beyond
   * the threshold, then activates and returns the current position.
   *
   * This is designed to be a drop-in replacement for Parameter::Process()
   * in the audio callback, with the caller applying any necessary scaling.
   *
   * @return Raw knob value or frozen value
   */
  float Process();

  /**
   * @brief Resets to pass-through mode.
   *
   * Call this when exiting an edit mode to restore normal operation.
   */
  void Reset();

private:
  daisy::Parameter& knob_;    ///< Reference to the knob's Parameter object
  float captured_knob_value_; ///< Value of the knob at the time of capture
  float frozen_value_;        ///< Cached value - returned until knob is turned beyond threshold
  bool is_frozen_;            ///< true = frozen, false = pass-through
  float threshold_;           ///< Movement threshold for activation
};

/**
 * @brief Implements soft takeover for switch-based parameters in edit modes.
 *
 * Similar to KnobCapture but for discrete toggle switches. When captured, the
 * parameter value remains frozen until the switch moves to a different position.
 *
 * Usage:
 * - Declare with switch index and value lookup array
 * - Call Capture(current_param_value) when entering edit mode
 * - Call Process() to get parameter value (handles lookup internally)
 * - Call Reset() when exiting edit mode
 */
class SwitchCapture {
public:
  /**
   * @brief Constructs a SwitchCapture bound to a specific toggle switch.
   *
   * @param hw Reference to the Funbox hardware object
   * @param switch_idx The toggle switch identifier (TOGGLESWITCH_1/2/3)
   */
  SwitchCapture(Funbox& hw, Funbox::Toggleswitch switch_idx);

  /**
   * @brief Freezes the current parameter value and records switch position.
   */
  void Capture(int frozen_value);

  /**
   * @brief Returns the appropriate parameter value based on capture state.
   *
   * In normal mode, looks up the value from the switch position.
   * In capture mode, returns the frozen value until the switch moves to
   * a different position.
   *
   * @return Parameter value (either frozen or looked up from current position)
   */
  int Process();

  /**
   * @brief Resets to pass-through mode.
   */
  void Reset();

private:
  Funbox& hw_;                      ///< Reference to hardware object
  Funbox::Toggleswitch switch_idx_; ///< Which toggle switch
  int captured_switch_value_;       ///< Position at the time of capture
  bool is_frozen_;                  ///< true = frozen, false = pass-through
  int frozen_value_;                ///< Cached value
};

} // namespace flick
