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

// IR convolution buffers in SDRAM
static float DSY_SDRAM_BSS irFreqBuf[CONV_IRFREQ_SIZE];
static float DSY_SDRAM_BSS fdlBuf[CONV_FDL_SIZE];

// Late tail FDN delay line buffers in SDRAM (4 x 3600 samples = ~56 KB)
using daisysp::DelayLine;

static DelayLine<float, 3600> DSY_SDRAM_BSS spring_fdn_mem_0;
static DelayLine<float, 3600> DSY_SDRAM_BSS spring_fdn_mem_1;
static DelayLine<float, 3600> DSY_SDRAM_BSS spring_fdn_mem_2;
static DelayLine<float, 3600> DSY_SDRAM_BSS spring_fdn_mem_3;

static DelayLine<float, 3600>* spring_fdn[4] = {
    &spring_fdn_mem_0, &spring_fdn_mem_1,
    &spring_fdn_mem_2, &spring_fdn_mem_3
};

namespace flick {

constexpr int SpringReverb::kFdnDelays[kNumFdnLines];
constexpr int SpringReverb::kInputAPDelays[kNumInputAP];

void SpringReverb::Init(float sample_rate) {
    sample_rate_ = sample_rate;

    // IR convolution engine
    convolution_.Init(irFreqBuf, fdlBuf);
    using namespace ImpulseResponseData;
    if (IR_COUNT > 0) {
        const IRInfo& ir = ir_collection[0];
        convolution_.Prepare(ir.data, ir.length);
    }

    // Late tail FDN delay lines
    for (int i = 0; i < kNumFdnLines; ++i) {
        spring_fdn[i]->Init();
        spring_fdn[i]->SetDelay(static_cast<float>(kFdnDelays[i]));
    }

    // FDN damping filters (low-pass for dark spring tail)
    float damp_nf = kFdnDampingFreq / sample_rate;
    for (auto& d : fdn_damping_) {
        d.Init();
        d.SetFrequency(damp_nf);
    }

    // Input diffusion allpasses
    for (int i = 0; i < kNumInputAP; ++i)
        input_ap_[i].Init(kInputAPDelays[i]);

    memset(inputBuf_, 0, sizeof(inputBuf_));
    memset(outputBuf_, 0, sizeof(outputBuf_));
    bufPos_ = 0;
}

void SpringReverb::ProcessSample(float in_left, float in_right,
                                  float* out_left, float* out_right) {
    float mono_in = (in_left + in_right) * 0.5f;

    // Feed dry input to convolution (no feedback recirculation)
    inputBuf_[bufPos_] = mono_in;

    // Read convolution output
    float conv_out = outputBuf_[bufPos_];

    bufPos_++;
    if (bufPos_ >= NUCONV_BLOCK_SIZE) {
        convolution_.ProcessBlock(inputBuf_, outputBuf_);
        bufPos_ = 0;
    }

    // --- Late tail FDN ---

    // Diffuse convolution output through allpass filters before FDN
    float fdn_input = conv_out * kFdnInputGain;
    for (auto& ap : input_ap_)
        fdn_input = ap.Process(fdn_input);

    float fdn_in_scaled = fdn_input * kHadamard4Norm;

    // Read FDN delay line outputs
    float out[kNumFdnLines];
    for (int i = 0; i < kNumFdnLines; ++i)
        out[i] = spring_fdn[i]->Read();

    // Hadamard mix
    float mx[kNumFdnLines];
    for (int i = 0; i < kNumFdnLines; ++i) mx[i] = out[i];
    hadamard4(mx);

    // Damp, decay, inject new input, write back
    for (int i = 0; i < kNumFdnLines; ++i)
        spring_fdn[i]->Write(fdn_damping_[i].Process(mx[i]) * kFdnDecay + fdn_in_scaled);

    // Sum FDN outputs (mono)
    float fdn_out = (out[0] + out[1] + out[2] + out[3]) * 0.25f;

    // Combine: conv output (spring character) + FDN output (tail)
    float wet = (conv_out + fdn_out * kFdnOutputGain) * 10.0f;

    // NaN guard
    if (wet != wet) wet = 0.0f;

    *out_left = wet;
    *out_right = wet;
}

void SpringReverb::Clear() {
    convolution_.Reset();
    memset(inputBuf_, 0, sizeof(inputBuf_));
    memset(outputBuf_, 0, sizeof(outputBuf_));
    bufPos_ = 0;

    for (int i = 0; i < kNumFdnLines; ++i)
        spring_fdn[i]->Reset();
    for (auto& ap : input_ap_)
        ap.delay.Reset();
}

void SpringReverb::hadamard4(float* x) {
    // 4x4 Hadamard via butterfly decomposition
    float a, b;
    // Stage 1: pairs
    a = x[0]; b = x[1];
    x[0] = a + b; x[1] = a - b;
    a = x[2]; b = x[3];
    x[2] = a + b; x[3] = a - b;
    // Stage 2: quads
    a = x[0]; b = x[2];
    x[0] = a + b; x[2] = a - b;
    a = x[1]; b = x[3];
    x[1] = a + b; x[3] = a - b;
    // Normalize
    for (int i = 0; i < 4; ++i)
        x[i] *= kHadamard4Norm;
}

} // namespace flick
