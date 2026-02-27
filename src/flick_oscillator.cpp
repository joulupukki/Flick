#include "flick_oscillator.h"

#include <cmath>

#define PI_F 3.1415927410125732421875f
#define TWOPI_F (2.0f * PI_F)

using namespace flick;

float FlickOscillator::Process() {
  float out, delta, scale;
  switch (waveform_) {
    case WAVE_SIN:
      out = sinf(phase_ * TWOPI_F);
      break;
    case WAVE_SQUARE_ROUNDED:
      // Modified from the Hothouse's HarmonicTremVerb so that you don't hear
      // as much dry sound and it's more square.
      delta = 0.04f;
      scale = amp_ * atanf(1.0f / delta);
      out = (amp_ / scale) * atanf(sinf(TWOPI_F * phase_) / delta);
      break;
    default:
      out = 0.0f;
      break;
  }
  phase_ += phase_inc_;
  if (phase_ > 1.0f) {
    phase_ -= 1.0f;
    eoc_ = true;
  } else {
    eoc_ = false;
  }
  eor_ = (phase_ - phase_inc_ < 0.5f && phase_ >= 0.5f);

  return out * amp_;
}

float FlickOscillator::CalcPhaseInc(float f) { return f * sr_recip_; }
