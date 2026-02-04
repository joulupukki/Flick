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

// Based on Schroeder reverb algorithm

#ifndef HALL_REVERB_HPP
#define HALL_REVERB_HPP

#include "daisysp.h"
#include <array>

namespace flick {

class HallReverb {
public:
    HallReverb() = default;
    ~HallReverb() = default;

    void Init(float sample_rate, size_t max_delay = 4000); // ~83ms at 48kHz

    void Process(const float* in_left, const float* in_right,
                 float* out_left, float* out_right, size_t size);

    void ProcessSample(float in_left, float in_right, float* out_left, float* out_right);

    void SetFeedback(float feedback) { feedback_ = feedback; }
    void SetDryWet(float dry_wet) { dry_wet_ = dry_wet; }
    void SetLpFreq(float freq) { lp_freq_ = freq; }

private:
    static constexpr int kNumCombFilters = 4;
    static constexpr int kNumAllPassFilters = 2;

    // Comb filter delays in samples (for 48kHz) - longer for hall reverb
    static constexpr std::array<int, kNumCombFilters> kCombDelays = {
        2400,  // ~50ms
        2688,  // ~56ms
        2928,  // ~61ms
        3264   // ~68ms
    };

    // All-pass filter delays
    static constexpr std::array<int, kNumAllPassFilters> kAllPassDelays = {
        225,   // ~5ms
        341    // ~7ms
    };

    struct CombFilter {
        daisysp::DelayLine<float, 4000> delay;
        float feedback;
        float damp;
        float damp_prev;
        daisysp::OnePole lp_filter;

        void Init(float sample_rate, float lp_freq) {
            delay.Init();
            lp_filter.Init();
            lp_filter.SetFrequency(lp_freq);
            feedback = 0.0f;
            damp = 0.0f;
            damp_prev = 0.0f;
        }

        float Process(float in) {
            float read = delay.Read();
            float filtered = lp_filter.Process(read * (1.0f - damp) + read * damp * damp_prev);
            damp_prev = filtered;
            delay.Write(in + filtered * feedback);
            return filtered;
        }
    };

    struct AllPassFilter {
        daisysp::DelayLine<float, 500> delay;
        float feedback;

        void Init() {
            delay.Init();
            feedback = 0.0f;
        }

        float Process(float in) {
            float read = delay.Read();
            float out = -in + read;
            delay.Write(in + read * feedback);
            return out;
        }
    };

    std::array<CombFilter, kNumCombFilters> comb_filters_;
    std::array<AllPassFilter, kNumAllPassFilters> allpass_filters_;

    float feedback_ = 0.85f;
    float dry_wet_ = 1.0f;
    float lp_freq_ = 12000.0f;
    float sample_rate_ = 48000.0f;
};

} // namespace flick

#endif // HALL_REVERB_HPP