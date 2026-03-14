#ifndef AUDIOLIB_VALUETABLES
#define AUDIOLIB_VALUETABLES

#include "MathDefs.h"

namespace AudioLib
{
	class ValueTables
	{
	public:
		static const int TableSize = 4001; // Flick: Reduced from 40001 (only indices 0-4000 are used)

		// Flick: Removed 13 unused tables (Sqrt, Sqrt3, Pow1_5, Pow2, Pow3, Pow4, x2Pow3,
		// Response2Oct, Response3Oct, Response5Oct, Response6Oct, Response4Dec)
		// Only these 3 are used by GetScaledParameter() in ReverbController:
		static DSY_SDRAM_BSS float Response4Oct[TableSize];
		static DSY_SDRAM_BSS float Response2Dec[TableSize];
		static DSY_SDRAM_BSS float Response3Dec[TableSize];

		static void Init();
		static float Get(float index, float* table);
	};
}

#endif
