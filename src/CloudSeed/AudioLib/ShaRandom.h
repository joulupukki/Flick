#ifndef SHARANDOM
#define SHARANDOM

// Flick: Removed <vector> — output written to caller-provided buffer instead

namespace AudioLib
{
	class ShaRandom
	{
	public:
		// Flick: Max output count for stack buffer sizing (largest caller is
		// MultitapDiffuser with count=100; crossSeed version needs this too)
		static const int MaxCount = 300;

		// Flick: Changed from returning std::vector<float> to writing to output buffer
		static void Generate(long long seed, int count, float* output);
		static void Generate(long long seed, int count, float crossSeed, float* output);
	};
}

#endif
