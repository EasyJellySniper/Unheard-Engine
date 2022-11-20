// descriptor binding are shared in Raytracing States
// I've not found the similar usage to "Local Root Signature" of D3D12, so just make sure resource slots won't overlap with other resource
#define UHMAT_BIND t16
#include "../UHInputs.hlsli"
#include "UHRTCommon.hlsli"

// texture/sampler tables, access this with the index from material struct
Texture2D UHTextureTable[] : register(t0, space1);
SamplerState UHSamplerTable[] : register(t0, space2);

// VB & IB data, access them with InstanceIndex()
StructuredBuffer<VertexInput> UHVertexTable[] : register(t0, space3);
ByteAddressBuffer UHIndicesTable[] : register(t0, space4);

// hit group shader, shared used for all RT shaders
// attribute structure for feteching mesh data
struct Attribute
{
	float2 Bary;
};

VertexInput GetHitVertex(uint InstanceIndex, uint PrimIndex, Attribute Attr)
{
	StructuredBuffer<VertexInput> Vertex = UHVertexTable[InstanceIndex];
	ByteAddressBuffer Indices = UHIndicesTable[InstanceIndex];

	// for now system uses 32-bit index, if 16-bit index is used in the future, some bit shift operation is needed
	uint IbStride = 4;
	uint3 Index;
	Index[0] = Indices.Load(PrimIndex * 3 * IbStride);
	Index[1] = Indices.Load(PrimIndex * 3 * IbStride + IbStride);
	Index[2] = Indices.Load(PrimIndex * 3 * IbStride + IbStride * 2);

	VertexInput Input = (VertexInput)0;

	// interpolate data according to barycentric coordinate
	Input.UV0 = Vertex[Index[0]].UV0 + Attr.Bary.x * (Vertex[Index[1]].UV0 - Vertex[Index[0]].UV0)
		+ Attr.Bary.y * (Vertex[Index[2]].UV0 - Vertex[Index[0]].UV0);

	return Input;
}

[shader("closesthit")]
void RTDefaultClosestHit(inout UHDefaultPayload Payload, in Attribute Attr)
{
	Payload.HitT = RayTCurrent();
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
		VertexInput Vin = GetHitVertex(InstanceIndex(), PrimitiveIndex(), Attr);

		float Opacity = OpacityTex.SampleLevel(OpacitySampler, Vin.UV0, 0).r * Mat.DiffuseColor.a;
		if (Opacity < Mat.Cutoff)
		{
			// discard this hit if it's cut
			IgnoreHit();
		}
	}
}