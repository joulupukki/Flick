
#pragma once

// Flick: removed #include <vector>
#include "ModulatedAllpass.h"
#include "AudioLib/ShaRandom.h"

// Flick: removed using namespace std

namespace CloudSeed
{
	class AllpassDiffuser
	{
	public:
		static const int MaxStageCount = 2;

	private:
		int samplerate;

		ModulatedAllpass* filters[MaxStageCount]; // Flick: fixed-size array replaces vector
		int delay;
		float modRate;
		float seedValues[MaxStageCount * 3]; // Flick: fixed-size array replaces vector
		int seed;
		float crossSeed;

	public:
		int Stages;

		AllpassDiffuser(int samplerate, int delayBufferLengthMillis)
		{
			auto delayBufferSize = samplerate * ((float)delayBufferLengthMillis / 1000.0f); // Flick: 1000.0 → 1000.0f
			for (int i = 0; i < MaxStageCount; i++) // Flick: direct assignment replaces push_back
			{
				filters[i] = new ModulatedAllpass((int)delayBufferSize, 100);
			}

			crossSeed = 0.0f; // Flick: 0.0 → 0.0f
			seed = 23456;
			UpdateSeeds();
			Stages = 1;

			SetSamplerate(samplerate);
		}

		~AllpassDiffuser()
		{
			// Flick: index loop replaces range-based for; pool-allocated, not actually freed
			for (int i = 0; i < MaxStageCount; i++)
				delete filters[i];
		}

		int GetSamplerate()
		{
			return samplerate;
		}

		void SetSamplerate(int samplerate)
		{
			this->samplerate = samplerate;
			SetModRate(modRate);
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


		bool GetModulationEnabled()
		{
			return filters[0]->ModulationEnabled;
		}

		void SetModulationEnabled(bool value)
		{
			for (int i = 0; i < MaxStageCount; i++) // Flick: index loop replaces range-based for
				filters[i]->ModulationEnabled = value;
		}

		void SetInterpolationEnabled(bool enabled)
		{
			for (int i = 0; i < MaxStageCount; i++) // Flick: index loop replaces range-based for
				filters[i]->InterpolationEnabled = enabled;
		}

		float* GetOutput()
		{
			return filters[Stages - 1]->GetOutput();
		}


		void SetDelay(int delaySamples)
		{
			delay = delaySamples;
			Update();
		}

		void SetFeedback(float feedback)
		{
			for (int i = 0; i < MaxStageCount; i++) // Flick: index loop replaces range-based for
				filters[i]->Feedback = feedback;
		}

		void SetModAmount(float amount)
		{
			for (int i = 0; i < MaxStageCount; i++) // Flick: MaxStageCount replaces filters.size()
			{
				filters[i]->ModAmount = amount * (0.85f + 0.3f * seedValues[MaxStageCount + i]); // Flick: float literals
			}
		}

		void SetModRate(float rate)
		{
			modRate = rate;

			for (int i = 0; i < MaxStageCount; i++) // Flick: index loop replaces range-based for, MaxStageCount replaces filters.size()
				filters[i]->ModRate = rate * (0.85f + 0.3f * seedValues[MaxStageCount * 2 + i]) / samplerate; // Flick: float literals
		}

		void Process(float* input, int sampleCount)
		{
			filters[0]->Process(input, sampleCount); // Flick: direct array access replaces filterPtr

			for (int i = 1; i < Stages; i++)
			{
				filters[i]->Process(filters[i - 1]->GetOutput(), sampleCount);
			}
		}

		void ClearBuffers()
		{
			for (int i = 0; i < MaxStageCount; i++) // Flick: MaxStageCount replaces filters.size()
				filters[i]->ClearBuffers();
		}

	private:
		void Update()
		{
			for (int i = 0; i < MaxStageCount; i++) // Flick: MaxStageCount replaces filters.size()
			{
				auto r = seedValues[i];
				auto d = powf(10.0f, r) * 0.1f; // Flick: powf + float literals replace std::pow + double
				filters[i]->SampleDelay = (int)(delay * d);
			}
		}

		void UpdateSeeds()
		{
			AudioLib::ShaRandom::Generate(seed, MaxStageCount * 3, crossSeed, seedValues); // Flick: output-parameter overload replaces vector return
			Update();
		}

	};
}
