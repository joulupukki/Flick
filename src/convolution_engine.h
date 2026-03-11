// Partitioned overlap-save convolution engine for real-time IR processing.
//
// Uses FFT-based convolution (CMSIS-DSP) to efficiently apply long impulse
// responses. Direct FIR convolution of 8160 taps at 48kHz exceeds the
// STM32H750's CPU budget; this approach reduces cost from ~122% to ~4-10%.
//
// Algorithm: uniform partitioned overlap-save
//   - IR is split into P partitions of L samples
//   - Each partition is pre-FFT'd (size 2L) at load time
//   - At runtime, each audio block is FFT'd and multiplied with all
//     IR partitions via a frequency-domain delay line (FDL)
//
// Memory: large buffers (IR FFTs, FDL) must be allocated externally in SDRAM
// and passed to Init().

#pragma once

#include <arm_math.h>
#include <cstring>

// Partition size L and FFT size N are compile-time constants.
// L must be a power of 2 (required by arm_rfft_fast).
static constexpr size_t CONV_PARTITION_SIZE = 128;
static constexpr size_t CONV_FFT_SIZE = 2 * CONV_PARTITION_SIZE;  // 256

class ConvolutionEngine {
public:
    static constexpr size_t L = CONV_PARTITION_SIZE;
    static constexpr size_t N = CONV_FFT_SIZE;

    // maxPartitions: max number of IR partitions supported
    // irFreqBuf: external buffer of at least maxPartitions * N floats (SDRAM)
    // fdlBuf: external buffer of at least maxPartitions * N floats (SDRAM)
    void Init(size_t maxPartitions, float* irFreqBuf, float* fdlBuf) {
        maxPartitions_ = maxPartitions;
        irFreq_ = irFreqBuf;
        fdl_ = fdlBuf;

        arm_rfft_fast_init_f32(&fftInst_, N);
        Reset();
        numPartitions_ = 0;
        prepared_ = false;
    }

    // Pre-compute FFTs of IR partitions. Call from main thread, not audio callback.
    void Prepare(const float* ir, size_t irLength) {
        prepared_ = false;
        numPartitions_ = (irLength + L - 1) / L;
        if (numPartitions_ > maxPartitions_) {
            numPartitions_ = maxPartitions_;
        }

        for (size_t p = 0; p < numPartitions_; p++) {
            float padded[N];
            memset(padded, 0, sizeof(padded));

            size_t offset = p * L;
            size_t count = irLength - offset;
            if (count > L) count = L;
            memcpy(padded, ir + offset, count * sizeof(float));

            arm_rfft_fast_f32(&fftInst_, padded, irFreqAt(p), 0);
        }

        Reset();
        prepared_ = true;
    }

    // Process one audio block. blockSize must equal L (128).
    void ProcessBlock(const float* in, float* out, size_t blockSize) {
        if (!prepared_ || numPartitions_ == 0) {
            memcpy(out, in, blockSize * sizeof(float));
            return;
        }

        // Form 2L input: [overlap from previous block | current block]
        float inputBuf[N];
        memcpy(inputBuf, inputOverlap_, L * sizeof(float));
        memcpy(inputBuf + L, in, L * sizeof(float));
        memcpy(inputOverlap_, in, L * sizeof(float));

        // Forward FFT of input
        float inputFreq[N];
        arm_rfft_fast_f32(&fftInst_, inputBuf, inputFreq, 0);

        // Store in FDL at current position
        memcpy(fdlAt(fdlIndex_), inputFreq, N * sizeof(float));

        // Accumulate: Y = sum( FDL[(cur-k) % P] * H[k] )
        float accumFreq[N];
        memset(accumFreq, 0, sizeof(accumFreq));

        for (size_t p = 0; p < numPartitions_; p++) {
            size_t fdlIdx = (fdlIndex_ + numPartitions_ - p) % numPartitions_;

            float product[N];
            arm_cmplx_mult_cmplx_f32(fdlAt(fdlIdx), irFreqAt(p), product, N / 2);
            arm_add_f32(accumFreq, product, accumFreq, N);
        }

        // Inverse FFT
        float timeDomain[N];
        arm_rfft_fast_f32(&fftInst_, accumFreq, timeDomain, 1);

        // Overlap-save: last L samples are the valid linear convolution output
        memcpy(out, timeDomain + L, L * sizeof(float));

        fdlIndex_ = (fdlIndex_ + 1) % numPartitions_;
    }

    void Reset() {
        memset(inputOverlap_, 0, sizeof(inputOverlap_));
        if (fdl_) {
            memset(fdl_, 0, maxPartitions_ * N * sizeof(float));
        }
        fdlIndex_ = 0;
    }

private:
    float* irFreqAt(size_t partition) { return irFreq_ + partition * N; }
    float* fdlAt(size_t partition) { return fdl_ + partition * N; }

    arm_rfft_fast_instance_f32 fftInst_;
    size_t maxPartitions_;
    size_t numPartitions_;
    size_t fdlIndex_;
    bool prepared_;

    float inputOverlap_[L];

    // Pointers to externally-allocated SDRAM buffers
    float* irFreq_;   // [maxPartitions * N] pre-computed IR FFTs
    float* fdl_;      // [maxPartitions * N] frequency-domain delay line
};
