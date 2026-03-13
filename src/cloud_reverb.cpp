// Flick for Funbox DIY DSP Platform
// Copyright (C) 2025-2026 Boyd Timothy <btimothy@gmail.com>
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

#include "cloud_reverb.h"
#include "CloudSeed/ReverbController.h"
#include "CloudSeed/FastSin.h"
#include "CloudSeed/AudioLib/ValueTables.h"
#include "CloudSeed/Parameter.h"

#include "daisy_seed.h"
#include <cstring>
#include <cmsis_gcc.h>  // for __get_FPSCR / __set_FPSCR

// ============================================================================
// SDRAM Memory Pool for CloudSeed delay buffers
// ============================================================================
// CloudSeed allocates delay line buffers via custom_pool_allocate() during
// construction. This pool is separate from the Dattorro plate reverb's sdramData.

#define CLOUD_POOL_SIZE (8 * 1024 * 1024)  // 8 MB
DSY_SDRAM_BSS char cloud_pool[CLOUD_POOL_SIZE];
static size_t cloud_pool_index = 0;

// This function is called by CloudSeed components (declared extern in their headers)
void* custom_pool_allocate(size_t size) {
    // Align to 4 bytes
    size = (size + 3) & ~3;
    if (cloud_pool_index + size >= CLOUD_POOL_SIZE) {
        return nullptr;
    }
    void* ptr = &cloud_pool[cloud_pool_index];
    cloud_pool_index += size;
    return ptr;
}

namespace flick {

void CloudReverb::Init(float sample_rate) {
    sample_rate_ = sample_rate;

    // Enable Flush-to-Zero (FTZ) and Default NaN (DN) modes on the FPU.
    // This prevents denormal floats (tiny values near zero) from causing
    // massive CPU slowdowns in the reverb feedback paths. Standard practice
    // for real-time audio on ARM Cortex-M7.
    uint32_t fpscr = __get_FPSCR();
    fpscr |= (1 << 24) | (1 << 25);  // FZ bit (24) + DN bit (25)
    __set_FPSCR(fpscr);

    // Zero the SDRAM pool before use
    memset(cloud_pool, 0, CLOUD_POOL_SIZE);
    cloud_pool_index = 0;

    // Initialize CloudSeed lookup tables (must be done before constructing controller)
    AudioLib::ValueTables::Init();
    CloudSeed::FastSin::Init();

    // Construct the reverb controller using placement new in the pool
    // (ReverbController itself is small, but its ReverbChannels allocate from the pool)
    controller_ = new CloudSeed::ReverbController((int)sample_rate);
    controller_->ClearBuffers();

    // Default to Rubi-Ka Fields (ambient, spacious)
    controller_->SetPreset(5);
    applyFlickOverrides();
}

void CloudReverb::ProcessSample(float in_L, float in_R,
                                 float* out_L, float* out_R) {
    // At the start of each 8-sample block, process the PREVIOUS block's buffered inputs.
    // All 8 outputs come from the same processing pass — no discontinuity.
    // Cost: 8 samples (167μs) latency, imperceptible.
    if (buf_pos_ == 0) {
        controller_->Process(in_buf_L_, in_buf_R_, out_buf_L_, out_buf_R_, kBlockSize);
    }

    // Read this sample's output (all from the same processing pass)
    *out_L = out_buf_L_[buf_pos_];
    *out_R = out_buf_R_[buf_pos_];

    // Buffer the current input for processing at the start of the NEXT block
    in_buf_L_[buf_pos_] = in_L;
    in_buf_R_[buf_pos_] = in_R;

    buf_pos_ = (buf_pos_ + 1) & (kBlockSize - 1);
}

void CloudReverb::Clear() {
    if (controller_) {
        controller_->ClearBuffers();
    }
}

void CloudReverb::SetDecay(float decay) {
    if (controller_) {
        controller_->SetParameter(Parameter::LineDecay, decay);
    }
}

void CloudReverb::SetDiffusion(float diffusion) {
    if (controller_) {
        controller_->SetParameter(Parameter::LateDiffusionFeedback, diffusion);
    }
}

void CloudReverb::SetPreDelay(float pre_delay) {
    if (controller_) {
        controller_->SetParameter(Parameter::PreDelay, pre_delay);
    }
}

void CloudReverb::SetPreset(int preset_index) {
    if (controller_) {
        controller_->ClearBuffers();
        controller_->SetPreset(preset_index);
        applyFlickOverrides();
    }
}

void CloudReverb::applyFlickOverrides() {
    // Flick's orchestrator handles dry/wet mixing externally via Knob 1 (reverb.wet).
    // CloudSeed's output formula is: dry*input + predelay*preOut + early*earlyOut + main*lateOut
    // We want ProcessSample to return ONLY the wet signal, so set DryOut=0.
    // The EarlyOut and MainOut from the preset control the wet signal's character.
    controller_->SetParameter(Parameter::DryOut, 0.0f);
    controller_->SetParameter(Parameter::PredelayOut, 0.0f);

    // Force line count to match our TotalLineCount (1 per channel)
    // Presets have LineCount commented out, so this ensures it's set correctly.
    controller_->SetParameter(Parameter::LineCount, 1.0f);
}

}  // namespace flick
