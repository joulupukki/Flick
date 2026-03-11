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

#ifndef TREMOLO_EFFECT_H
#define TREMOLO_EFFECT_H

#include "flick_oscillator.h"
#include "flick_filters.hpp"

namespace flick {

/**
 * Base class for tremolo effects.
 *
 * Provides common interface for all tremolo algorithms (sine, square, harmonic).
 * Uses virtual functions to allow runtime algorithm switching via base class pointers.
 */
class TremoloEffect {
public:
  virtual ~TremoloEffect() = default;

  /**
   * Initialize the tremolo effect.
   *
   * @param sample_rate Audio sample rate
   */
  virtual void Init(float sample_rate) = 0;

  /**
   * Process a single stereo sample pair.
   *
   * @param in_L Left input sample
   * @param in_R Right input sample
   * @param out_L Pointer to left output sample
   * @param out_R Pointer to right output sample
   */
  virtual void ProcessSample(float in_L, float in_R, float* out_L, float* out_R) = 0;

  /**
   * Set tremolo speed (LFO frequency).
   *
   * @param hz Frequency in Hz (typically 0.2-16 Hz)
   */
  virtual void SetSpeed(float hz) { speed_ = hz; }

  /**
   * Set tremolo depth (modulation amount).
   *
   * @param depth Depth 0-1 (derived classes may apply scaling)
   */
  virtual void SetDepth(float depth) { depth_ = depth; }

  /**
   * Get last LFO value for LED pulsing.
   *
   * @return LFO value (typically -1 to +1 or similar range)
   */
  virtual float GetLastLFOValue() const { return last_lfo_value_; }

protected:
  float speed_ = 4.0f;
  float depth_ = 0.5f;
  float last_lfo_value_ = 0.0f;
  float sample_rate_ = 48000.0f;
};

/**
 * Sine wave tremolo - smooth amplitude modulation.
 */
class SineTremolo : public TremoloEffect {
public:
  void Init(float sample_rate) override;
  void ProcessSample(float in_L, float in_R, float* out_L, float* out_R) override;

private:
  FlickOscillator osc_;
  float dc_offset_;
};

/**
 * Square wave (opto-style) tremolo - choppy amplitude modulation.
 */
class SquareTremolo : public TremoloEffect {
public:
  void Init(float sample_rate) override;
  void ProcessSample(float in_L, float in_R, float* out_L, float* out_R) override;

private:
  FlickOscillator osc_;
  float dc_offset_;
};

/**
 * Harmonic tremolo - band-split with opposite phase modulation.
 *
 * Splits signal into low (<144Hz) and high (>636Hz) bands, applies tremolo
 * with opposite phase to each band, then applies EQ shaping for authentic
 * vintage sound.
 */
class HarmonicTremolo : public TremoloEffect {
public:
  void Init(float sample_rate) override;
  void ProcessSample(float in_L, float in_R, float* out_L, float* out_R) override;
  void SetDepth(float depth) override;  // Applies 1.25x scaling

private:
  FlickOscillator osc_;

  // Band-splitting filters
  LowPassFilter lpf_L_, lpf_R_;
  HighPassFilter hpf_L_, hpf_R_;

  // EQ shaping filters (applied after band modulation)
  HighPassFilter eq_hpf1_L_, eq_hpf1_R_;
  LowPassFilter eq_lpf1_L_, eq_lpf1_R_;
  PeakingEQ eq_peak1_L_, eq_peak1_R_;
  PeakingEQ eq_peak2_L_, eq_peak2_R_;
  LowShelf eq_low_shelf_L_, eq_low_shelf_R_;
};

}  // namespace flick

#endif  // TREMOLO_EFFECT_H
