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

// Digital waveguide model of spring reverb
// Goal: Emulate 1960'ss Fender Deluxe Reverb

#ifndef SPRING_REVERB_HPP
#define SPRING_REVERB_HPP

#include "daisysp.h"
#include "reverb_effect.h"
#include <array>

namespace flick {

class SpringReverb : public ReverbEffect {
public:
    SpringReverb() = default;
    ~SpringReverb() override = default;

    // ReverbEffect interface
    void Init(float sample_rate) override;
    void ProcessSample(float in_left, float in_right, float* out_left, float* out_right) override;
    void Clear() override;

    // Override parameters this reverb uses
    void SetDecay(float decay) override { decay_ = decay; }
    void SetMix(float mix) override { mix_ = mix; }
    void SetPreDelay(float pre_delay) override {
        float samples = pre_delay * sample_rate_;
        pre_delay_.SetDelay(samples);
    }

    // Spring-specific parameters (not in base class)
    void SetDamping(float damping_freq) {
        lp_filter_.SetFrequency(damping_freq);
    }

    // Block processing (non-virtual, spring-specific)
    void Process(const float* in_left, const float* in_right,
                 float* out_left, float* out_right, size_t size);

private:
    static constexpr int kNumAllPassFilters = 4;

    // All-pass filter delays in samples (for 48kHz) - short for spring reverb
    static constexpr std::array<int, kNumAllPassFilters> kAllPassDelays = {
        120,   // ~2.5ms
        240,   // ~5ms
        336,   // ~7ms
        480    // ~10ms
    };

    // Main delay for recirculation (spring length)
    daisysp::DelayLine<float, 4800> main_delay_;  // ~100ms max for audible reverb

    // Tap delays for spring "boing"
    daisysp::DelayLine<float, 2400> tap_delay_1_;
    daisysp::DelayLine<float, 2400> tap_delay_2_;

    // Pre-delay buffer
    daisysp::DelayLine<float, 256> pre_delay_;  // ~5ms max

    struct AllPassFilter {
        daisysp::DelayLine<float, 512> delay;
        float feedback;

        void Init(float fb) {
            delay.Init();
            feedback = fb;
        }

        float Process(float in) {
            float read = delay.Read();
            float out = -in + read;
            delay.Write(in + read * feedback);
            return out;
        }
    };

    std::array<AllPassFilter, kNumAllPassFilters> allpass_filters_;

    // Low-pass filter for high-frequency damping
    daisysp::OnePole lp_filter_;

    float decay_ = 0.65f;     // Feedback gain
    float mix_ = 0.5f;        // Wet/dry mix (0-1)
    float drive_ = 1.4f;      // Input drive for spring character
    float sample_rate_ = 48000.0f;
};

} // namespace flick

#endif // SPRING_REVERB_HPP