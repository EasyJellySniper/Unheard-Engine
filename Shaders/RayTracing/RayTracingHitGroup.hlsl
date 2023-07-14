#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/RayTracing/UHRTCommon.hlsli"

// texture/sampler tables, access this with the index from material struct
// access via the code calculated by system
Texture2D UHTextureTable[] : register(t0, space1);
SamplerState UHSamplerTable[] : register(t0, space2);

// VB & IB data, access them with InstanceIndex()
StructuredBuffer<float2> UHUV0Table[] : register(t0, space3);
ByteAddressBuffer UHIndicesTable[] : register(t0, space4);

// there is no way to differentiate index type in DXR system for now, bind another indices type buffer
// 0 for 16-bit index, 1 for 32-bit index, access via InstanceIndex()
ByteAddressBuffer UHIndicesType : register(t0, space5);

// another 2D index array for matching, since Vulkan doesn't implement local descriptor yet, I need this to fetch data
// each texture node takes one slot, and the value of Index will be updated in C++ side
// access via InstanceID() first, then the index calculated by graph system
struct MaterialData
{
	int TextureIndex;
	int SamplerIndex;
	float Cutoff;
	float Padding;
};
StructuredBuffer<MaterialData> UHMaterialDataTable[] : register(t0, space6);

// hit group shader, shared used for all RT shaders
// attribute structure for feteching mesh data
struct Attribute
{
	float2 Bary;
};

// get material input, the simple version that has opacity only
UHMaterialInputs GetMaterialInputSimple(float2 UV0, float MipLevel, out float Cutoff)
{
	Cutoff = UHMaterialDataTable[InstanceID()][0].Cutoff;

	// material input code will be generated in C++ side
	//%UHS_INPUT_Simple
	return (UHMaterialInputs)0;
}

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
#if !WITH_TRANSLUCENT
	// set alpha to 1 if it's opaque
	Payload.HitAlpha = 1.0f;
#endif
}

[shader("anyhit")]
void RTDefaultAnyHit(inout UHDefaultPayload Payload, in Attribute Attr)
{
	// fetch material data and cutoff if it's alpha test
	float2 UV0 = GetHitUV0(InstanceIndex(), PrimitiveIndex(), Attr);
	float Cutoff;
	UHMaterialInputs MaterialInput = GetMaterialInputSimple(UV0, Payload.MipLevel, Cutoff);

#if WITH_ALPHATEST
	if (MaterialInput.Opacity < Cutoff)
	{
		// discard this hit if it's alpha testing
		IgnoreHit();
		return;
	}
#endif

#if WITH_TRANSLUCENT
	// for translucent object, evaludate the hit alpha and store the HitT data
	Payload.HitT = max(Payload.HitT, RayTCurrent());
	Payload.HitAlpha = max(MaterialInput.Opacity, Payload.HitAlpha);
	IgnoreHit();
#endif
}