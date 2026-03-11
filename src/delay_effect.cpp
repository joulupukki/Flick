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

#include "delay_effect.h"

using daisysp::fonepole;

namespace flick {

void DelayEffect::Init(float sample_rate,
                       daisysp::DelayLine<float, DELAY_MAX_SIZE>* mem_L,
                       daisysp::DelayLine<float, DELAY_MAX_SIZE>* mem_R) {
  sample_rate_ = sample_rate;
  del_L_ = mem_L;
  del_R_ = mem_R;

  // Initialize delay lines
  del_L_->Init();
  del_R_->Init();

  current_delay_L_ = 0.0f;
  current_delay_R_ = 0.0f;
  delay_target_ = 0.0f;
  feedback_ = 0.0f;
}

void DelayEffect::ProcessSample(float in_L, float in_R, float* out_L, float* out_R) {
  // Smooth delay time changes to avoid clicks/pops
  // Alpha value of 0.0002f gives very slow, smooth transitions
  fonepole(current_delay_L_, delay_target_, 0.0002f);
  fonepole(current_delay_R_, delay_target_, 0.0002f);

  // Set delay times
  del_L_->SetDelay(current_delay_L_);
  del_R_->SetDelay(current_delay_R_);

  // Read from delay lines
  float read_L = del_L_->Read();
  float read_R = del_R_->Read();

  // Write to delay lines with feedback
  del_L_->Write((feedback_ * read_L) + in_L);
  del_R_->Write((feedback_ * read_R) + in_R);

  // Return wet signal only (orchestrator handles dry/wet mixing and makeup gain)
  *out_L = read_L;
  *out_R = read_R;
}

void DelayEffect::SetDelayTime(float samples) {
  delay_target_ = samples;
}

void DelayEffect::SetFeedback(float feedback) {
  feedback_ = feedback;
}

void DelayEffect::Clear() {
  // Note: DaisySP DelayLine doesn't have a Clear() method,
  // so we just reset the delay time to 0 which effectively clears it
  current_delay_L_ = 0.0f;
  current_delay_R_ = 0.0f;
  delay_target_ = 0.0f;
}

}  // namespace flick
