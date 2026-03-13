#pragma once

#include <cmath>
#include "AudioLib/MathDefs.h"

namespace CloudSeed
{
	class FastSin
	{
	private:
		static const int DataSize = 32768;
		static DSY_SDRAM_BSS float data[DataSize];

	public:
		static void ZeroBuffer(float* buffer, int len);
		static void Init()
		{
			for (int i = 0; i < DataSize; i++)
			{
				data[i] = sinf(2.0f * (float)M_PI * i / (float)DataSize); // Flick: std::sin→sinf, double→float
			}
		}

		static float Get(float index)
		{
			return data[(int)(index * 32767.99999f)]; // Flick: double→float
		}
	};
}

