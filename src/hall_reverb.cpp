// Flick for Funbox DIY DSP Platform
// Copyright (C) 2026 Boyd Timothy <btimothy@gmail.com>
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

#include "hall_reverb.h"

namespace flick {

// Define the static constants
constexpr std::array<int, HallReverb::kNumCombFilters> HallReverb::kCombDelays;
constexpr std::array<int, HallReverb::kNumAllPassFilters> HallReverb::kAllPassDelays;

void HallReverb::Init(float sample_rate, size_t max_delay) {
    sample_rate_ = sample_rate;

    // Initialize comb filters
    for (auto& comb : comb_filters_) {
        comb.Init(sample_rate, lp_freq_);
        comb.feedback = feedback_;
        comb.damp = 0.5f; // Moderate damping
    }

    // Set comb filter delays
    for (size_t i = 0; i < kNumCombFilters; ++i) {
        comb_filters_[i].delay.SetDelay(static_cast<float>(kCombDelays[i]));
    }

    // Initialize all-pass filters
    for (auto& ap : allpass_filters_) {
        ap.Init();
        ap.feedback = 0.7f; // Standard all-pass feedback
    }

    // Set all-pass delays
    for (size_t i = 0; i < kNumAllPassFilters; ++i) {
        allpass_filters_[i].delay.SetDelay(static_cast<float>(kAllPassDelays[i]));
    }
}

void HallReverb::Process(const float* in_left, const float* in_right,
                         float* out_left, float* out_right, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        ProcessSample(in_left[i], in_right[i], &out_left[i], &out_right[i]);
    }
}

void HallReverb::ProcessSample(float in_left, float in_right, float* out_left, float* out_right) {
    // Mix left and right input for mono processing
    float mono_in = (in_left + in_right) * 0.5f;

    // Process through comb filters
    float comb_out = 0.0f;
    for (auto& comb : comb_filters_) {
        comb_out += comb.Process(mono_in);
    }
    comb_out /= kNumCombFilters; // Average the comb outputs

    // Process through all-pass filters in series
    float reverb_out = comb_out;
    for (auto& ap : allpass_filters_) {
        reverb_out = ap.Process(reverb_out);
    }

    // Apply dry/wet mix
    float dry = mono_in * (1.0f - dry_wet_);
    float wet = reverb_out * dry_wet_;

    // Output to both channels
    *out_left = dry + wet;
    *out_right = dry + wet;
}

} // namespace flick