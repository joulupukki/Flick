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

#ifndef FLICK_FILTERS_HPP
#define FLICK_FILTERS_HPP

#include <math.h>

struct LowPassFilter {
    float alpha;
    float prev_y = 0.0f;

    void Init(float fc, float fs) {
        alpha = expf(-2.0f * M_PI * fc / fs);
    }

    inline float Process(float x) {
        float y = (1.0f - alpha) * x + alpha * prev_y;
        prev_y = y;
        return y;
    }
};

struct HighPassFilter {
    float alpha;
    float prev_x = 0.0f;
    float prev_y = 0.0f;

    void Init(float fc, float fs) {
        alpha = expf(-2.0f * M_PI * fc / fs);
    }

    inline float Process(float x) {
        float y = (1.0f + alpha) * 0.5f * (x - prev_x) + alpha * prev_y;
        prev_x = x;
        prev_y = y;
        return y;
    }
};

struct PeakingEQ {
    float b0, b1, b2, a1, a2;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

    void Init(float f0, float gain_db, float Q, float fs) {
        float A = powf(10.0f, gain_db / 40.0f);
        float omega = 2.0f * M_PI * f0 / fs;
        float alpha = sinf(omega) / (2.0f * Q);
        float cos_omega = cosf(omega);

        b0 = 1.0f + alpha * A;
        b1 = -2.0f * cos_omega;
        b2 = 1.0f - alpha * A;
        float a0 = 1.0f + alpha / A;
        a1 = -2.0f * cos_omega;
        a2 = 1.0f - alpha / A;

        // Normalize by a0
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
    }

    inline float Process(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};

// Generic Direct-Form-I biquad with externally supplied (already a0-normalized)
// coefficients. Used to build the anti-aliasing / reconstruction filters for
// the 96kHz<->48kHz decimation stage. Same difference equation as PeakingEQ.
struct Biquad {
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

    void SetCoeffs(float nb0, float nb1, float nb2, float na1, float na2) {
        b0 = nb0; b1 = nb1; b2 = nb2; a1 = na1; a2 = na2;
    }
    void Reset() { x1 = x2 = y1 = y2 = 0.0f; }

    inline float Process(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

// 6th-order elliptic low-pass at 14kHz for Fs = 96kHz (three cascaded biquads).
//
// Applied to every raw 96kHz input sample BEFORE the 2:1 decimation to 48kHz.
// The codec's ADC passes content up to ~45kHz, so naive decimation (dropping
// every other sample) folds everything in 24-48kHz down into the audible band
// (aliasing). High-frequency interference (DMA/clock/switching harmonics picked
// up at the high-impedance guitar input) is the dominant source. This filter
// removes that energy first so it cannot alias.
//
// The 14kHz cutoff also attenuates residual input-side EMI just below Nyquist
// (a ~17kHz spike picked up by the guitar acting as an antenna) that a higher
// cutoff let through. A guitar has no useful content above ~12kHz, so the
// passband (flat to 14kHz) leaves the tone fully intact.
//
// Response: flat to ~14kHz (-0.3dB), -26dB @17k, -35dB @18k, -73dB @24k.
// Stable in float32 DF1 (verified: impulse decays, outputs bounded).
// Coefficients: scipy.signal.ellip(6, 0.3, 70, 14000, fs=96000, output='sos').
struct AntiAliasLowpass {
    Biquad s0, s1, s2;
    void Init() {
        s0.SetCoeffs(0.0043185288f, 0.0067808520f, 0.0043185288f,
                     -1.3314503549f, 0.4908691622f);
        s1.SetCoeffs(1.0000000000f, 0.2017033154f, 1.0000000000f,
                     -1.1994078446f, 0.6797048071f);
        s2.SetCoeffs(1.0000000000f, -0.3275247389f, 1.0000000000f,
                     -1.1280097703f, 0.8955384302f);
        s0.Reset();
        s1.Reset();
        s2.Reset();
    }
    inline float Process(float x) { return s2.Process(s1.Process(s0.Process(x))); }
};

struct LowShelf {
    float b0, b1, b2, a1, a2;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

    void Init(float f0, float gain_db, float Q, float fs) {
        float A = powf(10.0f, gain_db / 40.0f);
        float omega = 2.0f * M_PI * f0 / fs;
        float alpha = sinf(omega) / (2.0f * Q);
        float cos_omega = cosf(omega);
        float sqrt_A = sqrtf(A);

        b0 = A * ((A + 1.0f) - (A - 1.0f) * cos_omega + 2.0f * sqrt_A * alpha);
        b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_omega);
        b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_omega - 2.0f * sqrt_A * alpha);
        float a0 = (A + 1.0f) + (A - 1.0f) * cos_omega + 2.0f * sqrt_A * alpha;
        a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_omega);
        a2 = (A + 1.0f) + (A - 1.0f) * cos_omega - 2.0f * sqrt_A * alpha;

        // Normalize by a0
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
    }

    inline float Process(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};

#endif  // FLICK_FILTERS_HPP