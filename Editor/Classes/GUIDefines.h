#pragma once
#include "../../UnheardEngine.h"

#if WITH_DEBUG

struct UHGUIMeasurement
{
public:
	UHGUIMeasurement()
		: Width(0)
		, Height(0)
		, PosX(0)
		, PosY(0)
		, Margin(0)
		, MinVisibleItem(15)
	{

	}

	int32_t Width;
	int32_t Height;
	int32_t PosX;
	int32_t PosY;
	int32_t Margin;
	int32_t MinVisibleItem;
};

#endif