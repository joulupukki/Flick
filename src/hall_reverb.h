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

// 8-line Feedback Delay Network (FDN) hall reverb with Hadamard mixing matrix.
// Uses prime-length delay lines, input diffusion, per-line damping, and LFO
// modulation for dense, smooth, stereo hall reverb.

#ifndef HALL_REVERB_HPP
#define HALL_REVERB_HPP

#include "daisysp.h"
#include "reverb_effect.h"
#include <array>

namespace flick {

class HallReverb : public ReverbEffect {
public:
    HallReverb() = default;
    ~HallReverb() override = default;

    void Init(float sample_rate) override;
    void ProcessSample(float in_left, float in_right,
                       float* out_left, float* out_right) override;
    void Clear() override;

    void SetDecay(float decay) override;
    void SetTankHighCut(float freq) override;

private:
    static constexpr int kNumLines = 8;
    static constexpr int kNumInputAP = 2;
    static constexpr int kMaxLineDelay = 4800;
    static constexpr int kMaxAPDelay = 512;
    static constexpr float kHadamardNorm = 0.35355339f;

    static constexpr std::array<int, kNumLines> kLineDelays = {
        1087, 1283, 1601, 1949, 2311, 2801, 3371, 4409
    };

    static constexpr std::array<int, kNumInputAP> kInputAPDelays = {
        142, 379
    };

    // Bitmask: lines 0, 2, 5, 7 get LFO modulation
    static constexpr uint8_t kModMask = 0xA5; // 10100101
    // Maps line index to LFO phase index (-1 = not modulated)
    static constexpr int kModPhaseIdx[kNumLines] = {0, -1, 1, -1, -1, 2, -1, 3};

    struct AllPassFilter {
        daisysp::DelayLine<float, kMaxAPDelay> delay;
        float coeff = 0.5f;

        void Init() { delay.Init(); coeff = 0.5f; }

        float Process(float in) {
            float read = delay.Read();
            delay.Write(in + read * coeff);
            return -in * coeff + read;
        }
    };

    std::array<AllPassFilter, kNumInputAP> input_ap_;
    std::array<daisysp::OnePole, kNumLines> damping_;

    float lfo_phase_[4] = {};
    float lfo_phase_inc_ = 0.f;
    float mod_depth_ = 1.5f;
    float decay_ = 0.85f;
    float diffusion_coeff_ = 0.5f;

    static void hadamardTransform(float* x);
};

} // namespace flick

#endif // HALL_REVERB_HPP
