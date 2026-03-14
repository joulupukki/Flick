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

#ifndef REVERB_EFFECT_H
#define REVERB_EFFECT_H

namespace flick {

/**
 * Base class for reverb effects.
 *
 * Provides common interface for all reverb algorithms (plate, CloudSeed ambient, CloudSeed room).
 * Uses virtual functions to allow runtime algorithm switching via base class pointers.
 *
 * Design pattern: Virtual methods with no-op defaults for algorithm-specific parameters.
 * This allows the orchestrator to call any parameter setter on any reverb type safely,
 * with each derived class only responding to parameters it actually uses.
 */
class ReverbEffect {
public:
  virtual ~ReverbEffect() = default;

  /**
   * Initialize the reverb effect.
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
   * Clear reverb buffers (reset to silence).
   */
  virtual void Clear() = 0;

  // Algorithm-specific parameters (no-op by default, override if needed)

  /**
   * Set decay time/feedback.
   *
   * @param decay Decay amount 0-1
   */
  virtual void SetDecay(float decay) {}

  /**
   * Set diffusion amount.
   *
   * @param diffusion Diffusion 0-1
   */
  virtual void SetDiffusion(float diffusion) {}

  /**
   * Set pre-delay time.
   *
   * @param pre_delay Pre-delay in seconds
   */
  virtual void SetPreDelay(float pre_delay) {}

  /**
   * Set tone/brightness of reverb tail.
   *
   * Plate: tank high-cut filter frequency.
   * CloudSeed: post-processing cutoff frequency.
   *
   * @param tone Tone amount 0-1 (0=dark, 1=bright)
   */
  virtual void SetTone(float tone) {}

  /**
   * Set modulation depth (movement/shimmer).
   *
   * Plate: combined mod speed + depth.
   * CloudSeed: late reverb line modulation amount.
   *
   * @param mod Modulation amount 0-1
   */
  virtual void SetModulation(float mod) {}

protected:
  float sample_rate_ = 48000.0f;
};

}  // namespace flick

#endif  // REVERB_EFFECT_H
