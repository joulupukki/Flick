
#include <climits>
#include <cstring>
#include "ShaRandom.h"
#include "../Utils/Sha256.h"

// Flick: Removed "using namespace std;" — no longer using any std types

namespace AudioLib
{
	// Flick: Changed from returning std::vector<float> to writing to output buffer.
	// Uses fixed-size stack buffers instead of vectors.
	void ShaRandom::Generate(long long seed, int count, float* output)
	{
		// Max iterations: count * 4 / 32 + 1. For count=300: 300*4/32+1 = 39.
		// 39 * 32 = 1248 bytes needed. 1024 covers count up to ~256, use 1280 for safety.
		unsigned char byteList[1280]; // Flick: Fixed-size stack buffer (was std::vector)
		unsigned char bytes[32]; // Flick: Fixed-size stack buffer (was std::vector)

		auto iterations = count * sizeof(unsigned int) / (256 / 8) + 1;
		auto byteArr = (unsigned char*)&seed;
		memcpy(bytes, byteArr, 8); // Flick: memcpy instead of vector constructor

		for (size_t i = 0; i < iterations; i++)
		{
			sha256(bytes, 8, bytes); // Flick: Updated call to new sha256 signature
			memcpy(&byteList[i * 32], bytes, 32); // Flick: memcpy instead of push_back loop
		}

		auto intArray = (unsigned int*)(&byteList[0]);

		for (int i = 0; i < count; i++)
		{
			unsigned int val = intArray[i];
			float floatVal = val / (float)UINT_MAX;
			output[i] = floatVal; // Flick: Write directly to output instead of push_back
		}
	}

	// Flick: Changed from returning std::vector<float> to writing to output buffer.
	// Uses stack-allocated arrays instead of vectors (only called during init, not audio path).
	void ShaRandom::Generate(long long seed, int count, float crossSeed, float* output)
	{
		auto seedA = seed;
		auto seedB = ~seed;
		float seriesA[MaxCount]; // Flick: Stack buffer instead of std::vector
		float seriesB[MaxCount]; // Flick: Stack buffer instead of std::vector
		Generate(seedA, count, seriesA);
		Generate(seedB, count, seriesB);

		for (int i = 0; i < count; i++)
			output[i] = seriesA[i] * (1 - crossSeed) + seriesB[i] * crossSeed; // Flick: Write directly to output
	}
}
