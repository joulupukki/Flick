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

#include "hall_reverb.h"

#ifndef DSY_SDRAM_BSS
#define DSY_SDRAM_BSS __attribute__((section(".sdram_bss")))
#endif

using daisysp::DelayLine;

// FDN delay line buffers in SDRAM (8 x 4800 samples = ~150KB)
DelayLine<float, 4800> DSY_SDRAM_BSS fdn_delay_mem_0;
DelayLine<float, 4800> DSY_SDRAM_BSS fdn_delay_mem_1;
DelayLine<float, 4800> DSY_SDRAM_BSS fdn_delay_mem_2;
DelayLine<float, 4800> DSY_SDRAM_BSS fdn_delay_mem_3;
DelayLine<float, 4800> DSY_SDRAM_BSS fdn_delay_mem_4;
DelayLine<float, 4800> DSY_SDRAM_BSS fdn_delay_mem_5;
DelayLine<float, 4800> DSY_SDRAM_BSS fdn_delay_mem_6;
DelayLine<float, 4800> DSY_SDRAM_BSS fdn_delay_mem_7;

static DelayLine<float, 4800>* fdn_delay[8] = {
    &fdn_delay_mem_0, &fdn_delay_mem_1, &fdn_delay_mem_2, &fdn_delay_mem_3,
    &fdn_delay_mem_4, &fdn_delay_mem_5, &fdn_delay_mem_6, &fdn_delay_mem_7
};

namespace flick {

constexpr std::array<int, HallReverb::kNumLines> HallReverb::kLineDelays;
constexpr std::array<int, HallReverb::kNumInputAP> HallReverb::kInputAPDelays;
constexpr int HallReverb::kModPhaseIdx[HallReverb::kNumLines];

void HallReverb::Init(float sample_rate) {
    sample_rate_ = sample_rate;

    for (int i = 0; i < kNumLines; ++i) {
        fdn_delay[i]->Init();
        fdn_delay[i]->SetDelay(static_cast<float>(kLineDelays[i]));
    }

    for (int i = 0; i < kNumInputAP; ++i) {
        input_ap_[i].Init();
        input_ap_[i].delay.SetDelay(static_cast<float>(kInputAPDelays[i]));
        input_ap_[i].coeff = diffusion_coeff_;
    }

    pre_delay_line_.Init();
    pre_delay_line_.SetDelay(pre_delay_samples_);

    float damp_freq = 8000.0f / sample_rate_;
    for (auto& d : damping_) {
        d.Init();
        d.SetFrequency(damp_freq);
    }

    input_highcut_.Init();
    input_highcut_.SetFrequency(10000.0f / sample_rate_);

    lfo_phase_[0] = 0.0f;
    lfo_phase_[1] = 1.57079632f;
    lfo_phase_[2] = 3.14159265f;
    lfo_phase_[3] = 4.71238898f;
    lfo_phase_inc_ = 3.14159265f / sample_rate_; // 0.5 Hz default
}

void HallReverb::ProcessSample(float in_left, float in_right,
                                float* out_left, float* out_right) {
    float input = (in_left + in_right) * 0.5f;

    // Input high-cut filter
    input = input_highcut_.Process(input);

    // Pre-delay
    pre_delay_line_.Write(input);
    input = pre_delay_line_.Read();

    // Input diffusion: 4 all-pass filters in series
    for (auto& ap : input_ap_)
        input = ap.Process(input);

    float in_scaled = input * kHadamardNorm;

    // Read FDN delay line outputs
    float out[kNumLines];
    for (int i = 0; i < kNumLines; ++i) {
        if ((kModMask & (1 << i)) && mod_depth_ > 0.0f) {
            // Triangle wave LFO: phase 0..2pi → output -1..1
            float p = lfo_phase_[kModPhaseIdx[i]] * (1.0f / 3.14159265f);
            float tri = (p < 1.0f) ? (2.0f * p - 1.0f) : (3.0f - 2.0f * p);
            float md = static_cast<float>(kLineDelays[i]) + tri * mod_depth_;
            if (md < 1.0f) md = 1.0f;
            out[i] = fdn_delay[i]->Read(md);
        } else {
            out[i] = fdn_delay[i]->Read();
        }
    }

    // Hadamard mix
    float mx[kNumLines];
    for (int i = 0; i < kNumLines; ++i) mx[i] = out[i];
    hadamardTransform(mx);

    // Damp, decay, write
    for (int i = 0; i < kNumLines; ++i)
        fdn_delay[i]->Write(damping_[i].Process(mx[i]) * decay_ + in_scaled);

    // LFO advance
    for (int m = 0; m < 4; ++m) {
        lfo_phase_[m] += lfo_phase_inc_;
        if (lfo_phase_[m] >= 6.28318530f)
            lfo_phase_[m] -= 6.28318530f;
    }

    // Stereo taps with alternating polarity
    *out_left  = (out[0] - out[2] + out[4] - out[6]) * 0.5f;
    *out_right = (out[1] - out[3] + out[5] - out[7]) * 0.5f;
}

void HallReverb::Clear() {
    for (int i = 0; i < kNumLines; ++i)
        fdn_delay[i]->Reset();
    for (auto& ap : input_ap_)
        ap.delay.Reset();
    pre_delay_line_.Reset();
}

void HallReverb::SetDecay(float decay) {
    decay_ = (decay > 0.999f) ? 0.999f : ((decay < 0.0f) ? 0.0f : decay);
}

void HallReverb::SetDiffusion(float diffusion) {
    diffusion_coeff_ = diffusion * 0.7f;
    for (auto& ap : input_ap_) ap.coeff = diffusion_coeff_;
}

void HallReverb::SetPreDelay(float pre_delay) {
    pre_delay_samples_ = pre_delay * sample_rate_;
    if (pre_delay_samples_ > static_cast<float>(kMaxPreDelay - 1))
        pre_delay_samples_ = static_cast<float>(kMaxPreDelay - 1);
    if (pre_delay_samples_ < 0.0f)
        pre_delay_samples_ = 0.0f;
    pre_delay_line_.SetDelay(pre_delay_samples_);
}

void HallReverb::SetInputHighCut(float freq) {
    float nf = freq / sample_rate_;
    if (nf > 0.497f) nf = 0.497f;
    if (nf < 0.001f) nf = 0.001f;
    input_highcut_.SetFrequency(nf);
}

void HallReverb::SetTankHighCut(float freq) {
    float nf = freq / sample_rate_;
    if (nf > 0.497f) nf = 0.497f;
    if (nf < 0.001f) nf = 0.001f;
    for (auto& d : damping_) d.SetFrequency(nf);
}

void HallReverb::SetTankModSpeed(float speed) {
    if (speed < 0.01f) speed = 0.01f;
    if (speed > 10.0f) speed = 10.0f;
    lfo_phase_inc_ = 6.28318530f * speed / sample_rate_;
}

void HallReverb::SetTankModDepth(float depth) {
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 20.0f) depth = 20.0f;
    mod_depth_ = depth * 0.2f; // Scale to 0-4 samples range
}

void HallReverb::hadamardTransform(float* x) {
    float a, b;
    for (int i = 0; i < 8; i += 2) {
        a = x[i]; b = x[i+1];
        x[i] = a + b; x[i+1] = a - b;
    }
    for (int i = 0; i < 8; i += 4) {
        a = x[i]; b = x[i+2];
        x[i] = a + b; x[i+2] = a - b;
        a = x[i+1]; b = x[i+3];
        x[i+1] = a + b; x[i+3] = a - b;
    }
    for (int i = 0; i < 4; ++i) {
        a = x[i]; b = x[i+4];
        x[i] = a + b; x[i+4] = a - b;
    }
    for (int i = 0; i < 8; ++i)
        x[i] *= kHadamardNorm;
}

} // namespace flick
