//see https://github.com/ValdemarOrn/CloudSeed
//port to Daisy and App_Dekrispator by Erwin Coumans

#ifndef REVERBCONTROLLER
#define REVERBCONTROLLER

// Flick: removed #include <vector> (unused)
#include "Default.h"
#include "Parameter.h"
#include "ReverbChannel.h"

// Flick: removed spurious self-include of ReverbController.h
#include "AudioLib/ValueTables.h"
#include "AllpassDiffuser.h"
#include "MultitapDiffuser.h"
#include "Utils.h"

namespace CloudSeed
{
	class ReverbController
	{
	private:
		static const int bufferSize = 8; // Flick: match Flick's audio callback block size
		int samplerate;

		ReverbChannel channelL;
		ReverbChannel channelR; // Flick: restored stereo
		float leftChannelIn[bufferSize];
		float rightChannelIn[bufferSize]; // Flick: restored stereo
		float leftLineBuffer[bufferSize];
		float rightLineBuffer[bufferSize]; // Flick: restored stereo
		float parameters[(int)Parameter::Count];

	public:
		ReverbController(int samplerate)
			: channelL(bufferSize, samplerate, ChannelLR::Left)
			, channelR(bufferSize, samplerate, ChannelLR::Right) // Flick: restored stereo
		{
			this->samplerate = samplerate;
			initFactoryChorus();
		}

		void initFactoryChorus()
		{
			//parameters from Chorus Delay in
			//https://github.com/ValdemarOrn/CloudSeed/tree/master/Factory%20Programs
			parameters[(int)Parameter::InputMix] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::PreDelay] = 0.070000000298023224f; // Flick: double->float
			parameters[(int)Parameter::HighPass] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPass] = 0.29000008106231689f; // Flick: double->float
			parameters[(int)Parameter::TapCount]= 0.36499997973442078f; // Flick: double->float
			parameters[(int)Parameter::TapLength]= 1.0f; // Flick: double->float
			parameters[(int)Parameter::TapGain] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::TapDecay] = 0.86500012874603271f; // Flick: double->float
			parameters[(int)Parameter::DiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionStages] = 0.4285714328289032f; // Flick: double->float
			parameters[(int)Parameter::DiffusionDelay] = 0.43500006198883057f; // Flick: double->float
			parameters[(int)Parameter::DiffusionFeedback] = 0.725000262260437f; // Flick: double->float
			parameters[(int)Parameter::LineDelay] = 0.68499988317489624f; // Flick: double->float
			parameters[(int)Parameter::LineDecay] = 0.68000012636184692f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionStages] = 0.28571429848670959f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionDelay] = 0.54499995708465576f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionFeedback] = 0.65999996662139893f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfGain] = 0.5199999213218689f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfFrequency] = 0.31499990820884705f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfGain] = 0.83500003814697266f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfFrequency] = 0.73000013828277588f; // Flick: double->float
			parameters[(int)Parameter::PostCutoffFrequency] = 0.73499983549118042f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModAmount] = 0.50000005960464478f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModRate] = 0.42500010132789612f; // Flick: double->float
			parameters[(int)Parameter::LineModAmount] = 0.59000003337860107f; // Flick: double->float
			parameters[(int)Parameter::LineModRate] = 0.46999993920326233f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModAmount] = 0.619999885559082f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModRate] = 0.42500019073486328f; // Flick: double->float
			parameters[(int)Parameter::TapSeed] = 0.0011500000255182385f; // Flick: double->float
			parameters[(int)Parameter::DiffusionSeed] = 0.00018899999849963933f; // Flick: double->float
			parameters[(int)Parameter::DelaySeed] = 0.00033700000494718552f; // Flick: double->float
			parameters[(int)Parameter::PostDiffusionSeed] = 0.00050099997315555811f; // Flick: double->float
			parameters[(int)Parameter::CrossSeed] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::DryOut] = 0.94499987363815308f; // Flick: double->float
			parameters[(int)Parameter::PredelayOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::EarlyOut] = 0.77999997138977051f; // Flick: double->float
			parameters[(int)Parameter::MainOut] = 0.74500006437301636f; // Flick: double->float
			parameters[(int)Parameter::HiPassEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPassEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::CutoffEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateStageTap] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::Interpolation] = 0.0f; // Flick: double->float

			for (auto value = 0; value < (int)Parameter::Count; value++)
			{
				SetParameter((Parameter)value, parameters[value]);
			}

		}


		void initFactoryDullEchos()
		{
			//parameters from Dull Echos in
			//https://github.com/ValdemarOrn/CloudSeed/tree/master/Factory%20Programs
			parameters[(int)Parameter::InputMix] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::PreDelay] = 0.070000000298023224f; // Flick: double->float
			parameters[(int)Parameter::HighPass] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPass] = 0.29000008106231689f; // Flick: double->float
			parameters[(int)Parameter::TapCount] = 0.36499997973442078f; // Flick: double->float
			parameters[(int)Parameter::TapLength] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::TapGain] = 0.83499991893768311f; // Flick: double->float
			parameters[(int)Parameter::TapDecay] = 0.86500012874603271f; // Flick: double->float
			parameters[(int)Parameter::DiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionStages] = 0.4285714328289032f; // Flick: double->float
			parameters[(int)Parameter::DiffusionDelay] = 0.43500006198883057f; // Flick: double->float
			parameters[(int)Parameter::DiffusionFeedback] = 0.725000262260437f; // Flick: double->float
			parameters[(int)Parameter::LineDelay] = 0.34500002861022949f; // Flick: double->float
			parameters[(int)Parameter::LineDecay] = 0.41500008106231689f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionStages] = 0.57142859697341919f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionDelay] = 0.66499996185302734f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionFeedback] = 0.61000001430511475f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfGain] = 0.5199999213218689f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfFrequency] = 0.31499990820884705f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfGain] = 0.83500003814697266f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfFrequency] = 0.73000013828277588f; // Flick: double->float
			parameters[(int)Parameter::PostCutoffFrequency] = 0.73499983549118042f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModAmount] = 0.25499999523162842f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModRate] = 0.3250001072883606f; // Flick: double->float
			parameters[(int)Parameter::LineModAmount] = 0.33500000834465027f; // Flick: double->float
			parameters[(int)Parameter::LineModRate] = 0.26999998092651367f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModAmount] = 0.13499975204467773f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModRate] = 0.27500006556510925f; // Flick: double->float
			parameters[(int)Parameter::TapSeed] = 0.0011500000255182385f; // Flick: double->float
			parameters[(int)Parameter::DiffusionSeed] = 0.00018899999849963933f; // Flick: double->float
			parameters[(int)Parameter::DelaySeed] = 0.0002730000123847276f; // Flick: double->float
			parameters[(int)Parameter::PostDiffusionSeed] = 0.00050099997315555811f; // Flick: double->float
			parameters[(int)Parameter::CrossSeed] = 0.5f; // Flick: double->float
			parameters[(int)Parameter::DryOut] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::PredelayOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::EarlyOut] = 0.77999997138977051f; // Flick: double->float
			parameters[(int)Parameter::MainOut] = 0.74500006437301636f; // Flick: double->float
			parameters[(int)Parameter::HiPassEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPassEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LowShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::CutoffEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateStageTap] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::Interpolation] = 1.0f; // Flick: double->float

			for (auto value = 0; value < (int)Parameter::Count; value++)
			{
				SetParameter((Parameter)value, parameters[value]);
			}

		}


		void initFactoryHyperplane()
		{
			//parameters from Hyperplane in
			//https://github.com/ValdemarOrn/CloudSeed/tree/master/Factory%20Programs
			parameters[(int)Parameter::InputMix] = 0.1549999862909317f; // Flick: double->float
			parameters[(int)Parameter::PreDelay] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighPass] = 0.57999998331069946f; // Flick: double->float
			parameters[(int)Parameter::LowPass] = 0.9100000262260437f; // Flick: double->float
			parameters[(int)Parameter::TapCount] = 0.41499990224838257f; // Flick: double->float
			parameters[(int)Parameter::TapLength] = 0.43999996781349182f; // Flick: double->float
			parameters[(int)Parameter::TapGain] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::TapDecay] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionStages] = 0.4285714328289032f; // Flick: double->float
			parameters[(int)Parameter::DiffusionDelay] = 0.27500024437904358f; // Flick: double->float
			parameters[(int)Parameter::DiffusionFeedback] = 0.660000205039978f; // Flick: double->float
			parameters[(int)Parameter::LineDelay] = 0.22500017285346985f; // Flick: double->float
			parameters[(int)Parameter::LineDecay] = 0.794999897480011f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionStages] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionDelay] = 0.22999951243400574f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionFeedback] = 0.59499990940093994f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfGain] = 0.95999979972839355f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfFrequency] = 0.23999994993209839f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfGain] = 0.97500002384185791f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfFrequency] = 0.78499996662139893f; // Flick: double->float
			parameters[(int)Parameter::PostCutoffFrequency] = 0.87999981641769409f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModAmount] = 0.13499999046325684f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModRate] = 0.29000008106231689f; // Flick: double->float
			parameters[(int)Parameter::LineModAmount] = 0.53999996185302734f; // Flick: double->float
			parameters[(int)Parameter::LineModRate] = 0.44999989867210388f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModAmount] = 0.15999998152256012f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModRate] = 0.56000012159347534f; // Flick: double->float
			parameters[(int)Parameter::TapSeed] = 0.00048499999684281647f; // Flick: double->float
			parameters[(int)Parameter::DiffusionSeed] = 0.00020799999765586108f; // Flick: double->float
			parameters[(int)Parameter::DelaySeed] = 0.00034699999378062785f; // Flick: double->float
			parameters[(int)Parameter::PostDiffusionSeed] = 0.00037200000951997936f; // Flick: double->float
			parameters[(int)Parameter::CrossSeed] = 0.800000011920929f; // Flick: double->float
			parameters[(int)Parameter::DryOut] = 0.86500018835067749f; // Flick: double->float
			parameters[(int)Parameter::PredelayOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::EarlyOut] = 0.8200000524520874f; // Flick: double->float
			parameters[(int)Parameter::MainOut] = 0.79500007629394531f; // Flick: double->float
			parameters[(int)Parameter::HiPassEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LowPassEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LowShelfEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::HighShelfEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::CutoffEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateStageTap] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::Interpolation] = 0.0f; // Flick: double->float

			for (auto value = 0; value < (int)Parameter::Count; value++)
			{
				SetParameter((Parameter)value, parameters[value]);
			}

		}

		void initFactoryMediumSpace()
		{
			//parameters from Medium Space in
			//https://github.com/ValdemarOrn/CloudSeed/tree/master/Factory%20Programs
			parameters[(int)Parameter::InputMix] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::PreDelay] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighPass] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPass] = 0.63999992609024048f; // Flick: double->float
			parameters[(int)Parameter::TapCount] = 0.51999980211257935f; // Flick: double->float
			parameters[(int)Parameter::TapLength] = 0.26499992609024048f; // Flick: double->float
			parameters[(int)Parameter::TapGain] = 0.69499999284744263f; // Flick: double->float
			parameters[(int)Parameter::TapDecay] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionStages] = 0.8571428656578064f; // Flick: double->float
			parameters[(int)Parameter::DiffusionDelay] = 0.5700000524520874f; // Flick: double->float
			parameters[(int)Parameter::DiffusionFeedback] = 0.76000010967254639f; // Flick: double->float
			parameters[(int)Parameter::LineDelay] = 0.585000216960907f; // Flick: double->float
			parameters[(int)Parameter::LineDecay] = 0.29499980807304382f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionStages] = 0.57142859697341919f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionDelay] = 0.69499951601028442f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionFeedback] = 0.71499985456466675f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfGain] = 0.87999987602233887f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfFrequency] = 0.19499993324279785f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfGain] = 0.72000008821487427f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfFrequency] = 0.520000159740448f; // Flick: double->float
			parameters[(int)Parameter::PostCutoffFrequency] = 0.79999983310699463f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModAmount] = 0.13499999046325684f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModRate] = 0.26000010967254639f; // Flick: double->float
			parameters[(int)Parameter::LineModAmount] = 0.054999928921461105f; // Flick: double->float
			parameters[(int)Parameter::LineModRate] = 0.21499986946582794f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModAmount] = 0.17999963462352753f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModRate] = 0.38000011444091797f; // Flick: double->float
			parameters[(int)Parameter::TapSeed] = 0.0003009999927598983f; // Flick: double->float
			parameters[(int)Parameter::DiffusionSeed] = 0.00018899999849963933f; // Flick: double->float
			parameters[(int)Parameter::DelaySeed] = 0.0001610000035725534f; // Flick: double->float
			parameters[(int)Parameter::PostDiffusionSeed] = 0.00050099997315555811f; // Flick: double->float
			parameters[(int)Parameter::CrossSeed] = 0.7850000262260437f; // Flick: double->float
			parameters[(int)Parameter::DryOut] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::PredelayOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::EarlyOut] = 0.699999988079071f; // Flick: double->float
			parameters[(int)Parameter::MainOut] = 0.84499984979629517f; // Flick: double->float
			parameters[(int)Parameter::HiPassEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPassEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LowShelfEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::HighShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::CutoffEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateStageTap] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::Interpolation] = 1.0f; // Flick: double->float

			for (auto value = 0; value < (int)Parameter::Count; value++)
			{
				SetParameter((Parameter)value, parameters[value]);
			}

		}

		void initFactoryNoiseInTheHallway()
		{
			//parameters from Noise In The Hallway in
			//https://github.com/ValdemarOrn/CloudSeed/tree/master/Factory%20Programs
			parameters[(int)Parameter::InputMix] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::PreDelay] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighPass] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPass] = 0.60999995470047f; // Flick: double->float
			parameters[(int)Parameter::TapCount] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::TapLength] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::TapGain] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::TapDecay] = 0.830000102519989f; // Flick: double->float
			parameters[(int)Parameter::DiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionStages] = 0.28571429848670959f; // Flick: double->float
			parameters[(int)Parameter::DiffusionDelay] = 0.35499998927116394f; // Flick: double->float
			parameters[(int)Parameter::DiffusionFeedback] = 0.62500005960464478f; // Flick: double->float
			parameters[(int)Parameter::LineDelay] = 0.36000004410743713f; // Flick: double->float
			parameters[(int)Parameter::LineDecay] = 0.51000005006790161f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionStages] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionDelay] = 0.62999987602233887f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionFeedback] = 0.49000000953674316f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfGain] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfFrequency] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfGain] = 0.77499985694885254f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfFrequency] = 0.58000004291534424f; // Flick: double->float
			parameters[(int)Parameter::PostCutoffFrequency] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModAmount] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModRate] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LineModAmount] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LineModRate] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModAmount] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModRate] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::TapSeed] = 0.0001140000022132881f; // Flick: double->float
			parameters[(int)Parameter::DiffusionSeed] = 0.000155999994603917f; // Flick: double->float
			parameters[(int)Parameter::DelaySeed] = 0.00018099999579135329f; // Flick: double->float
			parameters[(int)Parameter::PostDiffusionSeed] = 8.4999999671708792E-05f; // Flick: double->float
			parameters[(int)Parameter::CrossSeed] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DryOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::PredelayOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::EarlyOut] = 0.64500010013580322f; // Flick: double->float
			parameters[(int)Parameter::MainOut] = 0.63000005483627319f; // Flick: double->float
			parameters[(int)Parameter::HiPassEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPassEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LowShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighShelfEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::CutoffEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LateStageTap] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::Interpolation] = 1.0f; // Flick: double->float

			for (auto value = 0; value < (int)Parameter::Count; value++)
			{
				SetParameter((Parameter)value, parameters[value]);
			}

		}

		void initFactoryRubiKaFields()
		{
			//parameters from Rubi-Ka Fields in
			//https://github.com/ValdemarOrn/CloudSeed/tree/master/Factory%20Programs
			parameters[(int)Parameter::InputMix] = 0.32499998807907104f; // Flick: double->float
			parameters[(int)Parameter::PreDelay] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighPass] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPass] = 0.8899998664855957f; // Flick: double->float
			parameters[(int)Parameter::TapCount] = 0.51999980211257935f; // Flick: double->float
			parameters[(int)Parameter::TapLength] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::TapGain] = 0.90000003576278687f; // Flick: double->float
			parameters[(int)Parameter::TapDecay] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionStages] = 0.8571428656578064f; // Flick: double->float
			parameters[(int)Parameter::DiffusionDelay] = 0.5700000524520874f; // Flick: double->float
			parameters[(int)Parameter::DiffusionFeedback] = 0.76000010967254639f; // Flick: double->float
			parameters[(int)Parameter::LineDelay] = 0.68500018119812012f; // Flick: double->float
			parameters[(int)Parameter::LineDecay] = 0.82999974489212036f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionStages] = 0.71428573131561279f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionDelay] = 0.69499951601028442f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionFeedback] = 0.71499985456466675f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfGain] = 0.87999987602233887f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfFrequency] = 0.19499993324279785f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfGain] = 0.72000008821487427f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfFrequency] = 0.520000159740448f; // Flick: double->float
			parameters[(int)Parameter::PostCutoffFrequency] = 0.79999983310699463f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModAmount] = 0.13499999046325684f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModRate] = 0.26000010967254639f; // Flick: double->float
			parameters[(int)Parameter::LineModAmount] = 0.054999928921461105f; // Flick: double->float
			parameters[(int)Parameter::LineModRate] = 0.21499986946582794f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModAmount] = 0.32499963045120239f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModRate] = 0.35500010848045349f; // Flick: double->float
			parameters[(int)Parameter::TapSeed] = 0.0003009999927598983f; // Flick: double->float
			parameters[(int)Parameter::DiffusionSeed] = 0.00018899999849963933f; // Flick: double->float
			parameters[(int)Parameter::DelaySeed] = 0.0001610000035725534f; // Flick: double->float
			parameters[(int)Parameter::PostDiffusionSeed] = 0.00050099997315555811f; // Flick: double->float
			parameters[(int)Parameter::CrossSeed] = 0.43000003695487976f; // Flick: double->float
			parameters[(int)Parameter::DryOut] = 0.88499999046325684f; // Flick: double->float
			parameters[(int)Parameter::PredelayOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::EarlyOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::MainOut] = 0.90999990701675415f; // Flick: double->float
			parameters[(int)Parameter::HiPassEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPassEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::CutoffEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateStageTap] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::Interpolation] = 0.0f; // Flick: double->float

			for (auto value = 0; value < (int)Parameter::Count; value++)
			{
				SetParameter((Parameter)value, parameters[value]);
			}

		}


		void initFactorySmallRoom()
		{
			//parameters from Small Room in
			//https://github.com/ValdemarOrn/CloudSeed/tree/master/Factory%20Programs
			parameters[(int)Parameter::InputMix] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::PreDelay] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighPass] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPass] = 0.755000114440918f; // Flick: double->float
			parameters[(int)Parameter::TapCount] = 0.41499990224838257f; // Flick: double->float
			parameters[(int)Parameter::TapLength] = 0.43999996781349182f; // Flick: double->float
			parameters[(int)Parameter::TapGain] = 0.87999999523162842f; // Flick: double->float
			parameters[(int)Parameter::TapDecay] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionStages] = 0.71428573131561279f; // Flick: double->float
			parameters[(int)Parameter::DiffusionDelay] = 0.335000216960907f; // Flick: double->float
			parameters[(int)Parameter::DiffusionFeedback] = 0.660000205039978f; // Flick: double->float
			parameters[(int)Parameter::LineDelay] = 0.51000016927719116f; // Flick: double->float
			parameters[(int)Parameter::LineDecay] = 0.29999998211860657f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionStages] = 0.4285714328289032f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionDelay] = 0.22999951243400574f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionFeedback] = 0.59499990940093994f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfGain] = 0.87999987602233887f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfFrequency] = 0.19499993324279785f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfGain] = 0.875f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfFrequency] = 0.59000009298324585f; // Flick: double->float
			parameters[(int)Parameter::PostCutoffFrequency] = 0.79999983310699463f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModAmount] = 0.13499999046325684f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModRate] = 0.29000008106231689f; // Flick: double->float
			parameters[(int)Parameter::LineModAmount] = 0.18999995291233063f; // Flick: double->float
			parameters[(int)Parameter::LineModRate] = 0.22999987006187439f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModAmount] = 0.1249999925494194f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModRate] = 0.28500008583068848f; // Flick: double->float
			parameters[(int)Parameter::TapSeed] = 0.00048499999684281647f; // Flick: double->float
			parameters[(int)Parameter::DiffusionSeed] = 0.00020799999765586108f; // Flick: double->float
			parameters[(int)Parameter::DelaySeed] = 0.00033499998971819878f; // Flick: double->float
			parameters[(int)Parameter::PostDiffusionSeed] = 0.00037200000951997936f; // Flick: double->float
			parameters[(int)Parameter::CrossSeed] = 0.42500001192092896f; // Flick: double->float
			parameters[(int)Parameter::DryOut] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::PredelayOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::EarlyOut] = 0.8599998950958252f; // Flick: double->float
			parameters[(int)Parameter::MainOut] = 0.90500003099441528f; // Flick: double->float
			parameters[(int)Parameter::HiPassEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPassEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LowShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::CutoffEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LateStageTap] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::Interpolation] = 1.0f; // Flick: double->float
			for (auto value = 0; value < (int)Parameter::Count; value++)
			{
				SetParameter((Parameter)value, parameters[value]);
			}

		}

		void initFactory90sAreBack()
		{
			//parameters from The 90s Are Back in
			//https://github.com/ValdemarOrn/CloudSeed/tree/master/Factory%20Programs
			parameters[(int)Parameter::InputMix] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::PreDelay] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighPass] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPass] = 0.6750001311302185f; // Flick: double->float
			parameters[(int)Parameter::TapCount] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::TapLength] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::TapGain] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::TapDecay] = 0.8650001287460327f; // Flick: double->float
			parameters[(int)Parameter::DiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionStages] = 0.5714285969734192f; // Flick: double->float
			parameters[(int)Parameter::DiffusionDelay] = 0.7100000381469727f; // Flick: double->float
			parameters[(int)Parameter::DiffusionFeedback] = 0.5450003147125244f; // Flick: double->float
			parameters[(int)Parameter::LineDelay] = 0.6849998831748962f; // Flick: double->float
			parameters[(int)Parameter::LineDecay] = 0.6300000548362732f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionStages] = 0.2857142984867096f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionDelay] = 0.5449999570846558f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionFeedback] = 0.6599999666213989f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfGain] = 0.5199999213218689f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfFrequency] = 0.31499990820884705f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfGain] = 0.8349999189376831f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfFrequency] = 0.705000102519989f; // Flick: double->float
			parameters[(int)Parameter::PostCutoffFrequency] = 0.7349998354911804f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModAmount] = 0.824999988079071f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModRate] = 0.4050004780292511f; // Flick: double->float
			parameters[(int)Parameter::LineModAmount] = 0.6300000548362732f; // Flick: double->float
			parameters[(int)Parameter::LineModRate] = 0.3199999928474426f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModAmount] = 0.619999885559082f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModRate] = 0.30000022053718567f; // Flick: double->float
			parameters[(int)Parameter::TapSeed] = 0.0011500000255182385f; // Flick: double->float
			parameters[(int)Parameter::DiffusionSeed] = 0.00018899999849963933f; // Flick: double->float
			parameters[(int)Parameter::DelaySeed] = 0.0003370000049471855f; // Flick: double->float
			parameters[(int)Parameter::PostDiffusionSeed] = 0.0005009999731555581f; // Flick: double->float
			parameters[(int)Parameter::CrossSeed] = 0.7950000166893005f; // Flick: double->float
			parameters[(int)Parameter::DryOut] = 0.9449997544288635f; // Flick: double->float
			parameters[(int)Parameter::PredelayOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::EarlyOut] = 0.7250000238418579f; // Flick: double->float
			parameters[(int)Parameter::MainOut] = 0.6050001382827759f; // Flick: double->float
			parameters[(int)Parameter::HiPassEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPassEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LowShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighShelfEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::CutoffEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LateStageTap] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::Interpolation] = 1.0f; // Flick: double->float
			for (auto value = 0; value < (int)Parameter::Count; value++)
			{
				SetParameter((Parameter)value, parameters[value]);
			}

		}

		void initFactoryThroughTheLookingGlass()
		{
			//parameters from Through The Looking Glass in
			//https://github.com/ValdemarOrn/CloudSeed/tree/master/Factory%20Programs
			parameters[(int)Parameter::InputMix] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::PreDelay] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighPass] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPass] = 0.74000012874603271f; // Flick: double->float
			parameters[(int)Parameter::TapCount] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::TapLength] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::TapGain] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::TapDecay] = 0.71000003814697266f; // Flick: double->float
			parameters[(int)Parameter::DiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionStages] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DiffusionDelay] = 0.65999996662139893f; // Flick: double->float
			parameters[(int)Parameter::DiffusionFeedback] = 0.76000010967254639f; // Flick: double->float
			parameters[(int)Parameter::LineDelay] = 0.9100002646446228f; // Flick: double->float
			parameters[(int)Parameter::LineDecay] = 0.80999958515167236f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionStages] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionDelay] = 0.71499955654144287f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionFeedback] = 0.71999979019165039f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfGain] = 0.87999987602233887f; // Flick: double->float
			parameters[(int)Parameter::PostLowShelfFrequency] = 0.19499993324279785f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfGain] = 0.72000008821487427f; // Flick: double->float
			parameters[(int)Parameter::PostHighShelfFrequency] = 0.520000159740448f; // Flick: double->float
			parameters[(int)Parameter::PostCutoffFrequency] = 0.7150002121925354f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModAmount] = 0.41999998688697815f; // Flick: double->float
			parameters[(int)Parameter::EarlyDiffusionModRate] = 0.30500012636184692f; // Flick: double->float
			parameters[(int)Parameter::LineModAmount] = 0.4649999737739563f; // Flick: double->float
			parameters[(int)Parameter::LineModRate] = 0.3199998140335083f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModAmount] = 0.40999993681907654f; // Flick: double->float
			parameters[(int)Parameter::LateDiffusionModRate] = 0.31500011682510376f; // Flick: double->float
			parameters[(int)Parameter::TapSeed] = 0.0003009999927598983f; // Flick: double->float
			parameters[(int)Parameter::DiffusionSeed] = 0.00018899999849963933f; // Flick: double->float
			parameters[(int)Parameter::DelaySeed] = 0.0001610000035725534f; // Flick: double->float
			parameters[(int)Parameter::PostDiffusionSeed] = 0.00050099997315555811f; // Flick: double->float
			parameters[(int)Parameter::CrossSeed] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::DryOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::PredelayOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::EarlyOut] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::MainOut] = 0.95499974489212036f; // Flick: double->float
			parameters[(int)Parameter::HiPassEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::LowPassEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LowShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::HighShelfEnabled] = 0.0f; // Flick: double->float
			parameters[(int)Parameter::CutoffEnabled] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::LateStageTap] = 1.0f; // Flick: double->float
			parameters[(int)Parameter::Interpolation] = 1.0f; // Flick: double->float
			for (auto value = 0; value < (int)Parameter::Count; value++)
			{
				SetParameter((Parameter)value, parameters[value]);
			}

		}

		int GetSamplerate()
		{
			return samplerate;
		}

		void SetSamplerate(int samplerate)
		{
			this->samplerate = samplerate;

			channelL.SetSamplerate(samplerate);
			channelR.SetSamplerate(samplerate); // Flick: restored stereo
		}

		int GetParameterCount()
		{
			return (int)Parameter::Count;
		}

		float* GetAllParameters()
		{
			return parameters;
		}

		float GetScaledParameter(Parameter param)
		{
			switch (param)
			{
				// Input
			case Parameter::InputMix:                  return P(Parameter::InputMix);
			case Parameter::PreDelay:                  return (int)(P(Parameter::PreDelay) * 1000);

			case Parameter::HighPass:                  return 20 + ValueTables::Get(P(Parameter::HighPass), ValueTables::Response4Oct) * 980.0f; // Flick: double->float
			case Parameter::LowPass:                   return 400 + ValueTables::Get(P(Parameter::LowPass), ValueTables::Response4Oct) * 19600.0f; // Flick: double->float

				// Early
			case Parameter::TapCount:                  return 1 + (int)(P(Parameter::TapCount) * (MultitapDiffuser::MaxTaps - 1));
			case Parameter::TapLength:                 return (int)(P(Parameter::TapLength) * 500);
			case Parameter::TapGain:                   return ValueTables::Get(P(Parameter::TapGain), ValueTables::Response2Dec);
			case Parameter::TapDecay:                  return P(Parameter::TapDecay);

			case Parameter::DiffusionEnabled:          return P(Parameter::DiffusionEnabled) < 0.5f ? 0.0f : 1.0f; // Flick: double->float
			case Parameter::DiffusionStages:           return 1 + (int)(P(Parameter::DiffusionStages) * (AllpassDiffuser::MaxStageCount - 0.001f)); // Flick: double->float
			case Parameter::DiffusionDelay:            return (int)(10 + P(Parameter::DiffusionDelay) * 90);
			case Parameter::DiffusionFeedback:         return P(Parameter::DiffusionFeedback);

				// Late
			case Parameter::LineCount:                 return (int)(P(Parameter::LineCount));
			case Parameter::LineDelay:                 return (int)(20.0f + ValueTables::Get(P(Parameter::LineDelay), ValueTables::Response2Dec) * 980.0f); // Flick: double->float
			case Parameter::LineDecay:                 return 0.05f + ValueTables::Get(P(Parameter::LineDecay), ValueTables::Response3Dec) * 59.95f; // Flick: double->float

			case Parameter::LateDiffusionEnabled:      return P(Parameter::LateDiffusionEnabled) < 0.5f ? 0.0f : 1.0f; // Flick: double->float
			case Parameter::LateDiffusionStages:       return 1 + (int)(P(Parameter::LateDiffusionStages) * (AllpassDiffuser::MaxStageCount - 0.001f)); // Flick: double->float
			case Parameter::LateDiffusionDelay:        return (int)(10 + P(Parameter::LateDiffusionDelay) * 90);
			case Parameter::LateDiffusionFeedback:     return P(Parameter::LateDiffusionFeedback);

				// Frequency Response
			case Parameter::PostLowShelfGain:          return ValueTables::Get(P(Parameter::PostLowShelfGain), ValueTables::Response2Dec);
			case Parameter::PostLowShelfFrequency:     return 20 + ValueTables::Get(P(Parameter::PostLowShelfFrequency), ValueTables::Response4Oct) * 980.0f; // Flick: double->float
			case Parameter::PostHighShelfGain:         return ValueTables::Get(P(Parameter::PostHighShelfGain), ValueTables::Response2Dec);
			case Parameter::PostHighShelfFrequency:    return 400 + ValueTables::Get(P(Parameter::PostHighShelfFrequency), ValueTables::Response4Oct) * 19600.0f; // Flick: double->float
			case Parameter::PostCutoffFrequency:       return 400 + ValueTables::Get(P(Parameter::PostCutoffFrequency), ValueTables::Response4Oct) * 19600.0f; // Flick: double->float

				// Modulation
			case Parameter::EarlyDiffusionModAmount:   return P(Parameter::EarlyDiffusionModAmount) * 2.5f; // Flick: double->float
			case Parameter::EarlyDiffusionModRate:     return ValueTables::Get(P(Parameter::EarlyDiffusionModRate), ValueTables::Response2Dec) * 5.0f; // Flick: double->float
			case Parameter::LineModAmount:             return P(Parameter::LineModAmount) * 2.5f; // Flick: double->float
			case Parameter::LineModRate:               return ValueTables::Get(P(Parameter::LineModRate), ValueTables::Response2Dec) * 5.0f; // Flick: double->float
			case Parameter::LateDiffusionModAmount:    return P(Parameter::LateDiffusionModAmount) * 2.5f; // Flick: double->float
			case Parameter::LateDiffusionModRate:      return ValueTables::Get(P(Parameter::LateDiffusionModRate), ValueTables::Response2Dec) * 5.0f; // Flick: double->float

				// Seeds
			case Parameter::TapSeed:                   return (int)floorf(P(Parameter::TapSeed) * 1000000 + 0.001f); // Flick: std::floor->floorf, double->float
			case Parameter::DiffusionSeed:             return (int)floorf(P(Parameter::DiffusionSeed) * 1000000 + 0.001f); // Flick: std::floor->floorf, double->float
			case Parameter::DelaySeed:                 return (int)floorf(P(Parameter::DelaySeed) * 1000000 + 0.001f); // Flick: std::floor->floorf, double->float
			case Parameter::PostDiffusionSeed:         return (int)floorf(P(Parameter::PostDiffusionSeed) * 1000000 + 0.001f); // Flick: std::floor->floorf, double->float

				// Output
			case Parameter::CrossSeed:                 return P(Parameter::CrossSeed);

			case Parameter::DryOut:                    return ValueTables::Get(P(Parameter::DryOut), ValueTables::Response2Dec);
			case Parameter::PredelayOut:               return ValueTables::Get(P(Parameter::PredelayOut), ValueTables::Response2Dec);
			case Parameter::EarlyOut:                  return ValueTables::Get(P(Parameter::EarlyOut), ValueTables::Response2Dec);
			case Parameter::MainOut:                   return ValueTables::Get(P(Parameter::MainOut), ValueTables::Response2Dec);

				// Switches
			case Parameter::HiPassEnabled:             return P(Parameter::HiPassEnabled) < 0.5f ? 0.0f : 1.0f; // Flick: double->float
			case Parameter::LowPassEnabled:            return P(Parameter::LowPassEnabled) < 0.5f ? 0.0f : 1.0f; // Flick: double->float
			case Parameter::LowShelfEnabled:           return P(Parameter::LowShelfEnabled) < 0.5f ? 0.0f : 1.0f; // Flick: double->float
			case Parameter::HighShelfEnabled:          return P(Parameter::HighShelfEnabled) < 0.5f ? 0.0f : 1.0f; // Flick: double->float
			case Parameter::CutoffEnabled:             return P(Parameter::CutoffEnabled) < 0.5f ? 0.0f : 1.0f; // Flick: double->float
			case Parameter::LateStageTap:			   return P(Parameter::LateStageTap) < 0.5f ? 0.0f : 1.0f; // Flick: double->float

				// Effects
			case Parameter::Interpolation:			   return P(Parameter::Interpolation) < 0.5f ? 0.0f : 1.0f; // Flick: double->float

			default: return 0.0f; // Flick: double->float
			}

			return 0.0f; // Flick: double->float
		}

		void SetParameter(Parameter param, float value)
		{
			parameters[(int)param] = value;
			auto scaled = GetScaledParameter(param);

			channelL.SetParameter(param, scaled);
			channelR.SetParameter(param, scaled); // Flick: restored stereo
		}



		void ClearBuffers()
		{
			channelL.ClearBuffers();
			channelR.ClearBuffers(); // Flick: restored stereo
		}

		// Flick: restored stereo Process() replacing mono version
		void Process(float* inputL, float* inputR, float* outputL, float* outputR, int sampleCount)
		{
			auto len = sampleCount;
			for (int i = 0; i < len; i++)
			{
				leftChannelIn[i] = inputL[i];
				rightChannelIn[i] = inputR[i];
			}

			channelL.Process(leftChannelIn, len);
			channelR.Process(rightChannelIn, len);
			auto leftOut = channelL.GetOutput();
			auto rightOut = channelR.GetOutput();

			for (int i = 0; i < len; i++)
			{
				outputL[i] = leftOut[i];
				outputR[i] = rightOut[i];
			}
		}

		// Flick: preset selector by index
		void SetPreset(int index)
		{
			switch (index)
			{
				case 0: initFactoryChorus(); break;
				case 1: initFactoryDullEchos(); break;
				case 2: initFactoryHyperplane(); break;
				case 3: initFactoryMediumSpace(); break;
				case 4: initFactoryNoiseInTheHallway(); break;
				case 5: initFactoryRubiKaFields(); break;
				case 6: initFactorySmallRoom(); break;
				case 7: initFactory90sAreBack(); break;
				case 8: initFactoryThroughTheLookingGlass(); break;
				default: initFactoryChorus(); break;
			}
		}

		static const int NumPresets = 9; // Flick: preset count

	private:
		float P(Parameter para)
		{
			auto idx = (int)para;
			return idx >= 0 && idx < (int)Parameter::Count ? parameters[idx] : 0.0f; // Flick: double->float
		}
	};
}
#endif
