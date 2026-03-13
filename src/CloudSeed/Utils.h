#ifndef UTILS
#define UTILS

#include <cstring>
#include <algorithm>
#include <cmath>

namespace CloudSeed
{
	class Utils
	{
	public:

		static inline void ZeroBuffer(float* buffer, int len)
		{
			for (int i = 0; i < len; i++)
				buffer[i] = 0.0f; // Flick: double→float
		}

		static inline void Copy(float* source, float* dest, int len)
		{
			std::memcpy(dest, source, len * sizeof(float));
		}

		static inline void Gain(float* buffer, float gain, int len)
		{
			for (int i = 0; i < len; i++)
			{
				buffer[i] *= gain;
			}
		}

		// perform bit crushing and undersampling
		// undersampling: if set to 1, perfroms no effect, if set to 2, will undersample to 1/2 samplerate, etc...
		// sampleResolution: if set to 32, will use 2^32 steps, if set to 8, will resude to 2^8 = 256 steps
		// Currently Unused
		static inline void BitcrushAndReduce(float* bufferIn, float* bufferOut, int len, int undersampling, int sampleResolution)
		{
			float sampleSteps = powf(2.0f, sampleResolution); // Flick: std::pow→powf, double→float
			float inverseSteps = 1.0f / sampleSteps; // Flick: double→float

			float sample = 0.0f; // Flick: double→float

			for (int i = 0; i < len; i++)
			{
				if (i % undersampling == 0)
					sample = ((long)(bufferIn[i] * sampleSteps)) * inverseSteps;

				bufferOut[i] = sample;
			}
		}

		template<typename T>
		static float DB2gain(T input)
		{
			return powf(10.0f, input / 20.0f); // Flick: std::pow→powf, double→float
		}

		template<typename T>
		static float Gain2DB(T input)
		{
			if (input < 0.0000001f) // Flick: double→float
				return -100000;

			return 20.0f * log10f(input); // Flick: std::log10→log10f
		}
	};
}

#endif
