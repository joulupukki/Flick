
#include <cmath>
#include "ValueTables.h"

namespace AudioLib
{
	// Flick: Removed 13 unused table definitions, kept only the 3 used by GetScaledParameter()
	float DSY_SDRAM_BSS ValueTables::Response4Oct[TableSize];
	float DSY_SDRAM_BSS ValueTables::Response2Dec[TableSize];
	float DSY_SDRAM_BSS ValueTables::Response3Dec[TableSize];

	void ValueTables::Init()
	{
		for (int i = 0; i <= 4000; i++)
		{
			float x = i / 4000.0f;

			Response4Oct[i] = (powf(16.0f, x) - 1.0f) / 16.0f + 0.125f / 2.0f;
			Response2Dec[i] = powf(100.0f, x) / 100.0f;
			Response3Dec[i] = powf(1000.0f, x) / 1000.0f;
		}

		for (int i = 1; i <= 4000; i++)
		{
			Response4Oct[i] = (Response4Oct[i] - Response4Oct[0]) / (1.0f - Response4Oct[0]);
			Response2Dec[i] = (Response2Dec[i] - Response2Dec[0]) / (1.0f - Response2Dec[0]);
			Response3Dec[i] = (Response3Dec[i] - Response3Dec[0]) / (1.0f - Response3Dec[0]);
		}

		Response4Oct[0] = 0;
		Response2Dec[0] = 0;
		Response3Dec[0] = 0;
	}

	float ValueTables::Get(float index, float* table)
	{
		if (table == nullptr)
			return index;

		int idx = (int)(index * 4000.999f);

		if (idx < 0) idx = 0;
		if (idx >= TableSize) idx = TableSize - 1;

		return table[idx];
	}
}
