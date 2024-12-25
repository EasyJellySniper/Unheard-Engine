#pragma once
#include "Runtime/Classes/Types.h"

// this should be sync with the define in the shader
struct UHDefaultPayload
{
	float HitT;
	float MipLevel;
	float HitAlpha;
	uint32_t PayloadData;

	// for opaque
	XMFLOAT4 HitDiffuse;
	XMFLOAT3 HitNormal;
	XMFLOAT3 HitWorldNormal;
	XMFLOAT4 HitSpecular;
	XMFLOAT3 HitEmissive;
	XMFLOAT2 HitScreenUV;

	// for translucent
	XMFLOAT4 HitDiffuseTrans;
	XMFLOAT3 HitNormalTrans;
	XMFLOAT3 HitWorldNormalTrans;
	XMFLOAT4 HitSpecularTrans;
	XMFLOAT4 HitEmissiveTrans;
	XMFLOAT2 HitScreenUVTrans;
	XMFLOAT3 HitWorldPosTrans;
	XMFLOAT2 HitRefractOffset;

	uint32_t IsInsideScreen;
	uint32_t HitInstanceIndex;
	uint32_t CurrentRecursion;
	// xyz = world pos, w = fresnel factor
	XMFLOAT4 PackedData0;
	XMFLOAT3 RayDir;
};

struct UHDefaultAttribute
{
	XMFLOAT2 Bary;
};