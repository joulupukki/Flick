// Partitioned overlap-save convolution engine for real-time IR processing
// on STM32H750 (ARM Cortex-M7).
//
// Uses uniform partitioning with L=128, N=256. Proven clean at 64 partitions
// (~170ms IR). Feedback recirculation extends the perceived tail.
//
// Large buffers (IR FFTs, FDL) must be externally allocated in SDRAM.

#pragma once

#include <arm_math.h>
#include <cstring>

static constexpr size_t CONV_L = 128;
static constexpr size_t CONV_N = 2 * CONV_L;           // 256
static constexpr size_t CONV_MAX_PARTS = 64;            // 64 × 128 = 8192 samples (~170ms)
static constexpr size_t NUCONV_BLOCK_SIZE = CONV_L;     // 128

// SDRAM buffer sizes (in floats)
static constexpr size_t CONV_IRFREQ_SIZE = CONV_MAX_PARTS * CONV_N;  // 16384
static constexpr size_t CONV_FDL_SIZE    = CONV_MAX_PARTS * CONV_N;  // 16384

class NonUniformConvolutionEngine {
public:
    void Init(float* irFreqBuf, float* fdlBuf) {
        irFreq_ = irFreqBuf;
        fdl_ = fdlBuf;
        arm_rfft_fast_init_f32(&fftInst_, CONV_N);
        numParts_ = 0;
        Reset();
        prepared_ = false;
    }

    void Prepare(const float* ir, size_t irLength) {
        prepared_ = false;
        numParts_ = (irLength + CONV_L - 1) / CONV_L;
        if (numParts_ > CONV_MAX_PARTS) {
            numParts_ = CONV_MAX_PARTS;
        }

        for (size_t p = 0; p < numParts_; p++) {
            float padded[CONV_N];
            memset(padded, 0, sizeof(padded));
            size_t offset = p * CONV_L;
            size_t count = irLength - offset;
            if (count > CONV_L) count = CONV_L;
            memcpy(padded, ir + offset, count * sizeof(float));
            arm_rfft_fast_f32(&fftInst_, padded, irFreq_ + p * CONV_N, 0);
        }

        Reset();
        prepared_ = true;
    }

    void ProcessBlock(const float* in, float* out) {
        if (!prepared_ || numParts_ == 0) {
            memcpy(out, in, CONV_L * sizeof(float));
            return;
        }

        float inputBuf[CONV_N];
        memcpy(inputBuf, inputOverlap_, CONV_L * sizeof(float));
        memcpy(inputBuf + CONV_L, in, CONV_L * sizeof(float));
        memcpy(inputOverlap_, in, CONV_L * sizeof(float));

        float inputFreq[CONV_N];
        arm_rfft_fast_f32(&fftInst_, inputBuf, inputFreq, 0);
        memcpy(fdl_ + fdlIndex_ * CONV_N, inputFreq, CONV_N * sizeof(float));

        float accumFreq[CONV_N];
        memset(accumFreq, 0, sizeof(accumFreq));

        for (size_t p = 0; p < numParts_; p++) {
            size_t fdlIdx = (fdlIndex_ + numParts_ - p) % numParts_;
            float product[CONV_N];
            arm_cmplx_mult_cmplx_f32(
                fdl_ + fdlIdx * CONV_N,
                irFreq_ + p * CONV_N,
                product, CONV_N / 2);
            arm_add_f32(accumFreq, product, accumFreq, CONV_N);
        }

        float timeDomain[CONV_N];
        arm_rfft_fast_f32(&fftInst_, accumFreq, timeDomain, 1);
        memcpy(out, timeDomain + CONV_L, CONV_L * sizeof(float));
        fdlIndex_ = (fdlIndex_ + 1) % numParts_;
    }

    void Reset() {
        memset(inputOverlap_, 0, sizeof(inputOverlap_));
        if (fdl_) memset(fdl_, 0, CONV_MAX_PARTS * CONV_N * sizeof(float));
        fdlIndex_ = 0;
    }

private:
    arm_rfft_fast_instance_f32 fftInst_;
    size_t numParts_;
    size_t fdlIndex_;
    bool prepared_;
    float inputOverlap_[CONV_L];
    float* irFreq_;
    float* fdl_;
};
