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

#ifndef DELAY_EFFECT_H
#define DELAY_EFFECT_H

#include "daisysp.h"

namespace flick {

// Forward declaration - max delay size must match flick.cpp
// This is set to 2 seconds at 48kHz = 96000 samples
constexpr size_t DELAY_MAX_SIZE = 96000;

/**
 * Simple stereo delay effect with feedback and dry/wet mixing.
 *
 * Features:
 * - Independent left/right delay lines
 * - Smooth delay time interpolation (fonepole)
 * - Adjustable feedback (0-1)
 * - Adjustable dry/wet mix (0-1)
 *
 * Note: This class does NOT handle tap tempo logic - that remains in the
 * orchestrator (flick.cpp) as it's a UX concern.
 */
class DelayEffect {
public:
  DelayEffect() = default;
  ~DelayEffect() = default;

  /**
   * Initialize the delay effect.
   *
   * @param sample_rate Audio sample rate
   * @param mem_L Pointer to left channel delay line buffer (externally allocated)
   * @param mem_R Pointer to right channel delay line buffer (externally allocated)
   */
  void Init(float sample_rate,
            daisysp::DelayLine<float, DELAY_MAX_SIZE>* mem_L,
            daisysp::DelayLine<float, DELAY_MAX_SIZE>* mem_R);

  /**
   * Process a single stereo sample pair.
   *
   * @param in_L Left input sample
   * @param in_R Right input sample
   * @param out_L Pointer to left output sample
   * @param out_R Pointer to right output sample
   */
  void ProcessSample(float in_L, float in_R, float* out_L, float* out_R);

  /**
   * Set delay time in samples.
   * The actual delay time will be smoothly interpolated to avoid clicks.
   *
   * @param samples Delay time in samples
   */
  void SetDelayTime(float samples);

  /**
   * Set feedback amount.
   *
   * @param feedback Feedback coefficient (0.0 = no feedback, 1.0 = max feedback)
   */
  void SetFeedback(float feedback);

  /**
   * Clear delay buffers (reset to silence).
   */
  void Clear();

private:
  daisysp::DelayLine<float, DELAY_MAX_SIZE>* del_L_ = nullptr;
  daisysp::DelayLine<float, DELAY_MAX_SIZE>* del_R_ = nullptr;

  float current_delay_L_ = 0.0f;
  float current_delay_R_ = 0.0f;
  float delay_target_ = 0.0f;
  float feedback_ = 0.0f;
  float sample_rate_ = 48000.0f;
};

}  // namespace flick

#endif  // DELAY_EFFECT_H
