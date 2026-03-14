
#ifndef REVERBCHANNEL
#define REVERBCHANNEL

// Flick: Removed #include <map>
// Flick: Removed #include <memory>
#include "Parameter.h"
#include "ModulatedDelay.h"
#include "MultitapDiffuser.h"
#include "AudioLib/ShaRandom.h"
#include "AudioLib/Lp1.h"
#include "AudioLib/Hp1.h"
#include "DelayLine.h"
#include "AllpassDiffuser.h"

#include <cmath>
// Flick: Removed spurious self-include of "ReverbChannel.h"
#include "AudioLib/ShaRandom.h"
#include "Utils.h"

extern void* custom_pool_allocate(size_t size);

// Flick: Removed 'using namespace std'

namespace CloudSeed
{
	enum class ChannelLR
	{
		Left,
		Right
	};

	class ReverbChannel
	{
	private:
		// Flick: 2 lines/channel × 2 channels = 4 total (mono DaisyCloudSeed runs 5).
		// Single-instance architecture (Phase 2) keeps SDRAM within 8MB pool.
		static const int TotalLineCount = 2;

		float parameters[(int)Parameter::Count]; // Flick: Was map<Parameter, float>
		int samplerate;
		int bufferSize;

		ModulatedDelay preDelay;
		MultitapDiffuser multitap;
		AllpassDiffuser diffuser;
		DelayLine* lines[TotalLineCount]; // Flick: Was vector<DelayLine*>
		AudioLib::ShaRandom rand;
		AudioLib::Hp1 highPass;
		AudioLib::Lp1 lowPass;
		float* tempBuffer;
		float* lineOutBuffer;
		float* outBuffer;
		int delayLineSeed;
		int postDiffusionSeed;

		// Used the the main process loop
		int lineCount;

		bool highPassEnabled;
		bool lowPassEnabled;
		bool diffuserEnabled;
		float dryOut;
		float predelayOut;
		float earlyOut;
		float lineOut;
		float crossSeed;
		ChannelLR channelLr;

	public:

		ReverbChannel(int bufferSize, int samplerate, ChannelLR leftOrRight)
			: preDelay(bufferSize, (int)(samplerate * 1.0f), 100) // Flick: 1.0 → 1.0f
			, multitap(samplerate) // use samplerate = 1 second delay buffer
			, highPass(samplerate)
			, lowPass(samplerate)
			, diffuser(samplerate, 150) // 150ms buffer, to allow for 100ms + modulation time
		{
			this->channelLr = leftOrRight;

			for (int i = 0; i < TotalLineCount; i++) // Flick: Was lines.push_back(...)
				lines[i] = new (custom_pool_allocate(sizeof(DelayLine)))DelayLine(bufferSize, samplerate);

			this->bufferSize = bufferSize;

			for (auto value = 0; value < (int)Parameter::Count; value++)
				this->parameters[value] = 0.0f; // Flick: Was parameters[static_cast<Parameter>(value)] = 0.0

			crossSeed = 0.0f; // Flick: 0.0 → 0.0f
			lineCount = TotalLineCount;
			diffuser.SetInterpolationEnabled(true);
			highPass.SetCutoffHz(20);
			lowPass.SetCutoffHz(20000);

			tempBuffer = new  (custom_pool_allocate(sizeof(float)*bufferSize))float[bufferSize];
			lineOutBuffer = new (custom_pool_allocate(sizeof(float) * bufferSize))float[bufferSize];
			outBuffer = new (custom_pool_allocate(sizeof(float) * bufferSize))float[bufferSize];

			this->samplerate = samplerate;
		}

		~ReverbChannel()
		{
			// Flick: Was range-based for (auto line : lines)
			for (int i = 0; i < TotalLineCount; i++)
				delete lines[i];

			delete tempBuffer;
			delete lineOutBuffer;
			delete outBuffer;
		}

		int GetSamplerate()
		{
			return samplerate;
		}

		void SetSamplerate(int samplerate)
		{
			this->samplerate = samplerate;
			highPass.SetSamplerate(samplerate);
			lowPass.SetSamplerate(samplerate);

			for (int i = 0; i < TotalLineCount; i++) // Flick: Was lines.size()
			{
				lines[i]->SetSamplerate(samplerate);
			}

			// Flick: Was lambda capturing parameters[p] with map lookup
			auto update = [&](Parameter p) { SetParameter(p, parameters[(int)p]); };
			update(Parameter::PreDelay);
			update(Parameter::TapLength);
			update(Parameter::DiffusionDelay);
			update(Parameter::LineDelay);
			update(Parameter::LateDiffusionDelay);
			update(Parameter::EarlyDiffusionModRate);
			update(Parameter::LineModRate);
			update(Parameter::LateDiffusionModRate);
			update(Parameter::LineModAmount);
			UpdateLines();
		}

		float* GetOutput()
		{
			return outBuffer;
		}

		float* GetLineOutput()
		{
			return lineOutBuffer;
		}

		void SetParameter(Parameter para, float value)
		{
			parameters[(int)para] = value; // Flick: Was parameters[para]

			switch (para)
			{
			case Parameter::PreDelay:
				preDelay.SampleDelay = (int)Ms2Samples(value);
				break;
			case Parameter::HighPass:
				highPass.SetCutoffHz(value);
				break;
			case Parameter::LowPass:
				lowPass.SetCutoffHz(value);
				break;

			case Parameter::TapCount:
				multitap.SetTapCount((int)value);
				break;
			case Parameter::TapLength:
				multitap.SetTapLength((int)Ms2Samples(value));
				break;
			case Parameter::TapGain:
				multitap.SetTapGain(value);
				break;
			case Parameter::TapDecay:
				multitap.SetTapDecay(value);
				break;

			case Parameter::DiffusionEnabled:
			{
				auto newVal = value >= 0.5f; // Flick: 0.5 → 0.5f
				if (newVal != diffuserEnabled)
					diffuser.ClearBuffers();
				diffuserEnabled = newVal;
				break;
			}
			case Parameter::DiffusionStages:
				diffuser.Stages = (int)value;
				break;
			case Parameter::DiffusionDelay:
				diffuser.SetDelay((int)Ms2Samples(value));
				break;
			case Parameter::DiffusionFeedback:
				diffuser.SetFeedback(value);
				break;

			case Parameter::LineCount:
				lineCount = (int)value; // Originally commented out
                //perLineGain = GetPerLineGain();  // In original Cloud Seed
				break;
			case Parameter::LineDelay:
				UpdateLines();
				break;
			case Parameter::LineDecay:
				UpdateLines();
				break;

			case Parameter::LateDiffusionEnabled:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
				{
					auto newVal = value >= 0.5f; // Flick: 0.5 → 0.5f
					if (newVal != lines[i]->DiffuserEnabled)
						lines[i]->ClearDiffuserBuffer();
					lines[i]->DiffuserEnabled = newVal;
				}
				break;
			case Parameter::LateDiffusionStages:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->SetDiffuserStages((int)value);
				break;
			case Parameter::LateDiffusionDelay:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->SetDiffuserDelay((int)Ms2Samples(value));
				break;
			case Parameter::LateDiffusionFeedback:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->SetDiffuserFeedback(value);
				break;

			case Parameter::PostLowShelfGain:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->SetLowShelfGain(value);
				break;
			case Parameter::PostLowShelfFrequency:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->SetLowShelfFrequency(value);
				break;
			case Parameter::PostHighShelfGain:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->SetHighShelfGain(value);
				break;
			case Parameter::PostHighShelfFrequency:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->SetHighShelfFrequency(value);
				break;
			case Parameter::PostCutoffFrequency:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->SetCutoffFrequency(value);
				break;

			case Parameter::EarlyDiffusionModAmount:
				diffuser.SetModulationEnabled(value > 0.0f); // Flick: 0.0 → 0.0f
				diffuser.SetModAmount(Ms2Samples(value));
				break;
			case Parameter::EarlyDiffusionModRate:
				diffuser.SetModRate(value);
				break;
			case Parameter::LineModAmount:
				UpdateLines();
				break;
			case Parameter::LineModRate:
				UpdateLines();
				break;
			case Parameter::LateDiffusionModAmount:
				UpdateLines();
				break;
			case Parameter::LateDiffusionModRate:
				UpdateLines();
				break;

			case Parameter::TapSeed:
				multitap.SetSeed((int)value);
				break;
			case Parameter::DiffusionSeed:
				diffuser.SetSeed((int)value);
				break;
			case Parameter::DelaySeed:
				delayLineSeed = (int)value;
				UpdateLines();
				break;
			case Parameter::PostDiffusionSeed:
				postDiffusionSeed = (int)value;
				UpdatePostDiffusion();
				break;

			case Parameter::CrossSeed:

				crossSeed = channelLr == ChannelLR::Right ? value : 0;
				multitap.SetCrossSeed(value);
				diffuser.SetCrossSeed(value);
				UpdateLines();
				UpdatePostDiffusion();
				break;

			case Parameter::DryOut:
				dryOut = value;
				break;
			case Parameter::PredelayOut:
				predelayOut = value;
				break;
			case Parameter::EarlyOut:
				earlyOut = value;
				break;
			case Parameter::MainOut:
				lineOut = value;
				break;

			case Parameter::HiPassEnabled:
				highPassEnabled = value >= 0.5f; // Flick: 0.5 → 0.5f
				break;
			case Parameter::LowPassEnabled:
				lowPassEnabled = value >= 0.5f; // Flick: 0.5 → 0.5f
				break;
			case Parameter::LowShelfEnabled:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->LowShelfEnabled = value >= 0.5f; // Flick: 0.5 → 0.5f
				break;
			case Parameter::HighShelfEnabled:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->HighShelfEnabled = value >= 0.5f; // Flick: 0.5 → 0.5f
				break;
			case Parameter::CutoffEnabled:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->CutoffEnabled = value >= 0.5f; // Flick: 0.5 → 0.5f
				break;
			case Parameter::LateStageTap:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->LateStageTap = value >= 0.5f; // Flick: 0.5 → 0.5f
				break;

			case Parameter::Interpolation:
				for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
					lines[i]->SetInterpolationEnabled(value >= 0.5f); // Flick: 0.5 → 0.5f
				break;
			}
		}

		void Process(float* input, int sampleCount)
		{
			int len = sampleCount;
			auto predelayOutput = preDelay.GetOutput();
			auto lowPassInput = highPassEnabled ? tempBuffer : input;

			if (highPassEnabled)
				highPass.Process(input, tempBuffer, len);
			if (lowPassEnabled)
				lowPass.Process(lowPassInput, tempBuffer, len);
			if (!lowPassEnabled && !highPassEnabled)
				Utils::Copy(input, tempBuffer, len);

			// completely zero if no input present
			// Previously, the very small values were causing some really strange CPU spikes
			for (int i = 0; i < len; i++)
			{
				auto n = tempBuffer[i];
				if (n * n < 0.000000001f) // Flick: double → float literal
					tempBuffer[i] = 0;
			}

			preDelay.Process(tempBuffer, len);
			multitap.Process(preDelay.GetOutput(), len);

			auto earlyOutStage = diffuserEnabled ? diffuser.GetOutput() : multitap.GetOutput();

			if (diffuserEnabled)
			{
				diffuser.Process(multitap.GetOutput(), len);
				Utils::Copy(diffuser.GetOutput(), tempBuffer, len);
			}
			else
			{
				Utils::Copy(multitap.GetOutput(), tempBuffer, len);
			}

			// mix in the feedback from the other channel
			//for (int i = 0; i < len; i++)
			//	tempBuffer[i] += crossMix[i];

			for (int i = 0; i < lineCount; i++)
				lines[i]->Process(tempBuffer, len);

			for (int i = 0; i < lineCount; i++)
			{
				auto buf = lines[i]->GetOutput();

				if (i == 0)
				{
					for (int j = 0; j < len; j++)
						tempBuffer[j] = buf[j];
				}
				else
				{
					for (int j = 0; j < len; j++)
						tempBuffer[j] += buf[j];
				}
			}

			auto perLineGain = GetPerLineGain();
			Utils::Gain(tempBuffer, perLineGain, len);
			Utils::Copy(tempBuffer, lineOutBuffer, len);

			for (int i = 0; i < len; i++)
			{
				outBuffer[i] =
					dryOut           * input[i] +
					predelayOut      * predelayOutput[i] +
					earlyOut         * earlyOutStage[i] +
					lineOut          * tempBuffer[i];
			}
		}

		void ClearBuffers()
		{
			for (int i = 0; i < bufferSize; i++)
			{
				tempBuffer[i] = 0.0f; // Flick: 0.0 → 0.0f
				lineOutBuffer[i] = 0.0f; // Flick: 0.0 → 0.0f
				outBuffer[i] = 0.0f; // Flick: 0.0 → 0.0f
			}

			lowPass.Output = 0;
			highPass.Output = 0;

			preDelay.ClearBuffers();
			multitap.ClearBuffers();
			diffuser.ClearBuffers();
			for (int i = 0; i < TotalLineCount; i++) // Flick: Was range-based for
				lines[i]->ClearBuffers();
		}


	private:
		float GetPerLineGain()
		{
			return 1.0f / sqrtf((float)lineCount); // Flick: Was 1.0 / std::sqrt(lineCount)
		}

		void UpdateLines()
		{
			auto lineDelaySamples = (int)Ms2Samples(parameters[(int)Parameter::LineDelay]); // Flick: Was parameters[Parameter::LineDelay]
			auto lineDecayMillis = parameters[(int)Parameter::LineDecay] * 1000.0f; // Flick: Was parameters[Parameter::LineDecay] * 1000
			auto lineDecaySamples = Ms2Samples(lineDecayMillis);

			auto lineModAmount = Ms2Samples(parameters[(int)Parameter::LineModAmount]); // Flick: Was parameters[Parameter::LineModAmount]
			auto lineModRate = parameters[(int)Parameter::LineModRate]; // Flick: Was parameters[Parameter::LineModRate]

			auto lateDiffusionModAmount = Ms2Samples(parameters[(int)Parameter::LateDiffusionModAmount]); // Flick: Was parameters[Parameter::LateDiffusionModAmount]
			auto lateDiffusionModRate = parameters[(int)Parameter::LateDiffusionModRate]; // Flick: Was parameters[Parameter::LateDiffusionModRate]

			float delayLineSeeds[TotalLineCount * 3]; // Flick: Was ShaRandom::MaxCount; sized to actual need
			ShaRandom::Generate(delayLineSeed, TotalLineCount * 3, crossSeed, delayLineSeeds); // Flick: Was lines.size() * 3
			int count = TotalLineCount; // Flick: Was (int)lines.size()

			for (int i = 0; i < count; i++)
			{
				auto modAmount = lineModAmount * (0.7f + 0.3f * delayLineSeeds[i + count]); // Flick: 0.7 → 0.7f, 0.3 → 0.3f
				auto modRate = lineModRate * (0.7f + 0.3f * delayLineSeeds[i + 2 * count]) / samplerate; // Flick: 0.7 → 0.7f, 0.3 → 0.3f

				auto delaySamples = (0.5f + 1.0f * delayLineSeeds[i]) * lineDelaySamples; // Flick: 0.5 → 0.5f, 1.0 → 1.0f
				if (delaySamples < modAmount + 2) // when the delay is set really short, and the modulation is very high
					delaySamples = modAmount + 2; // the mod could actually take the delay time negative, prevent that! -- provide 2 extra sample as margin of safety

				auto dbAfter1Iteration = delaySamples / lineDecaySamples * (-60); // lineDecay is the time it takes to reach T60
				auto gainAfter1Iteration = Utils::DB2gain(dbAfter1Iteration);



				lines[i]->SetDelay((int)delaySamples);
				lines[i]->SetFeedback(gainAfter1Iteration);
				lines[i]->SetLineModAmount(modAmount);
				lines[i]->SetLineModRate(modRate);
				lines[i]->SetDiffuserModAmount(lateDiffusionModAmount);
				lines[i]->SetDiffuserModRate(lateDiffusionModRate);
			}
		}

		void UpdatePostDiffusion()
		{
			for (int i = 0; i < TotalLineCount; i++) // Flick: Was lines.size()
				lines[i]->SetDiffuserSeed(((long long)postDiffusionSeed) * (i + 1), crossSeed);
		}

		float Ms2Samples(float value)
		{
			return value / 1000.0f * samplerate; // Flick: 1000.0 → 1000.0f
		}

	};

}

#endif
