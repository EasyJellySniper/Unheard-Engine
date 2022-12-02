// descriptor binding are shared in Raytracing States
// I've not found the similar usage to "Local Root Signature" of D3D12, so just make sure resource slots won't overlap with other resource
#define UHMAT_BIND t16
#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"

// texture/sampler tables, access this with the index from material struct
Texture2D UHTextureTable[] : register(t0, space1);
SamplerState UHSamplerTable[] : register(t0, space2);

// VB & IB data, access them with InstanceIndex()
StructuredBuffer<float2> UHUV0Table[] : register(t0, space3);
ByteAddressBuffer UHIndicesTable[] : register(t0, space4);

// there is no way to differentiate index type in DXR system for now, bind another indices type buffer
// 0 for 16-bit index, 1 for 32-bit index
ByteAddressBuffer UHIndicesType : register(t0, space5);

// hit group shader, shared used for all RT shaders
// attribute structure for feteching mesh data
struct Attribute
{
	float2 Bary;
};

float2 GetHitUV0(uint InstanceIndex, uint PrimIndex, Attribute Attr)
{
	StructuredBuffer<float2> UV0 = UHUV0Table[InstanceIndex];
	ByteAddressBuffer Indices = UHIndicesTable[InstanceIndex];
	int IndiceType = UHIndicesType.Load(InstanceIndex * 4);

	// get index data based on indice type, it can be 16 or 32 bit
	uint3 Index;
	if (IndiceType == 1)
	{
		uint IbStride = 4;
		Index[0] = Indices.Load(PrimIndex * 3 * IbStride);
		Index[1] = Indices.Load(PrimIndex * 3 * IbStride + IbStride);
		Index[2] = Indices.Load(PrimIndex * 3 * IbStride + IbStride * 2);
	}
	else
	{
		uint IbStride = 2;
		uint Offset = PrimIndex * 3 * IbStride;

		const uint DwordAlignedOffset = Offset & ~3;
		const uint2 Four16BitIndices = Indices.Load2(DwordAlignedOffset);

		// Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
		if (DwordAlignedOffset == Offset)
		{
			Index[0] = Four16BitIndices.x & 0xffff;
			Index[1] = (Four16BitIndices.x >> 16) & 0xffff;
			Index[2] = Four16BitIndices.y & 0xffff;
		}
		else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
		{
			Index[0] = (Four16BitIndices.x >> 16) & 0xffff;
			Index[1] = Four16BitIndices.y & 0xffff;
			Index[2] = (Four16BitIndices.y >> 16) & 0xffff;
		}
	}

	float2 OutUV = 0;

	// interpolate data according to barycentric coordinate
	OutUV = UV0[Index[0]] + Attr.Bary.x * (UV0[Index[1]] - UV0[Index[0]])
		+ Attr.Bary.y * (UV0[Index[2]] - UV0[Index[0]]);

	return OutUV;
}

[shader("closesthit")]
void RTDefaultClosestHit(inout UHDefaultPayload Payload, in Attribute Attr)
{
	Payload.HitT = RayTCurrent();
	Payload.HitInstance = InstanceIndex();
}

[shader("anyhit")]
void RTDefaultAnyHit(inout UHDefaultPayload Payload, in Attribute Attr)
{
	// fetch material with InstanceID, which is set to material buffer data index in C++ side
	UHMaterialConstants Mat = UHMaterials[InstanceID()];

	// on hardware that might have divergence issue when using index from structured buffer
	// NonUniformResourceIndex() is needed (usually happpens on AMD GPUs)

	if (Mat.OpacityTexIndex >= 0)
	{
		// cutoff test
		Texture2D OpacityTex = UHTextureTable[Mat.OpacityTexIndex];
		SamplerState OpacitySampler = UHSamplerTable[Mat.OpacitySamplerIndex];
		float2 UV0 = GetHitUV0(InstanceIndex(), PrimitiveIndex(), Attr);

		float Opacity = OpacityTex.SampleLevel(OpacitySampler, UV0, 0).r * Mat.DiffuseColor.a;
		if (Opacity < Mat.Cutoff)
		{
			// discard this hit if it's cut
			IgnoreHit();
		}
	}
}