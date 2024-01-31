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

// another descriptor array for matching, since Vulkan doesn't implement local descriptor yet, I need this to fetch data
// access via InstanceID()[0] first, the data will be filled by the systtem on C++ side
// max number of data member: 128 scalars for now
struct MaterialData
{
    uint Data[128];
};
StructuredBuffer<MaterialData> UHMaterialDataTable[] : register(t0, space6);

// hit group shader, shared used for all RT shaders
// attribute structure for feteching mesh data
struct Attribute
{
	float2 Bary;
};

struct MaterialUsage
{
    float Cutoff;
    uint BlendMode;
};

// get material input, the simple version that has opacity only
UHMaterialInputs GetMaterialInputSimple(float2 UV0, float MipLevel, out MaterialUsage Usages)
{
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    Usages.Cutoff = asfloat(MatData.Data[0]);
    Usages.BlendMode = MatData.Data[1];
	
	// TextureIndexStart in TextureNode.cpp decides where the first index of texture will start in MaterialData.Data[]

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
	
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];

	// set alpha to 1 if it's opaque
    if (MatData.Data[1] <= UH_ISMASKED)
    {
        Payload.HitAlpha = 1.0f;
    }
}

[shader("anyhit")]
void RTDefaultAnyHit(inout UHDefaultPayload Payload, in Attribute Attr)
{
	// fetch material data and cutoff if it's alpha test
	float2 UV0 = GetHitUV0(InstanceIndex(), PrimitiveIndex(), Attr);
	
    MaterialUsage Usages = (MaterialUsage)0;
    UHMaterialInputs MaterialInput = GetMaterialInputSimple(UV0, Payload.MipLevel, Usages);

    if (Usages.BlendMode == UH_ISMASKED && MaterialInput.Opacity < Usages.Cutoff)
	{
		// discard this hit if it's alpha testing
		IgnoreHit();
		return;
	}

    if (Usages.BlendMode > UH_ISMASKED)
    {
		// for translucent object, evaludate the hit alpha and store the HitT data
        Payload.HitT = max(Payload.HitT, RayTCurrent());
        Payload.HitAlpha = max(MaterialInput.Opacity, Payload.HitAlpha);
        IgnoreHit();
    }
}