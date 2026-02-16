#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"
#include "../Shaders/UHMaterialCommon.hlsli"
#include "../Shaders/RayTracing/UHRTCommon.hlsli"

// TLAS
RaytracingAccelerationStructure TLAS : register(t1);

// texture/sampler tables, access this with the index from material struct
// access via the code calculated by system
Texture2D UHTextureTable[] : register(t0, space1);
SamplerState UHSamplerTable[] : register(t0, space2);

// mesh instance data, access it with InstanceIndex() first, then use the info stored for other indexing or condition
StructuredBuffer<UHRendererInstance> UHRendererInstances : register(t0, space3);

// VB & IB data, access them with UHRendererInstance.MeshIndex
StructuredBuffer<float2> UHUV0Table[] : register(t0, space4);
StructuredBuffer<float3> UHNormalTable[] : register(t0, space5);
StructuredBuffer<float4> UHTangentTable[] : register(t0, space6);
ByteAddressBuffer UHIndicesTable[] : register(t0, space7);

// another descriptor array for matching, since Vulkan doesn't implement local descriptor yet, I need this to fetch data
// access via InstanceID()[0] first, the data will be filled by the systtem on C++ side
struct MaterialData
{
    uint Data[RT_MATERIALDATA_SLOT];
};
StructuredBuffer<MaterialData> UHMaterialDataTable[] : register(t0, space8);

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
    uint MaxReflectionBounce;
};

void UnpackMaterialData(MaterialData MatData, inout MaterialUsage Usages)
{
    Usages.Cutoff = asfloat(MatData.Data[0]);
    Usages.BlendMode = MatData.Data[1];
    Usages.MaterialFeature = MatData.Data[2];
    Usages.MaxReflectionBounce = MatData.Data[3];
}

// get material input, the simple version that has opacity only
UHMaterialInputs GetMaterialOpacity(float2 UV0, float MipLevel, out MaterialUsage Usages)
{
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    UnpackMaterialData(MatData, Usages);
	
	// TextureIndexStart in TextureNode.cpp decides where the first index of texture will start in MaterialData.Data[]

	// material input code will be generated in C++ side
	//%UHS_INPUT_OpacityOnly
}

// get only the bump normal from the material
UHMaterialInputs GetMaterialBumpNormal(float2 UV0, float MipLevel, out MaterialUsage Usages)
{
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    UnpackMaterialData(MatData, Usages);
    
    // material input code will be generated in C++ side
	//%UHS_INPUT_NormalOnly
}

// get material input fully
UHMaterialInputs GetMaterialInput(float2 UV0, float MipLevel, out MaterialUsage Usages)
{
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    UnpackMaterialData(MatData, Usages);
    
    // material input code will be generated in C++ side
	//%UHS_INPUT
}

UHMaterialInputs GetMaterialSmoothness(float2 UV0, float MipLevel, out MaterialUsage Usages)
{
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    UnpackMaterialData(MatData, Usages);
    
    // material input code will be generated in C++ side
	//%UHS_INPUT_SmoothnessOnly
}

uint3 GetIndex(in uint PrimIndex)
{
    UHRendererInstance RendererInstances = UHRendererInstances[InstanceIndex()];
    ByteAddressBuffer Indices = UHIndicesTable[RendererInstances.MeshIndex];
	
    // get index data based on indice type, it can be 16 or 32 bit
    uint3 Index;
    if (RendererInstances.IndiceType == 1)
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

float2 GetHitUV0(uint3 PrimIndices, Attribute Attr)
{
    UHRendererInstance RendererInstances = UHRendererInstances[InstanceIndex()];

    StructuredBuffer<float2> UV0 = UHUV0Table[RendererInstances.MeshIndex];
	float2 OutUV = 0;

	// interpolate data according to barycentric coordinate
    OutUV = UV0[PrimIndices[0]] + Attr.Bary.x * (UV0[PrimIndices[1]] - UV0[PrimIndices[0]])
		+ Attr.Bary.y * (UV0[PrimIndices[2]] - UV0[PrimIndices[0]]);

	return OutUV;
}

float3 GetHitVertexNormal(uint3 PrimIndices, Attribute Attr)
{
    UHRendererInstance RendererInstances = UHRendererInstances[InstanceIndex()];
    
    StructuredBuffer<float3> Normal = UHNormalTable[RendererInstances.MeshIndex];
    float3 OutNormal = float3(0, 0, 1);
    
    OutNormal = Normal[PrimIndices[0]] + Attr.Bary.x * (Normal[PrimIndices[1]] - Normal[PrimIndices[0]])
		+ Attr.Bary.y * (Normal[PrimIndices[2]] - Normal[PrimIndices[0]]);
    
    return OutNormal;
}

float4 GetHitTangent(uint3 PrimIndices, Attribute Attr)
{
    UHRendererInstance RendererInstances = UHRendererInstances[InstanceIndex()];
    
    StructuredBuffer<float4> Tangent = UHTangentTable[RendererInstances.MeshIndex];
    float4 OutTangent = float4(1, 0, 0, 1);
    
    OutTangent = Tangent[PrimIndices[0]] + Attr.Bary.x * (Tangent[PrimIndices[1]] - Tangent[PrimIndices[0]])
		+ Attr.Bary.y * (Tangent[PrimIndices[2]] - Tangent[PrimIndices[0]]);
    
    return OutTangent;
}

float3 CalculateBumpNormal(float3 InBump, float3 VertexNormal, in Attribute Attr, uint3 PrimIndices)
{
    bool bIsFrontFace = (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE);
    
    // tangent to world space
    float4 VertexTangent = GetHitTangent(PrimIndices, Attr);
    VertexTangent.xyz = mul(VertexTangent.xyz, (float3x3) ObjectToWorld3x4());
            
    float3 Binormal = cross(VertexNormal, VertexTangent.xyz) * VertexTangent.w;
            
    // build TBN matrix and transform the bump normal
    float3x3 WorldTBN = float3x3(VertexTangent.xyz, Binormal, VertexNormal);
    InBump = mul(InBump, WorldTBN);
    InBump *= (bIsFrontFace) ? 1 : -1;
    
    return InBump;
}

void CalculateMaterial(inout UHDefaultPayload Payload, float3 WorldPos, in Attribute Attr, bool bMergeDiffuseEmissive = false)
{    
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    bool bIsOpaque = MatData.Data[1] <= UH_ISMASKED;
	
	// fetch material data
    uint3 PrimIndices = GetIndex(PrimitiveIndex());
    float2 UV0 = GetHitUV0(PrimIndices, Attr);
    float MipLevel = Payload.MipLevel;
        
    // normal calculation, fetch the vertex normal
    bool bIsFrontFace = (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE);
    float3 VertexNormal = GetHitVertexNormal(PrimIndices, Attr);
        
    // for normal transform, inverse-transposed world matrix is needed to deal with non-uniform scaling
    // so use WorldToObject3x4 instead
    VertexNormal = normalize(mul(VertexNormal, (float3x3)WorldToObject3x4()));
    float3 FlippedVertexNormal = VertexNormal * ((bIsFrontFace) ? 1 : -1);
    
    float4 Diffuse = 0;
    float4 Specular = 0;
    float3 BumpNormal = 0;
    float3 Emissive = 0;
    float2 RefractOffset = 0;
    Payload.FresnelFactor = 1;
    
    {
		// get material data as like in BasePixelShader
        MaterialUsage Usage;
        UHMaterialInputs MaterialInput = GetMaterialInput(UV0, Payload.MipLevel, Usage);
        
        float3 BaseColor = MaterialInput.Diffuse;
        float Occlusion = MaterialInput.Occlusion;
        float Metallic = MaterialInput.Metallic;
        float Roughness = MaterialInput.Roughness;
        float Smoothness = 1.0f - Roughness;
        float SmoothnessSquare = Smoothness * Smoothness;

        BaseColor = BaseColor - BaseColor * Metallic;
        Diffuse = float4(saturate(BaseColor), Occlusion);
        
        BumpNormal = FlippedVertexNormal;
        bool bRefraction = Usage.MaterialFeature & UH_REFRACTION;
        if (Usage.MaterialFeature & UH_TANGENT_SPACE)
        {
            BumpNormal = MaterialInput.Normal;
            if (bRefraction)
            {
                RefractOffset = BumpNormal.xy * MaterialInput.Refraction;
            }
            
            BumpNormal = CalculateBumpNormal(BumpNormal, VertexNormal, Attr, PrimIndices);

        }
        else if (bRefraction)
        {
            // refraction without input bump
            float3 EyeVector = normalize(WorldPos - GCameraPos);
            float3 RefractRay = refract(EyeVector, BumpNormal, MaterialInput.Refraction);
            
            RefractOffset = (RefractRay.xy - EyeVector.xy) * 4.0f;
        }
        
        Specular.rgb = MaterialInput.Specular;
        Specular.rgb = ComputeSpecularColor(Specular.rgb, MaterialInput.Diffuse, Metallic);
        Specular.a = Smoothness;
        Payload.FresnelFactor = MaterialInput.FresnelFactor;
        
        Emissive = MaterialInput.Emissive;
    }
    
    Payload.HitDiffuse = bMergeDiffuseEmissive ? Diffuse + float4(Emissive, 0) : Diffuse;
    Payload.HitMaterialNormal = BumpNormal;
    Payload.HitVertexNormal = FlippedVertexNormal;
    Payload.HitSpecular = Specular;
    if (!bMergeDiffuseEmissive)
    {
        Payload.HitEmissive = Emissive;
    }
    if (!bIsOpaque)
    {
        Payload.HitRefractOffset = RefractOffset;
    }
    
    Payload.RayDir = WorldRayDirection();
}

[shader("closesthit")]
void RTDefaultClosestHit(inout UHDefaultPayload Payload, in Attribute Attr)
{
    // closest hit shader, only opaque objects will reach here as IgnoreHit() is used in any hit shader
    float PrevHitT = Payload.HitT;
    float PrevAlpha = Payload.HitAlpha;
    Payload.HitT = RayTCurrent();
    
    // cancel the flag of hit translucent if it's behind this opaque object
    if ((Payload.PayloadData & PAYLOAD_HITTRANSLUCENT) && Payload.HitT <= PrevHitT)
    {
        Payload.PayloadData &= ~uint(PAYLOAD_HITTRANSLUCENT);
    }
    
    UHDefaultPayload TransPayload;
    bool bHitTranslucent = (Payload.PayloadData & PAYLOAD_HITTRANSLUCENT) > 0;
    if (bHitTranslucent)
    {
        TransPayload.CopyFrom(Payload);
    }
    
    // set hit T, instance index and hit alpha
    Payload.HitInstanceIndex = InstanceIndex();
    Payload.HitAlpha = 1.0f;

    bool bIsReflection = (Payload.PayloadData & PAYLOAD_ISREFLECTION) > 0;
    bool bIsIndirectLight = (Payload.PayloadData & PAYLOAD_ISINDIRECTLIGHT) > 0;
    if (!bIsReflection && !bIsIndirectLight)
	{
		return;
	}
    
    UHBRANCH
    if (bIsReflection)
    {
        uint3 PrimIndices = GetIndex(PrimitiveIndex());
        float2 UV0 = GetHitUV0(PrimIndices, Attr);
        
        // check whether to shoot the multiple bounce reflection ray
        // shoot only when the CurrentRecursion < MaxReflectionRecursion and smoothnes is > GRTReflectionSmoothCutoff
        MaterialUsage MatUsage;
        UHMaterialInputs MaterialInput = GetMaterialSmoothness(UV0, Payload.MipLevel, MatUsage);
        float Smoothness = 1.0f - MaterialInput.Roughness;
    
        UHDefaultPayload BouncePayload = (UHDefaultPayload)0;
        UHBRANCH
        if (Payload.CurrentRecursion < min(GMaxReflectionRecursion, MatUsage.MaxReflectionBounce))
        {
            // setup the new payload, be sure to acculumate the CurrentRecursion!
            BouncePayload.MipLevel = Payload.MipLevel;
            BouncePayload.PayloadData |= PAYLOAD_ISREFLECTION;
            BouncePayload.CurrentRecursion = Payload.CurrentRecursion + 1;
        
            // fetch world normal to calculate the new reflect ray
            MaterialUsage Usages;
            MaterialInput = GetMaterialBumpNormal(UV0, Payload.MipLevel, Usages);
            bool bIsTangent = (Usages.MaterialFeature & UH_TANGENT_SPACE);
        
            float3 WorldNormal = GetHitVertexNormal(PrimIndices, Attr);
            WorldNormal = normalize(mul(WorldNormal, (float3x3) WorldToObject3x4()));
            float3 Normal = bIsTangent ? CalculateBumpNormal(MaterialInput.Normal, WorldNormal, Attr, PrimIndices) : WorldNormal;
        
            RayDesc BounceRay = (RayDesc) 0;
            BounceRay.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
            BounceRay.Direction = reflect(WorldRayDirection(), Normal);

            BounceRay.TMin = 0.1f;
            // lerp the TMax by the smoothness, can't just use smoothness as condition as it might be divergent 
            BounceRay.TMax = lerp(0, GRTReflectionRayTMax, Smoothness > GRTReflectionSmoothCutoff);
        
            TraceRay(TLAS, RAY_FLAG_NONE, 0xff, 0, 0, 0, BounceRay, BouncePayload);
        }
    
        // copy from BouncePayload if it hit something, only missed bounce ray or the first ray will proceed to material fetching
        if (BouncePayload.IsHit())
        {
            Payload.CopyFrom(BouncePayload);
            return;
        }
    }
    
    float3 WorldPos = WorldRayOrigin() + WorldRayDirection() * Payload.HitT;
    CalculateMaterial(Payload, WorldPos, Attr, bIsIndirectLight);
    
    if (bHitTranslucent)
    {
        Payload.HitDiffuse = lerp(Payload.HitDiffuse, TransPayload.HitDiffuse, PrevAlpha);
        Payload.HitMaterialNormal = lerp(Payload.HitMaterialNormal, TransPayload.HitMaterialNormal, PrevAlpha);
        Payload.HitVertexNormal = lerp(Payload.HitVertexNormal, TransPayload.HitVertexNormal, PrevAlpha);
        Payload.HitSpecular = lerp(Payload.HitSpecular, TransPayload.HitSpecular, PrevAlpha);
        Payload.HitEmissive = lerp(Payload.HitEmissive, TransPayload.HitEmissive, PrevAlpha);
        Payload.HitT = PrevHitT;
    }
}

[shader("anyhit")]
void RTDefaultAnyHit(inout UHDefaultPayload Payload, in Attribute Attr)
{
	// fetch material data and cutoff if it's alpha test
    float2 UV0 = GetHitUV0(GetIndex(PrimitiveIndex()), Attr);
	
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

        // evaluate whether to update cloest trancluent material
        float ThisHitT = RayTCurrent();
        bool bUpdateClosestTranslucentMaterial = false;
        
        UHBRANCH
        if (Payload.HitT > 0)
        {
            if (ThisHitT < Payload.HitT)
            {
                Payload.HitT = ThisHitT;
                bUpdateClosestTranslucentMaterial = true;
            }
        }
        else
        {
            Payload.HitT = ThisHitT;
            bUpdateClosestTranslucentMaterial = true;
        }
        
        // this tracing hits a translucent object
        Payload.PayloadData |= PAYLOAD_HITTRANSLUCENT;
        
        if (bUpdateClosestTranslucentMaterial)
        {
            // calc translucent material for reflection or indirect lighting
            bool bIsReflection = (Payload.PayloadData & PAYLOAD_ISREFLECTION) > 0;
            bool bIsIndirectLight = (Payload.PayloadData & PAYLOAD_ISINDIRECTLIGHT) > 0;
        
            if (bIsReflection || bIsIndirectLight)
            {
                // override the closest translucent material
                float3 WorldPos = WorldRayOrigin() + WorldRayDirection() * Payload.HitT;
                CalculateMaterial(Payload, WorldPos, Attr);
                Payload.HitAlpha = MaterialInput.Opacity;

                // set refraction flag
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

[shader("closesthit")]
void RTMinimalClosestHit(inout UHMinimalPayload Payload, in Attribute Attr)
{
    // simply output HitT and HitAlpha
    Payload.HitT = RayTCurrent();
    Payload.HitAlpha = 1.0f;
}

[shader("anyhit")]
void RTMinimalAnyHit(inout UHMinimalPayload Payload, in Attribute Attr)
{
    // fetch material data and cutoff if it's alpha test
    float2 UV0 = GetHitUV0(GetIndex(PrimitiveIndex()), Attr);
	
    MaterialUsage Usages = (MaterialUsage) 0;
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
        
        // select max alpha for the shadow pass
        Payload.HitAlpha = max(MaterialInput.Opacity, Payload.HitAlpha);
        IgnoreHit();
    }
}