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

#include "spring_reverb.h"
#include <cmath>

namespace flick {

// Define the static constants
constexpr std::array<int, SpringReverb::kNumAllPassFilters> SpringReverb::kAllPassDelays;

void SpringReverb::Init(float sample_rate) {
    sample_rate_ = sample_rate;

    // Initialize pre-delay (~1.3ms)
    pre_delay_.Init();
    pre_delay_.SetDelay(64.0f);

    // Initialize main delay (~50ms for spring length - more audible)
    main_delay_.Init();
    main_delay_.SetDelay(2400.0f);  // ~50ms at 48kHz

    // Initialize tap delays for spring "boing"
    tap_delay_1_.Init();
    tap_delay_1_.SetDelay(600.0f);  // ~12.5ms
    tap_delay_2_.Init();
    tap_delay_2_.SetDelay(1200.0f); // ~25ms

    // Initialize all-pass filters
    for (auto& ap : allpass_filters_) {
        ap.Init(0.5f);  // Lower feedback for stable dispersion
    }

    // Set all-pass delays
    for (size_t i = 0; i < kNumAllPassFilters; ++i) {
        allpass_filters_[i].delay.SetDelay(static_cast<float>(kAllPassDelays[i]));
    }

    // Initialize low-pass filter for damping (default 6kHz cutoff)
    lp_filter_.Init();
    lp_filter_.SetFrequency(6000.0f);
}

void SpringReverb::Clear() {
    // Reset all delay line buffers to silence
    pre_delay_.Reset();
    main_delay_.Reset();
    tap_delay_1_.Reset();
    tap_delay_2_.Reset();
    for (auto& ap : allpass_filters_) {
        ap.delay.Reset();
    }
}

void SpringReverb::Process(const float* in_left, const float* in_right,
                           float* out_left, float* out_right, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        ProcessSample(in_left[i], in_right[i], &out_left[i], &out_right[i]);
    }
}

void SpringReverb::ProcessSample(float in_left, float in_right, float* out_left, float* out_right) {
    // Mix left and right input for mono processing
    float mono_in = (in_left + in_right) * 0.5f;

    // Pre-delay and drive for spring character
    pre_delay_.Write(mono_in);
    float input = pre_delay_.Read() * drive_;
    input = std::tanh(input);

    // Read from main delay (recirculating signal)
    float recirc = main_delay_.Read();

    // Apply damping (low-pass filter) to recirculating signal
    float damped = lp_filter_.Process(recirc);

    // Calculate feedback
    float feedback = damped * decay_;

    // Add dispersion via all-pass chain
    float dispersive = input;
    for (auto& ap : allpass_filters_) {
        dispersive = ap.Process(dispersive);
    }

    // Write to main delay: dispersive input + feedback
    main_delay_.Write(dispersive + feedback);

    // Tap delays for spring "boing"
    tap_delay_1_.Write(damped);
    tap_delay_2_.Write(damped);
    float tap1 = tap_delay_1_.Read();
    float tap2 = tap_delay_2_.Read();

    // Mix dry and wet signals
    float dry = mono_in * (1.0f - mix_);
    float wet = (0.55f * damped + 0.25f * tap1 + 0.20f * tap2) * mix_;

    // Output to both channels
    *out_left = dry + wet;
    *out_right = dry + wet;
}

} // namespace flick