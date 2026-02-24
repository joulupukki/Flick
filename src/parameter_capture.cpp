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

#include "parameter_capture.h"
#include <cmath>

namespace flick {

// KnobCapture implementation

KnobCapture::KnobCapture(daisy::Parameter& knob, float threshold)
    : knob_(knob),
      captured_knob_value_(0.0f),
      frozen_value_(0.0f),
      is_frozen_(false),
      threshold_(threshold) {}

void KnobCapture::Capture(float frozen_value) {
  captured_knob_value_ = knob_.Process();
  frozen_value_ = frozen_value;
  is_frozen_ = true;
}

float KnobCapture::Process() {
  float current_knob_value = knob_.Process();

  if (!is_frozen_) {
    // Pass-through mode (normal operation or already activated)
    return current_knob_value;
  }

  // Capture mode - check for movement
  if (std::fabs(current_knob_value - captured_knob_value_) >= threshold_) {
    // Threshold exceeded, activate and return current value
    is_frozen_ = false;
    return current_knob_value;
  }

  // Still frozen, return the frozen parameter value
  return frozen_value_;
}

void KnobCapture::Reset() {
  is_frozen_ = false;
}

// SwitchCapture implementation

SwitchCapture::SwitchCapture(Funbox& hw, Funbox::Toggleswitch switch_idx)
    : hw_(hw),
      switch_idx_(switch_idx),
      captured_switch_value_(0),
      is_frozen_(false),
      frozen_value_(0) {}

void SwitchCapture::Capture(int frozen_value) {
  captured_switch_value_ = hw_.GetToggleswitchPosition(switch_idx_);
  frozen_value_ = frozen_value;
  is_frozen_ = true;
}

int SwitchCapture::Process() {
  int current_value = hw_.GetToggleswitchPosition(switch_idx_);

  if (!is_frozen_) {
    return current_value;
  }

  // Capture mode - check for movement
  if (current_value != captured_switch_value_) {
    // Switch moved, activate and return new value
    is_frozen_ = false;
    return current_value;
  }

  // Still frozen
  return frozen_value_;
}

void SwitchCapture::Reset() {
  is_frozen_ = false;
}

} // namespace flick
