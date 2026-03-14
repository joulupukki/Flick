#define _USE_MATH_DEFINES
#include <cmath>
#include "Biquad.h"

// Flick: use float pi to avoid double promotion
static constexpr float kPi = 3.14159265f;

namespace AudioLib
{
	Biquad::Biquad()
	{
		ClearBuffers();
	}

	Biquad::Biquad(FilterType filterType, float samplerate)
	{
		Type = filterType;
		SetSamplerate(samplerate);

		SetGainDb(0.0f); // Flick: double→float
		Frequency = samplerate / 4;
		SetQ(0.5f); // Flick: double→float
		ClearBuffers();
	}

	Biquad::~Biquad()
	{

	}


	float Biquad::GetSamplerate()
	{
		return samplerate;
	}

	void Biquad::SetSamplerate(float value)
	{
		samplerate = value;
		Update();
	}

	float Biquad::GetGainDb()
	{
		return log10f(gain) * 20.0f; // Flick: std::log10→log10f, double→float
	}

	void Biquad::SetGainDb(float value)
	{
		SetGain(powf(10.0f, value / 20.0f)); // Flick: std::pow→powf, double→float
	}

	float Biquad::GetGain()
	{
		return gain;
	}

	void Biquad::SetGain(float value)
	{
		if (value == 0)
			value = 0.001f; // -60dB // Flick: double→float

		gain = value;
	}

	float Biquad::GetQ()
	{
		return _q;
	}

	void Biquad::SetQ(float value)
	{
		if (value == 0)
			value = 1e-12f; // Flick: double→float
		_q = value;
	}

	vector<float> Biquad::GetA()
	{
		return vector<float>({ 1, a1, a2 });
	}

	vector<float> Biquad::GetB()
	{
		return vector<float>({ b0, b1, b2 });
	}


	void Biquad::Update()
	{
		float omega = 2.0f * kPi * Frequency / samplerate; // Flick: use kPi, double→float
		float sinOmega = sinf(omega); // Flick: std::sin→sinf
		float cosOmega = cosf(omega); // Flick: std::cos→cosf

		float sqrtGain = 0.0f; // Flick: double→float
		float alpha = 0.0f; // Flick: double→float

		if (Type == FilterType::LowShelf || Type == FilterType::HighShelf)
		{
			alpha = sinOmega / 2.0f * sqrtf((gain + 1.0f / gain) * (1.0f / Slope - 1.0f) + 2.0f); // Flick: std::sqrt→sqrtf, double→float
			sqrtGain = sqrtf(gain); // Flick: std::sqrt→sqrtf
		}
		else
		{
			alpha = sinOmega / (2.0f * _q); // Flick: double→float
		}

		switch (Type)
		{
		case FilterType::LowPass:
			b0 = (1.0f - cosOmega) / 2.0f; // Flick: double→float
			b1 = 1.0f - cosOmega;
			b2 = (1.0f - cosOmega) / 2.0f;
			a0 = 1.0f + alpha;
			a1 = -2.0f * cosOmega;
			a2 = 1.0f - alpha;
			break;
		case FilterType::HighPass:
			b0 = (1.0f + cosOmega) / 2.0f;
			b1 = -(1.0f + cosOmega);
			b2 = (1.0f + cosOmega) / 2.0f;
			a0 = 1.0f + alpha;
			a1 = -2.0f * cosOmega;
			a2 = 1.0f - alpha;
			break;
		case FilterType::BandPass:
			b0 = alpha;
			b1 = 0;
			b2 = -alpha;
			a0 = 1.0f + alpha;
			a1 = -2.0f * cosOmega;
			a2 = 1.0f - alpha;
			break;
		case FilterType::Notch:
			b0 = 1;
			b1 = -2.0f * cosOmega;
			b2 = 1;
			a0 = 1.0f + alpha;
			a1 = -2.0f * cosOmega;
			a2 = 1.0f - alpha;
			break;
		case FilterType::Peak:
			b0 = 1.0f + (alpha * gain);
			b1 = -2.0f * cosOmega;
			b2 = 1.0f - (alpha * gain);
			a0 = 1.0f + (alpha / gain);
			a1 = -2.0f * cosOmega;
			a2 = 1.0f - (alpha / gain);
			break;
		case FilterType::LowShelf:
			b0 = gain * ((gain + 1.0f) - (gain - 1.0f) * cosOmega + 2.0f * sqrtGain * alpha);
			b1 = 2.0f * gain * ((gain - 1.0f) - (gain + 1.0f) * cosOmega);
			b2 = gain * ((gain + 1.0f) - (gain - 1.0f) * cosOmega - 2.0f * sqrtGain * alpha);
			a0 = (gain + 1.0f) + (gain - 1.0f) * cosOmega + 2.0f * sqrtGain * alpha;
			a1 = -2.0f * ((gain - 1.0f) + (gain + 1.0f) * cosOmega);
			a2 = (gain + 1.0f) + (gain - 1.0f) * cosOmega - 2.0f * sqrtGain * alpha;
			break;
		case FilterType::HighShelf:
			b0 = gain * ((gain + 1.0f) + (gain - 1.0f) * cosOmega + 2.0f * sqrtGain * alpha);
			b1 = -2.0f * gain * ((gain - 1.0f) + (gain + 1.0f) * cosOmega);
			b2 = gain * ((gain + 1.0f) + (gain - 1.0f) * cosOmega - 2.0f * sqrtGain * alpha);
			a0 = (gain + 1.0f) - (gain - 1.0f) * cosOmega + 2.0f * sqrtGain * alpha;
			a1 = 2.0f * ((gain - 1.0f) - (gain + 1.0f) * cosOmega);
			a2 = (gain + 1.0f) - (gain - 1.0f) * cosOmega - 2.0f * sqrtGain * alpha;
			break;
		}

		float g = 1.0f / a0; // Flick: double→float

		b0 = b0 * g;
		b1 = b1 * g;
		b2 = b2 * g;
		a1 = a1 * g;
		a2 = a2 * g;
	}

	float Biquad::GetResponse(float freq)
	{
		// Flick: std::pow→powf, std::sin→sinf, double→float, M_PI→kPi
		float phi = powf((sinf(2.0f * kPi * freq / (2.0f * samplerate))), 2.0f);
		return (powf(b0 + b1 + b2, 2.0f) - 4.0f * (b0 * b1 + 4.0f * b0 * b2 + b1 * b2) * phi + 16.0f * b0 * b2 * phi * phi) / (powf(1.0f + a1 + a2, 2.0f) - 4.0f * (a1 + 4.0f * a2 + a1 * a2) * phi + 16.0f * a2 * phi * phi);
	}

	void Biquad::ClearBuffers()
	{
		y = 0;
		x2 = 0;
		y2 = 0;
		x1 = 0;
		y1 = 0;
	}

}
