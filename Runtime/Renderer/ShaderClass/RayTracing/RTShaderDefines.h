#pragma once
#include "Runtime/Classes/Types.h"

// this should be sync with the define in the shader

// wrapper for minimal payload (shadows)
struct UHMinimalPayload
{
	float HitT;
	float MipLevel;
	float HitAlpha;
};

// wrapper for indirect light payload
struct UHIndirectPayload
{
	uint32_t PayloadData;
	UHVector4 HitDiffuse;
	UHVector3 HitVertexNormal;
	uint32_t HitInstanceIndex;
};

struct UHDefaultPayload
{
	UHMinimalPayload MinimalPayload;
	UHIndirectPayload IndirectPayload;

	UHVector3 HitMaterialNormal;
	UHVector4 HitSpecular;
	UHVector3 HitEmissive;
	UHVector2 HitRefractOffset;

	uint32_t CurrentRecursion;
	UHVector3 RayDir;
	float FresnelFactor;
};

struct UHDefaultAttribute
{
	UHVector2 Bary;
};