#pragma once
#include "../../../Runtime/Classes/Types.h"

// this should be sync with the define in the shader
struct UHDefaultPayload
{
	float HitT;
};

struct UHDefaultAttribute
{
	XMFLOAT2 Bary;
};