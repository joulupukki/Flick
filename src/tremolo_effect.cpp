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

#include "tremolo_effect.h"

namespace flick {

// Filter frequency constants (from flick.cpp)
constexpr float HARMONIC_TREMOLO_LPF_CUTOFF = 144.0f;
constexpr float HARMONIC_TREMOLO_HPF_CUTOFF = 636.0f;
constexpr float HARMONIC_TREM_EQ_HPF1_CUTOFF = 63.0f;
constexpr float HARMONIC_TREM_EQ_LPF1_CUTOFF = 11200.0f;
constexpr float HARMONIC_TREM_EQ_PEAK1_FREQ = 7500.0f;
constexpr float HARMONIC_TREM_EQ_PEAK1_GAIN = -3.37f;
constexpr float HARMONIC_TREM_EQ_PEAK1_Q = 0.263f;
constexpr float HARMONIC_TREM_EQ_PEAK2_FREQ = 254.0f;
constexpr float HARMONIC_TREM_EQ_PEAK2_GAIN = 2.0f;
constexpr float HARMONIC_TREM_EQ_PEAK2_Q = 0.707f;
constexpr float HARMONIC_TREM_EQ_LOW_SHELF_FREQ = 37.0f;
constexpr float HARMONIC_TREM_EQ_LOW_SHELF_GAIN = -10.5f;
constexpr float HARMONIC_TREM_EQ_LOW_SHELF_Q = 1.0f;

// ============================================================================
// SineTremolo Implementation
// ============================================================================

void SineTremolo::Init(float sample_rate) {
  sample_rate_ = sample_rate;
  osc_.Init(sample_rate);
  osc_.SetWaveform(FlickOscillator::WAVE_SIN);
  dc_offset_ = 1.0f;
}

void SineTremolo::ProcessSample(float in_L, float in_R, float* out_L, float* out_R) {
  // Update oscillator parameters
  osc_.SetFreq(speed_);

  // Apply 0.5x depth scaling for sine tremolo
  float scaled_depth = depth_ * 0.5f;
  osc_.SetAmp(scaled_depth);
  dc_offset_ = 1.0f - scaled_depth;

  // Generate LFO sample
  float lfo_sample = osc_.Process();
  last_lfo_value_ = dc_offset_ + lfo_sample;

  // Apply amplitude modulation
  *out_L = in_L * last_lfo_value_;
  *out_R = in_R * last_lfo_value_;
}

// ============================================================================
// SquareTremolo Implementation
// ============================================================================

void SquareTremolo::Init(float sample_rate) {
  sample_rate_ = sample_rate;
  osc_.Init(sample_rate);
  osc_.SetWaveform(FlickOscillator::WAVE_SQUARE_ROUNDED);
  dc_offset_ = 1.0f;
}

void SquareTremolo::ProcessSample(float in_L, float in_R, float* out_L, float* out_R) {
  // Update oscillator parameters
  osc_.SetFreq(speed_);

  // Apply 0.5x depth scaling for square tremolo
  float scaled_depth = depth_ * 0.5f;
  osc_.SetAmp(scaled_depth);
  dc_offset_ = 1.0f - scaled_depth;

  // Generate LFO sample
  float lfo_sample = osc_.Process();
  last_lfo_value_ = dc_offset_ + lfo_sample;

  // Apply amplitude modulation
  *out_L = in_L * last_lfo_value_;
  *out_R = in_R * last_lfo_value_;
}

// ============================================================================
// HarmonicTremolo Implementation
// ============================================================================

void HarmonicTremolo::Init(float sample_rate) {
  sample_rate_ = sample_rate;
  osc_.Init(sample_rate);
  osc_.SetWaveform(FlickOscillator::WAVE_SIN);

  // Initialize band-splitting filters
  lpf_L_.Init(HARMONIC_TREMOLO_LPF_CUTOFF, sample_rate);
  lpf_R_.Init(HARMONIC_TREMOLO_LPF_CUTOFF, sample_rate);
  hpf_L_.Init(HARMONIC_TREMOLO_HPF_CUTOFF, sample_rate);
  hpf_R_.Init(HARMONIC_TREMOLO_HPF_CUTOFF, sample_rate);

  // Initialize EQ shaping filters
  eq_hpf1_L_.Init(HARMONIC_TREM_EQ_HPF1_CUTOFF, sample_rate);
  eq_hpf1_R_.Init(HARMONIC_TREM_EQ_HPF1_CUTOFF, sample_rate);
  eq_lpf1_L_.Init(HARMONIC_TREM_EQ_LPF1_CUTOFF, sample_rate);
  eq_lpf1_R_.Init(HARMONIC_TREM_EQ_LPF1_CUTOFF, sample_rate);
  eq_peak1_L_.Init(HARMONIC_TREM_EQ_PEAK1_FREQ, HARMONIC_TREM_EQ_PEAK1_GAIN,
                   HARMONIC_TREM_EQ_PEAK1_Q, sample_rate);
  eq_peak1_R_.Init(HARMONIC_TREM_EQ_PEAK1_FREQ, HARMONIC_TREM_EQ_PEAK1_GAIN,
                   HARMONIC_TREM_EQ_PEAK1_Q, sample_rate);
  eq_peak2_L_.Init(HARMONIC_TREM_EQ_PEAK2_FREQ, HARMONIC_TREM_EQ_PEAK2_GAIN,
                   HARMONIC_TREM_EQ_PEAK2_Q, sample_rate);
  eq_peak2_R_.Init(HARMONIC_TREM_EQ_PEAK2_FREQ, HARMONIC_TREM_EQ_PEAK2_GAIN,
                   HARMONIC_TREM_EQ_PEAK2_Q, sample_rate);
  eq_low_shelf_L_.Init(HARMONIC_TREM_EQ_LOW_SHELF_FREQ, HARMONIC_TREM_EQ_LOW_SHELF_GAIN,
                       HARMONIC_TREM_EQ_LOW_SHELF_Q, sample_rate);
  eq_low_shelf_R_.Init(HARMONIC_TREM_EQ_LOW_SHELF_FREQ, HARMONIC_TREM_EQ_LOW_SHELF_GAIN,
                       HARMONIC_TREM_EQ_LOW_SHELF_Q, sample_rate);
}

void HarmonicTremolo::SetDepth(float depth) {
  // Apply 1.25x depth scaling for harmonic tremolo
  depth_ = depth * 1.25f;
}

void HarmonicTremolo::ProcessSample(float in_L, float in_R, float* out_L, float* out_R) {
  // Update oscillator parameters (depth already scaled by SetDepth)
  osc_.SetFreq(speed_);
  osc_.SetAmp(depth_);

  // Generate LFO sample
  float lfo_sample = osc_.Process();
  last_lfo_value_ = lfo_sample;

  // Process left channel
  float low_L = lpf_L_.Process(in_L);
  float high_L = hpf_L_.Process(in_L);

  // Apply tremolo with opposite phase to each band
  float low_mod_L = low_L * (1.0f + lfo_sample);
  float high_mod_L = high_L * (1.0f - lfo_sample);
  *out_L = low_mod_L + high_mod_L;

  // Process right channel
  float low_R = lpf_R_.Process(in_R);
  float high_R = hpf_R_.Process(in_R);

  float low_mod_R = low_R * (1.0f + lfo_sample);
  float high_mod_R = high_R * (1.0f - lfo_sample);
  *out_R = low_mod_R + high_mod_R;

  // Apply EQ shaping to both channels
  *out_L = eq_hpf1_L_.Process(*out_L);
  *out_R = eq_hpf1_R_.Process(*out_R);

  *out_L = eq_lpf1_L_.Process(*out_L);
  *out_R = eq_lpf1_R_.Process(*out_R);

  *out_L = eq_low_shelf_L_.Process(*out_L);
  *out_R = eq_low_shelf_R_.Process(*out_R);

  *out_L = eq_peak2_L_.Process(*out_L);
  *out_R = eq_peak2_R_.Process(*out_R);

  *out_L = eq_peak1_L_.Process(*out_L);
  *out_R = eq_peak1_R_.Process(*out_R);
}

}  // namespace flick
