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

  // Override reverb parameters (unified edit mode interface)
  void SetDecay(float decay) override;
  void SetDiffusion(float diffusion) override;
  void SetPreDelay(float pre_delay) override;
  void SetTone(float tone) override;
  void SetModulation(float mod) override;

private:
  Dattorro verb_;

  /**
   * Apply default editable parameters to Dattorro instance.
   * Called during Init(); loadSettings() will override with saved values.
   */
  void updateDattorroParameters();
};

}  // namespace flick

#endif  // PLATE_REVERB_H
