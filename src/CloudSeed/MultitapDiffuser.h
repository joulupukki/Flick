#ifndef MULTITAPDIFFUSER
#define MULTITAPDIFFUSER

// Flick: removed <vector> and <memory>
#include <array>
#include <cstring> // Flick: for memcpy
#include "MultitapDiffuser.h"
#include "Utils.h"
#include "AudioLib/ShaRandom.h"
extern void* custom_pool_allocate(size_t size);

namespace CloudSeed
{
	// Flick: removed using namespace std;

	class MultitapDiffuser
	{
	public:
		static const int MaxTaps = 50;

	private:
		float* buffer;
		float* output;
		int len;

		int index;
		float tapGains[MaxTaps]; // Flick: was vector<float>
		int tapPosition[MaxTaps]; // Flick: was vector<int>
		float seedValues[MaxTaps * 2]; // Flick: was vector<float>; Generate called with count=100, MaxTaps=50
		int seed;
		float crossSeed;
		int count;
		float length;
		float gain;
		float decay;

		bool isDirty;
		float tapGainsTemp[MaxTaps]; // Flick: was vector<float>
		int tapPositionTemp[MaxTaps]; // Flick: was vector<int>
		int countTemp;

	public:
		MultitapDiffuser(int delayBufferSize)
		{
			len = delayBufferSize;
			buffer = new (custom_pool_allocate(sizeof(float) * delayBufferSize)) float[delayBufferSize];
			output = new (custom_pool_allocate(sizeof(float) * delayBufferSize)) float[delayBufferSize];
			index = 0;
			count = 1;
			length = 1;
			gain = 1.0f; // Flick: was 1.0
			decay = 0.0f; // Flick: was 0.0
			crossSeed = 0.0f; // Flick: was 0.0
			UpdateSeeds();
		}

		~MultitapDiffuser()
		{
			delete buffer;
			delete output;
		}


		void SetSeed(int seed)
		{
			this->seed = seed;
			UpdateSeeds();
		}

		void SetCrossSeed(float crossSeed)
		{
			this->crossSeed = crossSeed;
			UpdateSeeds();
		}

		float* GetOutput()
		{
			return output;
		}

		void SetTapCount(int tapCount)
		{
			count = tapCount;
			Update();
		}

		void SetTapLength(int tapLength)
		{
			length = tapLength;
			Update();
		}

		void SetTapDecay(float tapDecay)
		{
			decay = tapDecay;
			Update();
		}

		void SetTapGain(float tapGain)
		{
			gain = tapGain;
			Update();
		}

		void Process(float* input, int sampleCount)
		{
			// prevents race condition when parameters are updated from Gui
			if (isDirty)
			{
				// Flick: was vector assignment
				memcpy(tapGainsTemp, tapGains, MaxTaps * sizeof(float));
				memcpy(tapPositionTemp, tapPosition, MaxTaps * sizeof(int));
				countTemp = count;
				isDirty = false;
			}

			int* const tapPos = &tapPositionTemp[0];
			float* const tapGain = &tapGainsTemp[0];
			const int cnt = countTemp;

			for (int i = 0; i < sampleCount; i++)
			{
				if (index < 0) index += len;
				buffer[index] = input[i];
				output[i] = 0.0f; // Flick: was 0.0

				for (int j = 0; j < cnt; j++)
				{
					auto idx = (index + tapPos[j]) % len;
					output[i] += buffer[idx] * tapGain[j];
				}

				index--;
			}
		}

		void ClearBuffers()
		{
			Utils::ZeroBuffer(buffer, len);
			Utils::ZeroBuffer(output, len);
		}


	private:
		void Update()
		{
			// Flick: was vector<float> and vector<int>
			float newTapGains[MaxTaps];
			int newTapPosition[MaxTaps];

			int s = 0;
			auto rand = [&]() {return seedValues[s++]; };

			if (count < 1)
				count = 1;

			if (length < count)
				length = count;

			// used to adjust the volume of the overall output as it grows when we add more taps
			float tapCountFactor = 1.0f / (1.0f + sqrtf((float)count / MaxTaps)); // Flick: was 1.0, std::sqrt

			// Flick: removed newTapGains.resize(count) and newTapPosition.resize(count)

			// Flick: was vector<float> tapData(count, 0.0)
			float tapData[MaxTaps];
			for (int i = 0; i < count; i++) tapData[i] = 0.0f;

			float sumLengths = 0.0f; // Flick: was auto = 0.0
			for (int i = 0; i < count; i++) // Flick: was size_t
			{
				auto val = 0.1f + rand(); // Flick: was 0.1
				tapData[i] = val;
				sumLengths += val;
			}

			auto scaleLength = length / sumLengths;
			newTapPosition[0] = 0;

			for (int i = 1; i < count; i++)
			{
				newTapPosition[i] = newTapPosition[i - 1] + (int)(tapData[i] * scaleLength);
			}

			float sumGains = 0.0f; // Flick: was 0.0
			float lastTapPos = newTapPosition[count - 1];
			for (int i = 0; i < count; i++)
			{
				// when decay set to 0, there is no decay, when set to 1, the gain at the last sample is 0.01 = -40dB
				auto g = powf(10.0f, -decay * 2.0f * newTapPosition[i] / (lastTapPos + 1.0f)); // Flick: was std::pow with int/double args

				auto tap = (2.0f * rand() - 1.0f) * tapCountFactor; // Flick: was 2, 1
				newTapGains[i] = tap * g * gain;
			}

			// Set the tap vs. clean mix
			newTapGains[0] = (1.0f - gain); // Flick: was 1

			// Flick: was vector assignment
			memcpy(tapGains, newTapGains, count * sizeof(float));
			memcpy(tapPosition, newTapPosition, count * sizeof(int));
			isDirty = true;
		}

		void UpdateSeeds()
		{
			// Flick: was this->seedValues = AudioLib::ShaRandom::Generate(seed, 100, crossSeed)
			AudioLib::ShaRandom::Generate(seed, 100, crossSeed, seedValues);
			Update();
		}
	};
}

#endif
