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

#include "plate_reverb.h"

namespace flick {

// Constants for parameter scaling
constexpr float PLATE_PRE_DELAY_SCALE = 0.25f;
constexpr float PLATE_DAMP_SCALE = 10.0f;
constexpr float PLATE_TANK_MOD_SPEED_SCALE = 8.0f;
constexpr float PLATE_TANK_MOD_DEPTH_SCALE = 15.0f;

// Hardcoded parameters (previously editable, now fixed to good defaults)
constexpr float PLATE_INPUT_HIGH_CUT = 7.25f;   // Input high-cut filter pitch
constexpr float PLATE_MOD_SHAPE = 0.25f;         // Tank modulation shape

PlateReverb::PlateReverb()
    : verb_(48000, 16, 4.0f) {
}

void PlateReverb::Init(float sample_rate) {
  sample_rate_ = sample_rate;
  verb_.setSampleRate(sample_rate);
  verb_.setTimeScale(1.007500f);
  verb_.enableInputDiffusion(true);

  // Set low-cut filters (pitch-based: 440 * 2^(pitch - 5))
  // pitch = 2.87 -> 440 * 2^(-2.13) ~ 100Hz
  verb_.setInputFilterLowCutoffPitch(2.87f);
  verb_.setTankFilterLowCutFrequency(2.87f);

  // Hardcoded parameters
  verb_.setInputFilterHighCutoffPitch(PLATE_INPUT_HIGH_CUT);
  verb_.setTankModShape(PLATE_MOD_SHAPE);

  // Apply editable parameters
  updateDattorroParameters();
}

void PlateReverb::ProcessSample(float in_L, float in_R, float* out_L, float* out_R) {
  // Dattorro's process() method processes the input and stores output internally
  verb_.process(in_L, in_R);

  // Retrieve output (wet signal only)
  *out_L = verb_.getLeftOutput();
  *out_R = verb_.getRightOutput();
}

void PlateReverb::Clear() {
  verb_.clear();
}

void PlateReverb::SetDecay(float decay) {
  verb_.setDecay(decay);
}

void PlateReverb::SetDiffusion(float diffusion) {
  verb_.setTankDiffusion(diffusion);
}

void PlateReverb::SetPreDelay(float pre_delay) {
  verb_.setPreDelay(pre_delay * PLATE_PRE_DELAY_SCALE);
}

void PlateReverb::SetTone(float tone) {
  verb_.setTankFilterHighCutFrequency(tone * PLATE_DAMP_SCALE);
}

void PlateReverb::SetModulation(float mod) {
  // Single knob (0-1) controls both mod speed and depth together.
  // Range 0.1-0.5 matches the original 3-position switch values.
  float speed = 0.1f + mod * 0.4f;
  float depth = 0.1f + mod * 0.4f;
  verb_.setTankModSpeed(speed * PLATE_TANK_MOD_SPEED_SCALE);
  verb_.setTankModDepth(depth * PLATE_TANK_MOD_DEPTH_SCALE);
}

void PlateReverb::updateDattorroParameters() {
  // Default values for editable parameters (applied at init, overridden by loadSettings)
  verb_.setDecay(0.8f);
  verb_.setTankDiffusion(0.85f);
  verb_.setPreDelay(0.0f);
  verb_.setTankFilterHighCutFrequency(0.725f * PLATE_DAMP_SCALE);
  // Default modulation: low (mod = 0.0 -> speed and depth both 0.1)
  verb_.setTankModSpeed(0.1f * PLATE_TANK_MOD_SPEED_SCALE);
  verb_.setTankModDepth(0.1f * PLATE_TANK_MOD_DEPTH_SCALE);
}

}  // namespace flick
