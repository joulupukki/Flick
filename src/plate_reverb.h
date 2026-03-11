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

#ifndef PLATE_REVERB_H
#define PLATE_REVERB_H

#include "reverb_effect.h"
#include "PlateauNEVersio/Dattorro.hpp"

namespace flick {

/**
 * Plate reverb using Dattorro's 1997 algorithm.
 *
 * Wraps the PlateauNEVersio Dattorro implementation and provides
 * the ReverbEffect interface. Features:
 * - Pre-delay (0-250ms)
 * - Input diffusion
 * - Tank diffusion (0-100%)
 * - High/low-cut filtering
 * - LFO modulation (speed, depth, shape)
 * - Decay control
 *
 * All parameters are persistent and saved to flash via Settings.
 */
class PlateReverb : public ReverbEffect {
public:
  PlateReverb();
  ~PlateReverb() override = default;

  void Init(float sample_rate) override;
  void ProcessSample(float in_L, float in_R, float* out_L, float* out_R) override;
  void Clear() override;

  // Override plate-specific parameters
  void SetDecay(float decay) override;
  void SetDiffusion(float diffusion) override;
  void SetPreDelay(float pre_delay) override;
  void SetInputHighCut(float freq) override;
  void SetTankHighCut(float freq) override;
  void SetTankModSpeed(float speed) override;
  void SetTankModDepth(float depth) override;
  void SetTankModShape(float shape) override;

  /**
   * Parameter structure for persistence (saved to Settings).
   */
  struct Params {
    float decay;
    float diffusion;
    float pre_delay;
    float input_cutoff_freq;
    float tank_cutoff_freq;
    int tank_mod_speed_pos;
    int tank_mod_depth_pos;
    int tank_mod_shape_pos;
  };

  /**
   * Get current parameters for saving to Settings.
   */
  Params GetParams() const;

  /**
   * Set parameters from Settings.
   */
  void SetParams(const Params& params);

private:
  Dattorro verb_;
  Params params_;

  /**
   * Apply current parameters to Dattorro instance.
   */
  void updateDattorroParameters();
};

}  // namespace flick

#endif  // PLATE_REVERB_H
