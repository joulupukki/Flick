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

// IR-based spring reverb using partitioned overlap-save convolution.
// Uses a pre-recorded spring reverb impulse response (~170ms) for the
// characteristic spring sound. A feedback loop with low-pass damping
// extends the tail beyond the IR length.

#ifndef SPRING_REVERB_HPP
#define SPRING_REVERB_HPP

#include "reverb_effect.h"
#include "non_uniform_convolution_engine.h"

namespace flick {

class SpringReverb : public ReverbEffect {
public:
    SpringReverb() = default;
    ~SpringReverb() override = default;

    void Init(float sample_rate) override;
    void ProcessSample(float in_left, float in_right, float* out_left, float* out_right) override;
    void Clear() override;

private:
    NonUniformConvolutionEngine convolution_;

    float inputBuf_[NUCONV_BLOCK_SIZE];
    float outputBuf_[NUCONV_BLOCK_SIZE];
    size_t bufPos_ = 0;

    // Feedback recirculation to extend the reverb tail beyond the IR length.
    static constexpr float FEEDBACK = 0.5f;
    static constexpr float DAMPING_FREQ = 3500.0f;

    float feedback_ = 0.0f;
    float dampingState_ = 0.0f;
    float dampingCoeff_ = 0.0f;

    float sample_rate_ = 48000.0f;
};

} // namespace flick

#endif // SPRING_REVERB_HPP
