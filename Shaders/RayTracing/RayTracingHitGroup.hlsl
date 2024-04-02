#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"
#include "../Shaders/UHMaterialCommon.hlsli"
#include "../Shaders/RayTracing/UHRTCommon.hlsli"

// texture/sampler tables, access this with the index from material struct
// access via the code calculated by system
Texture2D UHTextureTable[] : register(t0, space1);
SamplerState UHSamplerTable[] : register(t0, space2);

// VB & IB data, access them with InstanceIndex()
StructuredBuffer<float2> UHUV0Table[] : register(t0, space3);
StructuredBuffer<float3> UHNormalTable[] : register(t0, space4);
StructuredBuffer<float4> UHTangentTable[] : register(t0, space5);
ByteAddressBuffer UHIndicesTable[] : register(t0, space6);

// there is no way to differentiate index type in DXR system for now, bind another indices type buffer
// 0 for 16-bit index, 1 for 32-bit index, access via InstanceIndex()
ByteAddressBuffer UHIndicesType : register(t0, space7);

// another descriptor array for matching, since Vulkan doesn't implement local descriptor yet, I need this to fetch data
// access via InstanceID()[0] first, the data will be filled by the systtem on C++ side
// max number of data member: 128 scalars for now
struct MaterialData
{
    uint Data[128];
};
StructuredBuffer<MaterialData> UHMaterialDataTable[] : register(t0, space8);

// opaque buffers to lookup, so it might be able to reduce a few instructions in reflection calculation.
Texture2D OpaqueBuffers[4] : register(t0, space9);

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
    uint MaterialFeature;
};

// get material input, the simple version that has opacity only
UHMaterialInputs GetMaterialOpacity(float2 UV0, float MipLevel, out MaterialUsage Usages)
{
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    Usages.Cutoff = asfloat(MatData.Data[0]);
    Usages.BlendMode = MatData.Data[1];
    Usages.MaterialFeature = MatData.Data[2];
	
	// TextureIndexStart in TextureNode.cpp decides where the first index of texture will start in MaterialData.Data[]

	// material input code will be generated in C++ side
	//%UHS_INPUT_OpacityOnly
}

// get only the bump normal from the material
UHMaterialInputs GetMaterialBumpNormal(float2 UV0, float MipLevel)
{
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    
    // material input code will be generated in C++ side
	//%UHS_INPUT_NormalOnly
}

// get only the emissive from the material
UHMaterialInputs GetMaterialEmissive(float2 UV0, float MipLevel)
{
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    
    // material input code will be generated in C++ side
	//%UHS_INPUT_EmissiveOnly
}

// get material input fully
UHMaterialInputs GetMaterialInput(float2 UV0, float MipLevel, out MaterialUsage Usages)
{
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    Usages.Cutoff = asfloat(MatData.Data[0]);
    Usages.BlendMode = MatData.Data[1];
    Usages.MaterialFeature = MatData.Data[2];
    
    // material input code will be generated in C++ side
	//%UHS_INPUT
}

uint3 GetIndex(in uint InstanceIndex, in uint PrimIndex)
{
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
	
	return Index;
}

float2 GetHitUV0(uint InstanceIndex, uint PrimIndex, Attribute Attr)
{
    uint3 Index = GetIndex(InstanceIndex, PrimIndex);

    StructuredBuffer<float2> UV0 = UHUV0Table[InstanceIndex];
	float2 OutUV = 0;

	// interpolate data according to barycentric coordinate
	OutUV = UV0[Index[0]] + Attr.Bary.x * (UV0[Index[1]] - UV0[Index[0]])
		+ Attr.Bary.y * (UV0[Index[2]] - UV0[Index[0]]);

	return OutUV;
}

float3 GetHitNormal(uint InstanceIndex, uint PrimIndex, Attribute Attr)
{
    uint3 Index = GetIndex(InstanceIndex, PrimIndex);
    
    StructuredBuffer<float3> Normal = UHNormalTable[InstanceIndex];
    float3 OutNormal = float3(0, 0, 1);
    
    OutNormal = Normal[Index[0]] + Attr.Bary.x * (Normal[Index[1]] - Normal[Index[0]])
		+ Attr.Bary.y * (Normal[Index[2]] - Normal[Index[0]]);
    
    return OutNormal;
}

float4 GetHitTangent(uint InstanceIndex, uint PrimIndex, Attribute Attr)
{
    uint3 Index = GetIndex(InstanceIndex, PrimIndex);
    
    StructuredBuffer<float4> Tangent = UHTangentTable[InstanceIndex];
    float4 OutTangent = float4(1, 0, 0, 1);
    
    OutTangent = Tangent[Index[0]] + Attr.Bary.x * (Tangent[Index[1]] - Tangent[Index[0]])
		+ Attr.Bary.y * (Tangent[Index[2]] - Tangent[Index[0]]);
    
    return OutTangent;
}

void CalculateReflectionMaterial(inout UHDefaultPayload Payload, float3 WorldPos, in Attribute Attr)
{
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    bool bIsOpaque = MatData.Data[1] <= UH_ISMASKED;
    
    float4 ClipPos = mul(float4(WorldPos, 1.0f), GViewProj);
    ClipPos /= ClipPos.w;
	
    float2 ScreenUV = ClipPos.xy * 0.5f + 0.5f;
    bool bInsideScreen = IsUVInsideScreen(ScreenUV);
	
	// fetch material data
    float2 UV0 = GetHitUV0(InstanceIndex(), PrimitiveIndex(), Attr);
    float MipLevel = Payload.MipLevel;
    SamplerState PointClampSampler = UHSamplerTable[GPointClampSamplerIndex];
        
    // normal calculation, fetch the vertex normal
    bool bIsFrontFace = (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE);
    float3 VertexNormal = GetHitNormal(InstanceIndex(), PrimitiveIndex(), Attr);
        
    // for normal transform, inverse-transposed world matrix is needed to deal with non-uniform scaling
    // so use WorldToObject3x4 instead
    VertexNormal = normalize(mul(VertexNormal, (float3x3)WorldToObject3x4()));
    float3 FlippedVertexNormal = VertexNormal * ((bIsFrontFace) ? 1 : -1);
    
    // fetch vertex normal stored in GBuffer
    float4 GBufferVertexNormal = OpaqueBuffers[4].SampleLevel(PointClampSampler, ScreenUV, 0);
    GBufferVertexNormal.xyz = DecodeNormal(GBufferVertexNormal.xyz);
    
    float4 Diffuse = 0;
    float4 Specular = 0;
    float3 BumpNormal = 0;
    float3 Emissive = 0;
    float2 RefractScale = 1;
    float Refraction = 0;
    
    // evaluate if it can reuse GBuffer info
    bool bCanFetchScreenInfo = bIsOpaque && bInsideScreen && (length(GBufferVertexNormal.xyz - FlippedVertexNormal) < 0.01f);
    
    UHBRANCH
    if (bCanFetchScreenInfo)
    {
		// when the hit position is inside screen and the vertex normal are very close
        // lookup material data from Opaque GBuffer instead, this is the opaque-only optimization for preventing the evaluation of a complex material again
        Diffuse = OpaqueBuffers[0].SampleLevel(PointClampSampler, ScreenUV, 0);
        BumpNormal = DecodeNormal(OpaqueBuffers[1].SampleLevel(PointClampSampler, ScreenUV, 0).xyz);
        Specular = OpaqueBuffers[2].SampleLevel(PointClampSampler, ScreenUV, 0);
        
        // the emissive still needs to be calculated from material
        UHMaterialInputs MaterialInput = GetMaterialEmissive(UV0, Payload.MipLevel);
        Emissive = MaterialInput.Emissive;
    }
    else
    {
		// otherwise, fully get material data as like in BasePixelShader
        MaterialUsage Usage;
        UHMaterialInputs MaterialInput = GetMaterialInput(UV0, Payload.MipLevel, Usage);
        
        float3 BaseColor = MaterialInput.Diffuse;
        float Occlusion = MaterialInput.Occlusion;
        float Metallic = MaterialInput.Metallic;
        float Roughness = MaterialInput.Roughness;
        float Smoothness = 1.0f - Roughness;

        BaseColor = BaseColor - BaseColor * Metallic;
        Diffuse = float4(saturate(BaseColor), Occlusion);
        
        BumpNormal = FlippedVertexNormal;
        if (Usage.MaterialFeature & UH_TANGENT_SPACE)
        {
            BumpNormal = MaterialInput.Normal;
            RefractScale *= BumpNormal.xy;
            
		    // tangent to world space
            float4 VertexTangent = GetHitTangent(InstanceIndex(), PrimitiveIndex(), Attr);
            VertexTangent.xyz = mul(VertexTangent.xyz, (float3x3)ObjectToWorld3x4());
            
            float3 Binormal = cross(VertexNormal, VertexTangent.xyz) * VertexTangent.w;
            
            // build TBN matrix and transform the bump normal
            float3x3 WorldTBN = float3x3(VertexTangent.xyz, Binormal, VertexNormal);
            BumpNormal = mul(BumpNormal, WorldTBN);
            BumpNormal *= (bIsFrontFace) ? 1 : -1;
        }
        
        Specular.rgb = MaterialInput.Specular;
        Specular.rgb = ComputeSpecularColor(Specular.rgb, MaterialInput.Diffuse, Metallic);
        Specular.a = Smoothness;
        
        Emissive = MaterialInput.Emissive;
        Refraction = MaterialInput.Refraction;
    }
    
    Payload.HitDiffuse = Diffuse;
    Payload.HitNormal = BumpNormal;
    Payload.HitSpecular = Specular;
    Payload.HitEmissive = Emissive;
    Payload.HitScreenUV = ScreenUV;
    if (!bIsOpaque)
    {
        Payload.HitRefractScale = RefractScale;
        Payload.HitRefraction = Refraction;
    }
    Payload.IsInsideScreen = bInsideScreen;
}

[shader("closesthit")]
void RTDefaultClosestHit(inout UHDefaultPayload Payload, in Attribute Attr)
{
    float PrevHitT = Payload.HitT;
	Payload.HitT = RayTCurrent();
	
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    bool bIsOpaque = MatData.Data[1] <= UH_ISMASKED;

	// set alpha to 1 and cancel the flag of hit translucent if the closest one is opaque
    if (bIsOpaque)
    {
        Payload.HitAlpha = 1.0f;
        
        // see if we need to cancel the hit translucent flag, in case the translucent object behind a object is traced
        if ((Payload.PayloadData & PAYLOAD_HITTRANSLUCENT) && Payload.HitT <= PrevHitT)
        {
            Payload.PayloadData &= ~PAYLOAD_HITTRANSLUCENT;
        }
    }
	
	// following calculation is for reflection
    // also early out for translucent as it's processed in any hit shader
    if ((Payload.PayloadData & PAYLOAD_ISREFLECTION) == 0 || !bIsOpaque)
	{
		return;
	}
	
	// world pos to screen uv conversion
    float3 WorldPos = WorldRayOrigin() + WorldRayDirection() * Payload.HitT;
    CalculateReflectionMaterial(Payload, WorldPos, Attr);
}

[shader("anyhit")]
void RTDefaultAnyHit(inout UHDefaultPayload Payload, in Attribute Attr)
{
	// fetch material data and cutoff if it's alpha test
	float2 UV0 = GetHitUV0(InstanceIndex(), PrimitiveIndex(), Attr);
	
    MaterialUsage Usages = (MaterialUsage)0;
    UHMaterialInputs MaterialInput = GetMaterialOpacity(UV0, Payload.MipLevel, Usages);

    if (Usages.BlendMode == UH_ISMASKED && MaterialInput.Opacity < Usages.Cutoff)
	{
		// discard this hit if it's alpha testing
		IgnoreHit();
		return;
	}

    if (Usages.BlendMode > UH_ISMASKED)
    {
        // The same rule as in MotionPixelShader.hlsl, not worth a update for 0 alpha pixel
        if (MaterialInput.Opacity < 0.001f)
        {
            IgnoreHit();
            return;
        }
        
		// for translucent object, evaludate the hit alpha and store the HitT data
        Payload.HitAlpha = max(MaterialInput.Opacity, Payload.HitAlpha);
        float ThisHitT = RayTCurrent();
        bool bUpdateClosestTranslucent = false;
        
        UHBRANCH
        if (Payload.HitT > 0)
        {
            if (ThisHitT < Payload.HitT)
            {
                Payload.HitT = ThisHitT;
                bUpdateClosestTranslucent = true;
            }
        }
        else
        {
            Payload.HitT = ThisHitT;
            bUpdateClosestTranslucent = true;
        }
        
        // this tracing hits a translucent object
        Payload.PayloadData |= PAYLOAD_HITTRANSLUCENT;
        
        if (bUpdateClosestTranslucent)
        {
            // calc translucent material for reflection
            if ((Payload.PayloadData & PAYLOAD_ISREFLECTION))
            {
                UHDefaultPayload TransPayload = Payload;
                float3 WorldPos = WorldRayOrigin() + WorldRayDirection() * Payload.HitT;
                CalculateReflectionMaterial(TransPayload, WorldPos, Attr);
                    
                Payload.HitDiffuseTrans = TransPayload.HitDiffuse;
                Payload.HitNormalTrans = TransPayload.HitNormal;
                Payload.HitSpecularTrans = TransPayload.HitSpecular;
                Payload.HitEmissiveTrans = float4(TransPayload.HitEmissive, MaterialInput.Opacity);
                Payload.HitScreenUVTrans = TransPayload.HitScreenUV;
                Payload.HitWorldPosTrans = WorldPos;
                Payload.HitRefractScale = TransPayload.HitRefractScale;
                Payload.HitRefraction = TransPayload.HitRefraction;
                Payload.IsInsideScreen = TransPayload.IsInsideScreen;
                
                if (Usages.MaterialFeature & UH_REFRACTION)
                {
                    Payload.PayloadData |= PAYLOAD_HITREFRACTION;
                }
            }
        }
        
        // do not accept the hit and let the ray continue
        IgnoreHit();
    }
}