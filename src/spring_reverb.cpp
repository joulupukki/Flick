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

#define DSY_SDRAM_BSS __attribute__((section(".sdram_bss")))

static float DSY_SDRAM_BSS irFreqBuf[CONV_IRFREQ_SIZE];
static float DSY_SDRAM_BSS fdlBuf[CONV_FDL_SIZE];

namespace flick {

void SpringReverb::Init(float sample_rate) {
    sample_rate_ = sample_rate;

    convolution_.Init(irFreqBuf, fdlBuf);

    using namespace ImpulseResponseData;
    if (IR_COUNT > 0) {
        const IRInfo& ir = ir_collection[0];
        convolution_.Prepare(ir.data, ir.length);
    }

    dampingCoeff_ = 1.0f - expf(-6.2831853f * DAMPING_FREQ / sample_rate);
    dampingState_ = 0.0f;
    feedback_ = 0.0f;

    memset(inputBuf_, 0, sizeof(inputBuf_));
    memset(outputBuf_, 0, sizeof(outputBuf_));
    bufPos_ = 0;
}

void SpringReverb::ProcessSample(float in_left, float in_right, float* out_left, float* out_right) {
    float mono_in = (in_left + in_right) * 0.5f;

    inputBuf_[bufPos_] = mono_in + feedback_;

    float wet = outputBuf_[bufPos_];

    dampingState_ += dampingCoeff_ * (wet - dampingState_);
    feedback_ = dampingState_ * FEEDBACK;

    bufPos_++;

    if (bufPos_ >= NUCONV_BLOCK_SIZE) {
        convolution_.ProcessBlock(inputBuf_, outputBuf_);
        bufPos_ = 0;
    }

    wet *= 10.0f;

    if (wet != wet) wet = 0.0f;

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
