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
// Uses a pre-recorded spring reverb impulse response for realistic sound.
// Convolution engine adapted from MuleBox (github.com/optilude/MuleBox).
//
// The IR captures the first ~170ms of a real spring reverb. To extend the
// tail beyond the IR length, a feedback loop recirculates the convolution
// output back into its input with low-pass damping (highs decay faster
// than lows, mimicking real spring behavior).

#ifndef SPRING_REVERB_HPP
#define SPRING_REVERB_HPP

#include "reverb_effect.h"
#include "convolution_engine.h"

namespace flick {

class SpringReverb : public ReverbEffect {
public:
    SpringReverb() = default;
    ~SpringReverb() override = default;

    // ReverbEffect interface
    void Init(float sample_rate) override;
    void ProcessSample(float in_left, float in_right, float* out_left, float* out_right) override;
    void Clear() override;

    // SetMix inherited from ReverbEffect (used by orchestrator for dry/wet)
    // All other parameter setters (SetDecay, SetPreDelay, etc.) use
    // inherited no-op defaults since the IR captures these characteristics.

private:
    ConvolutionEngine convolution_;

    // Internal buffering: accumulate samples until we have a full block
    // for the convolution engine (CONV_PARTITION_SIZE = 128 samples).
    float inputBuf_[CONV_PARTITION_SIZE];
    float outputBuf_[CONV_PARTITION_SIZE];
    size_t bufPos_ = 0;

    // Feedback recirculation to extend the reverb tail beyond the IR length.
    // Each round trip through the IR adds another ~170ms of tail.
    static constexpr float FEEDBACK = 0.5f;      // Feedback amount (0=none, <1=stable)
    static constexpr float DAMPING_FREQ = 4000.0f; // LPF cutoff for feedback path

    float feedback_ = 0.0f;       // Current feedback sample
    float dampingState_ = 0.0f;   // One-pole LPF state
    float dampingCoeff_ = 0.0f;   // LPF coefficient (computed in Init)
};

} // namespace flick

#endif // SPRING_REVERB_HPP
