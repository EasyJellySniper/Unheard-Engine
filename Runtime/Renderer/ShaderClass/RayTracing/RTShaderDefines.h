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
	XMFLOAT4 HitDiffuse;
	XMFLOAT3 HitVertexNormal;
	uint32_t HitInstanceIndex;
};

struct UHDefaultPayload
{
	UHMinimalPayload MinimalPayload;
	UHIndirectPayload IndirectPayload;

	XMFLOAT3 HitMaterialNormal;
	XMFLOAT4 HitSpecular;
	XMFLOAT3 HitEmissive;
	XMFLOAT2 HitRefractOffset;

	uint32_t CurrentRecursion;
	XMFLOAT3 RayDir;
	float FresnelFactor;
};

struct UHDefaultAttribute
{
	XMFLOAT2 Bary;
};