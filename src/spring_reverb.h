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

// Hybrid IR + FDN spring reverb.
// Uses a pre-recorded impulse response (~170ms) for authentic spring character,
// followed by a 4-line Feedback Delay Network that extends the tail to ~1-2s.

#ifndef SPRING_REVERB_HPP
#define SPRING_REVERB_HPP

#include "reverb_effect.h"
#include "non_uniform_convolution_engine.h"
#include "daisysp.h"
#include <array>

namespace flick {

class SpringReverb : public ReverbEffect {
public:
    SpringReverb() = default;
    ~SpringReverb() override = default;

    void Init(float sample_rate) override;
    void ProcessSample(float in_left, float in_right, float* out_left, float* out_right) override;
    void Clear() override;

private:
    // --- IR convolution ---
    NonUniformConvolutionEngine convolution_;
    float inputBuf_[NUCONV_BLOCK_SIZE];
    float outputBuf_[NUCONV_BLOCK_SIZE];
    size_t bufPos_ = 0;

    // --- Late tail FDN ---
    static constexpr int kNumFdnLines = 4;
    static constexpr int kMaxFdnDelay = 3600;
    static constexpr float kHadamard4Norm = 0.5f; // 1/sqrt(4)

    static constexpr int kFdnDelays[kNumFdnLines] = {1361, 1873, 2657, 3547};

    // Input diffusion allpasses (smear conv output before FDN)
    static constexpr int kNumInputAP = 2;
    static constexpr int kMaxAPDelay = 200;
    static constexpr int kInputAPDelays[kNumInputAP] = {113, 199};
    static constexpr float kAPCoeff = 0.6f;

    // FDN tuning constants
    static constexpr float kFdnDecay = 0.90f;        // ~1.5s RT60
    static constexpr float kFdnDampingFreq = 3000.0f; // Dark spring tail
    static constexpr float kFdnInputGain = 0.25f;     // Conv output → FDN
    static constexpr float kFdnOutputGain = 0.7f;     // FDN contribution to mix

    struct AllPassFilter {
        daisysp::DelayLine<float, kMaxAPDelay> delay;
        float coeff = kAPCoeff;

        void Init(int delay_samples) {
            delay.Init();
            delay.SetDelay(static_cast<float>(delay_samples));
            coeff = kAPCoeff;
        }

        float Process(float in) {
            float read = delay.Read();
            delay.Write(in + read * coeff);
            return -in * coeff + read;
        }
    };

    std::array<daisysp::OnePole, kNumFdnLines> fdn_damping_;
    std::array<AllPassFilter, kNumInputAP> input_ap_;

    static void hadamard4(float* x);
};

} // namespace flick

#endif // SPRING_REVERB_HPP
