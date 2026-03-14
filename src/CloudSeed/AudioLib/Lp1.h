#pragma once

#include <cmath>

// Flick: use float pi constant instead of double macro
static constexpr float kLp1Pi = 3.14159265f;

namespace AudioLib
{
	using namespace std;

	class Lp1
	{
	public:
		float Output;

	public:
		Lp1(float fs)
		{
			this->fs = fs;
		}

		float GetSamplerate()
		{
			return fs;
		}

		void SetSamplerate(float samplerate)
		{
			fs = samplerate;
		}

		float GetCutoffHz()
		{
			return cutoffHz;
		}

		void SetCutoffHz(float hz)
		{
			cutoffHz = hz;
			Update();
		}

		void Update()
		{
			// Prevent going over the Nyquist frequency
			if (cutoffHz >= fs * 0.5f) // Flick: double→float
				cutoffHz = fs * 0.499f; // Flick: double→float

			auto x = 2.0f * kLp1Pi * cutoffHz / fs; // Flick: use kLp1Pi, double→float
			auto nn = (2.0f - cosf(x)); // Flick: cos→cosf, double→float
			auto alpha = nn - sqrtf(nn * nn - 1.0f); // Flick: sqrt→sqrtf, double→float

			a1 = alpha;
			b0 = 1.0f - alpha; // Flick: double→float
		}

		float Process(float input)
		{
			if (input == 0 && Output < 0.000000000001f) // Flick: double→float
			{
				Output = 0;
			}
			else
			{
				Output = b0 * input + a1 * Output;
			}
			return Output;
		}

		void Process(float* input, float* output, int len)
		{
			for (int i = 0; i < len; i++)
				output[i] = Process(input[i]);
		}


	private:
		float fs;
		float b0, a1;
		float cutoffHz;
	};
}

