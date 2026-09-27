#pragma once

#include "Common.h"

struct DescriptorIndex
{
	uint32_t index = UINT32_MAX;

	// first time trying this inline stuff
	inline bool IsValid() const
	{
		return index != UINT32_MAX;
	}
};