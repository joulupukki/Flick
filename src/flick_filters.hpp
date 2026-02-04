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