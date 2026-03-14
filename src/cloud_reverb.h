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

// Ambient reverb based on CloudSeed algorithm
// Original CloudSeed by ValdemarOrn (MIT license)
// Daisy port by erwincoumans, GuitarML adaptation for Terrarium
// Adapted for Flick stereo operation

#ifndef CLOUD_REVERB_H
#define CLOUD_REVERB_H

#include "reverb_effect.h"

namespace CloudSeed {
  class ReverbController;
}

namespace flick {

class CloudReverb : public ReverbEffect {
public:
  // Cloud reverb type identifiers — used for type-aware preset switching.
  enum CloudReverbType { CLOUD_AMBIENT = 0, CLOUD_ROOM = 1 };

  CloudReverb() = default;
  ~CloudReverb() override = default;

  // ReverbEffect interface
  void Init(float sample_rate) override;
  void ProcessSample(float in_L, float in_R, float* out_L, float* out_R) override;
  void Clear() override;

  // Override parameters relevant to CloudSeed
  void SetDecay(float decay) override;         // Maps to LineDecay
  void SetDiffusion(float diffusion) override;  // Maps to LateDiffusionFeedback
  void SetPreDelay(float pre_delay) override;   // Maps to PreDelay
  void SetTone(float tone) override;            // Maps to PostCutoffFrequency
  void SetModulation(float mod) override;       // Maps to LineModAmount

  // Preset selection (for experimentation — change preset and reflash)
  void SetPreset(int preset_index);

  // Set output gain boost (for volume matching)
  void SetOutputGain(float gain) { output_gain_ = gain; }

  // Number of available presets
  static const int kNumPresets = 9;

  // Apply any pending deferred parameter changes (called from main loop)
  void ApplyPendingParams();

  // Deferred preset switch — call from audio callback, applied in main loop.
  // Clears buffers, loads new preset, applies overrides.
  void RequestPresetSwitch(int preset_index, float output_gain);
  bool HasPendingPresetSwitch() const { return pending_preset_ >= 0; }
  void ApplyPendingPresetSwitch();

  // High-level type switch — encapsulates type→preset mapping and gain.
  // Call from audio callback; applied by main loop via ApplyPendingPresetSwitch().
  void RequestTypeSwitch(CloudReverbType type);

private:
  CloudSeed::ReverbController* controller_ = nullptr;
  float output_gain_ = 1.0f;
  int active_preset_ = -1;
  int pending_preset_ = -1;
  float pending_output_gain_ = 1.0f;
  void applyFlickOverrides();  // Sets DryOut=0, LineCount, and preset-specific tweaks

  // Deferred parameter updates — SetParameter triggers expensive operations
  // (SHA-256 hashing in UpdateLines, biquad coefficient recalculation) that
  // must NOT run inside the audio callback. Parameters are stored here and
  // applied from the main loop via ApplyPendingParams().
  struct PendingParams {
    float pre_delay = -1.0f;
    float decay = -1.0f;
    float tone = -1.0f;
    float modulation = -1.0f;
    float diffusion = -1.0f;
    bool has_pending = false;
  };
  volatile PendingParams pending_;

  // Block buffering: CloudSeed processes in blocks of 8 (matching Flick's audio callback).
  // All 8 output samples come from the same processing pass — no discontinuity.
  static constexpr int kBlockSize = 8;
  float in_buf_L_[kBlockSize] = {};
  float in_buf_R_[kBlockSize] = {};
  float out_buf_L_[kBlockSize] = {};
  float out_buf_R_[kBlockSize] = {};
  int buf_pos_ = 0;
};

}  // namespace flick

#endif  // CLOUD_REVERB_H
