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
#include "ir_data.h"
#include <cmath>
#include <cstring>

// SDRAM buffers for convolution engine.
// For a 170ms IR at 48kHz: (8160 + 127) / 128 = 64 partitions.
// MAX_PARTITIONS is sized for headroom if we increase IR length later.
#define DSY_SDRAM_BSS __attribute__((section(".sdram_bss")))

static constexpr size_t MAX_PARTITIONS = 128;

static float DSY_SDRAM_BSS irFreqBuf[MAX_PARTITIONS * CONV_FFT_SIZE];
static float DSY_SDRAM_BSS fdlBuf[MAX_PARTITIONS * CONV_FFT_SIZE];

namespace flick {

void SpringReverb::Init(float sample_rate) {
    sample_rate_ = sample_rate;

    // Initialize convolution engine with SDRAM buffers
    convolution_.Init(MAX_PARTITIONS, irFreqBuf, fdlBuf);

    // Load the spring reverb IR (first IR in the collection)
    using namespace ImpulseResponseData;
    if (IR_COUNT > 0) {
        const IRInfo& ir = ir_collection[0];
        convolution_.Prepare(ir.data, ir.length);
    }

    // Compute one-pole LPF coefficient for feedback damping
    // alpha = 1 - exp(-2pi * freq / samplerate)
    dampingCoeff_ = 1.0f - expf(-6.2831853f * DAMPING_FREQ / sample_rate);
    dampingState_ = 0.0f;
    feedback_ = 0.0f;

    // Clear internal buffers
    memset(inputBuf_, 0, sizeof(inputBuf_));
    memset(outputBuf_, 0, sizeof(outputBuf_));
    bufPos_ = 0;
}

void SpringReverb::ProcessSample(float in_left, float in_right, float* out_left, float* out_right) {
    // Mix stereo to mono (spring reverb is inherently mono)
    float mono_in = (in_left + in_right) * 0.5f;

    // Add damped feedback from previous convolution output to extend the tail.
    // Each recirculation passes through the IR again, adding ~170ms of decay.
    inputBuf_[bufPos_] = mono_in + feedback_;

    // Read output from previous convolution result
    float wet = outputBuf_[bufPos_];

    // Update feedback: low-pass filter the convolution output so highs
    // decay faster than lows (natural spring reverb behavior)
    dampingState_ += dampingCoeff_ * (wet - dampingState_);
    feedback_ = dampingState_ * FEEDBACK;

    bufPos_++;

    // When buffer is full, run convolution
    if (bufPos_ >= CONV_PARTITION_SIZE) {
        convolution_.ProcessBlock(inputBuf_, outputBuf_, CONV_PARTITION_SIZE);
        bufPos_ = 0;
    }

    // The orchestrator attenuates reverb input by ~-20 dB to prevent
    // feedback-based reverbs (plate/hall) from clipping. Convolution reverb
    // is linear (no internal gain), so we compensate here.
    wet *= 10.0f;

    // Safety: prevent NaN from corrupting downstream effects
    if (wet != wet) wet = 0.0f;

    // Output mono wet signal to both channels
    // (orchestrator handles dry/wet mixing)
    *out_left = wet;
    *out_right = wet;
}

void SpringReverb::Clear() {
    convolution_.Reset();
    memset(inputBuf_, 0, sizeof(inputBuf_));
    memset(outputBuf_, 0, sizeof(outputBuf_));
    bufPos_ = 0;
    feedback_ = 0.0f;
    dampingState_ = 0.0f;
}

} // namespace flick
