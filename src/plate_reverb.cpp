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

// Constants for parameter scaling (from flick.cpp)
constexpr float PLATE_PRE_DELAY_SCALE = 0.25f;
constexpr float PLATE_DAMP_SCALE = 10.0f;
constexpr float PLATE_TANK_MOD_SPEED_SCALE = 8.0f;
constexpr float PLATE_TANK_MOD_DEPTH_SCALE = 15.0f;

// Switch position value mappings (from flick.cpp)
constexpr float PLATE_TANK_MOD_SPEED_VALUES[] = {0.5f, 0.25f, 0.1f};
constexpr float PLATE_TANK_MOD_DEPTH_VALUES[] = {0.5f, 0.25f, 0.1f};
constexpr float PLATE_TANK_MOD_SHAPE_VALUES[] = {0.5f, 0.25f, 0.1f};

PlateReverb::PlateReverb()
    : verb_(48000, 16, 4.0) {
  // Initialize default parameters (from flick.cpp)
  params_.decay = 0.8f;
  params_.diffusion = 0.85f;
  params_.pre_delay = 0.0f;
  params_.input_cutoff_freq = 7.25f;  // Will be scaled by PLATE_DAMP_SCALE
  params_.tank_cutoff_freq = 7.25f;   // Will be scaled by PLATE_DAMP_SCALE
  params_.tank_mod_speed_pos = 2;     // Position 2 = 0.1
  params_.tank_mod_depth_pos = 2;     // Position 2 = 0.1
  params_.tank_mod_shape_pos = 1;     // Position 1 = 0.25
}

void PlateReverb::Init(float sample_rate) {
  sample_rate_ = sample_rate;
  verb_.setSampleRate(sample_rate);
  verb_.setTimeScale(1.007500);
  verb_.enableInputDiffusion(true);

  // Set low-cut filters (pitch-based: 440 * 2^(pitch - 5))
  // pitch = 2.87 -> 440 * 2^(-2.13) ≈ 100Hz
  verb_.setInputFilterLowCutoffPitch(2.87f);
  verb_.setTankFilterLowCutFrequency(2.87f);

  // Apply all current parameters
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
  params_.decay = decay;
  verb_.setDecay(decay);
}

void PlateReverb::SetDiffusion(float diffusion) {
  params_.diffusion = diffusion;
  verb_.setTankDiffusion(diffusion);
}

void PlateReverb::SetPreDelay(float pre_delay) {
  params_.pre_delay = pre_delay;
  // Pre-delay is scaled when applied to Dattorro
  verb_.setPreDelay(pre_delay * PLATE_PRE_DELAY_SCALE);
}

void PlateReverb::SetInputHighCut(float freq) {
  params_.input_cutoff_freq = freq;
  // Input high-cut uses pitch (scaled frequency)
  verb_.setInputFilterHighCutoffPitch(freq * PLATE_DAMP_SCALE);
}

void PlateReverb::SetTankHighCut(float freq) {
  params_.tank_cutoff_freq = freq;
  // Tank high-cut uses frequency directly (scaled)
  verb_.setTankFilterHighCutFrequency(freq * PLATE_DAMP_SCALE);
}

void PlateReverb::SetTankModSpeed(float speed) {
  // Speed is passed as a switch position value (0.5, 0.25, or 0.1)
  // Find which position this corresponds to
  int pos = 0;
  if (speed <= 0.15f) pos = 2;      // 0.1
  else if (speed <= 0.375f) pos = 1; // 0.25
  else pos = 0;                      // 0.5

  params_.tank_mod_speed_pos = pos;
  verb_.setTankModSpeed(PLATE_TANK_MOD_SPEED_VALUES[pos] * PLATE_TANK_MOD_SPEED_SCALE);
}

void PlateReverb::SetTankModDepth(float depth) {
  // Depth is passed as a switch position value (0.5, 0.25, or 0.1)
  int pos = 0;
  if (depth <= 0.15f) pos = 2;      // 0.1
  else if (depth <= 0.375f) pos = 1; // 0.25
  else pos = 0;                      // 0.5

  params_.tank_mod_depth_pos = pos;
  verb_.setTankModDepth(PLATE_TANK_MOD_DEPTH_VALUES[pos] * PLATE_TANK_MOD_DEPTH_SCALE);
}

void PlateReverb::SetTankModShape(float shape) {
  // Shape is passed as a switch position value (0.5, 0.25, or 0.1)
  int pos = 0;
  if (shape <= 0.15f) pos = 2;      // 0.1
  else if (shape <= 0.375f) pos = 1; // 0.25
  else pos = 0;                      // 0.5

  params_.tank_mod_shape_pos = pos;
  verb_.setTankModShape(PLATE_TANK_MOD_SHAPE_VALUES[pos]);
}

PlateReverb::Params PlateReverb::GetParams() const {
  return params_;
}

void PlateReverb::SetParams(const Params& params) {
  params_ = params;
  updateDattorroParameters();
}

void PlateReverb::updateDattorroParameters() {
  // Apply all stored parameters to Dattorro
  verb_.setDecay(params_.decay);
  verb_.setTankDiffusion(params_.diffusion);
  verb_.setPreDelay(params_.pre_delay * PLATE_PRE_DELAY_SCALE);
  verb_.setInputFilterHighCutoffPitch(params_.input_cutoff_freq * PLATE_DAMP_SCALE);
  verb_.setTankFilterHighCutFrequency(params_.tank_cutoff_freq * PLATE_DAMP_SCALE);
  verb_.setTankModSpeed(PLATE_TANK_MOD_SPEED_VALUES[params_.tank_mod_speed_pos] * PLATE_TANK_MOD_SPEED_SCALE);
  verb_.setTankModDepth(PLATE_TANK_MOD_DEPTH_VALUES[params_.tank_mod_depth_pos] * PLATE_TANK_MOD_DEPTH_SCALE);
  verb_.setTankModShape(PLATE_TANK_MOD_SHAPE_VALUES[params_.tank_mod_shape_pos]);
}

}  // namespace flick
