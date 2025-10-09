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

float2 GetHitUV0(uint PrimIndex, Attribute Attr)
{
    UHRendererInstance RendererInstances = UHRendererInstances[InstanceIndex()];
    uint3 Index = GetIndex(PrimIndex);

    StructuredBuffer<float2> UV0 = UHUV0Table[RendererInstances.MeshIndex];
	float2 OutUV = 0;

	// interpolate data according to barycentric coordinate
	OutUV = UV0[Index[0]] + Attr.Bary.x * (UV0[Index[1]] - UV0[Index[0]])
		+ Attr.Bary.y * (UV0[Index[2]] - UV0[Index[0]]);

	return OutUV;
}

float3 GetHitNormal(uint PrimIndex, Attribute Attr)
{
    UHRendererInstance RendererInstances = UHRendererInstances[InstanceIndex()];
    uint3 Index = GetIndex(PrimIndex);
    
    StructuredBuffer<float3> Normal = UHNormalTable[RendererInstances.MeshIndex];
    float3 OutNormal = float3(0, 0, 1);
    
    OutNormal = Normal[Index[0]] + Attr.Bary.x * (Normal[Index[1]] - Normal[Index[0]])
		+ Attr.Bary.y * (Normal[Index[2]] - Normal[Index[0]]);
    
    return OutNormal;
}

float4 GetHitTangent(uint PrimIndex, Attribute Attr)
{
    UHRendererInstance RendererInstances = UHRendererInstances[InstanceIndex()];
    uint3 Index = GetIndex(PrimIndex);
    
    StructuredBuffer<float4> Tangent = UHTangentTable[RendererInstances.MeshIndex];
    float4 OutTangent = float4(1, 0, 0, 1);
    
    OutTangent = Tangent[Index[0]] + Attr.Bary.x * (Tangent[Index[1]] - Tangent[Index[0]])
		+ Attr.Bary.y * (Tangent[Index[2]] - Tangent[Index[0]]);
    
    return OutTangent;
}

float3 CalculateBumpNormal(float3 InBump, float3 VertexNormal, in Attribute Attr)
{
    bool bIsFrontFace = (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE);
    
    // tangent to world space
    float4 VertexTangent = GetHitTangent(PrimitiveIndex(), Attr);
    VertexTangent.xyz = mul(VertexTangent.xyz, (float3x3) ObjectToWorld3x4());
            
    float3 Binormal = cross(VertexNormal, VertexTangent.xyz) * VertexTangent.w;
            
    // build TBN matrix and transform the bump normal
    float3x3 WorldTBN = float3x3(VertexTangent.xyz, Binormal, VertexNormal);
    InBump = mul(InBump, WorldTBN);
    InBump *= (bIsFrontFace) ? 1 : -1;
    
    return InBump;
}

void CalculateReflectionMaterial(inout UHDefaultPayload Payload, float3 WorldPos, in Attribute Attr)
{
    MaterialData MatData = UHMaterialDataTable[InstanceID()][0];
    bool bIsOpaque = MatData.Data[1] <= UH_ISMASKED;
    
    float4 ClipPos = mul(float4(WorldPos, 1.0f), GViewProj);
    ClipPos /= ClipPos.w;
	
    float2 ScreenUV = ClipPos.xy * 0.5f + 0.5f;
    bool bInsideScreen = IsUVInsideScreen(ScreenUV) && (ClipPos.z > GNearPlane);
	
	// fetch material data
    float2 UV0 = GetHitUV0(PrimitiveIndex(), Attr);
    float MipLevel = Payload.MipLevel;
    SamplerState PointClampSampler = UHSamplerTable[GPointClampSamplerIndex];
        
    // normal calculation, fetch the vertex normal
    bool bIsFrontFace = (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE);
    float3 VertexNormal = GetHitNormal(PrimitiveIndex(), Attr);
        
    // for normal transform, inverse-transposed world matrix is needed to deal with non-uniform scaling
    // so use WorldToObject3x4 instead
    VertexNormal = normalize(mul(VertexNormal, (float3x3)WorldToObject3x4()));
    float3 FlippedVertexNormal = VertexNormal * ((bIsFrontFace) ? 1 : -1);
    
    float4 Diffuse = 0;
    float4 Specular = 0;
    float3 BumpNormal = 0;
    float3 Emissive = 0;
    float2 RefractOffset = 0;
    Payload.PackedData0.a = 1;
    
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
            
            BumpNormal = CalculateBumpNormal(BumpNormal, VertexNormal, Attr);

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
        Payload.PackedData0.a = MaterialInput.FresnelFactor;
        
        Emissive = MaterialInput.Emissive;
    }
    
    Payload.HitDiffuse = Diffuse;
    Payload.HitNormal = BumpNormal;
    Payload.HitWorldNormal = FlippedVertexNormal;
    Payload.HitSpecular = Specular;
    Payload.HitEmissive = Emissive;
    Payload.HitScreenUV = ScreenUV;
    if (!bIsOpaque)
    {
        Payload.HitRefractOffset = RefractOffset;
    }
    
    Payload.IsInsideScreen = bInsideScreen;
    Payload.PackedData0.xyz = WorldPos;
    
    // ray dir
    Payload.RayDir = WorldRayDirection();
}

[shader("closesthit")]
void RTDefaultClosestHit(inout UHDefaultPayload Payload, in Attribute Attr)
{
    // closest hit shader, only opaque objects will reach here
    float PrevHitT = Payload.HitT;
	Payload.HitT = RayTCurrent();
    Payload.HitInstanceIndex = InstanceIndex();

	// set alpha to 1 and cancel the flag of hit translucent if it's behind this opaque object
    Payload.HitAlpha = 1.0f;
    if ((Payload.PayloadData & PAYLOAD_HITTRANSLUCENT) && Payload.HitT <= PrevHitT)
    {
        Payload.PayloadData &= ~uint(PAYLOAD_HITTRANSLUCENT);
    }

    if ((Payload.PayloadData & PAYLOAD_ISREFLECTION) == 0)
	{
		return;
	}
    
    // check whether to shoot the multiple bounce ray
    // shoot only when the CurrentRecursion < MaxReflectionRecursion and smoothnes is > GRTReflectionSmoothCutoff
    float2 UV0 = GetHitUV0(PrimitiveIndex(), Attr);
    MaterialUsage MatUsage;
    UHMaterialInputs MaterialInput = GetMaterialSmoothness(UV0, Payload.MipLevel, MatUsage);
    float Smoothness = 1.0f - MaterialInput.Roughness;
    
    UHDefaultPayload BouncePayload = (UHDefaultPayload) 0;
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
        
        float3 WorldNormal = GetHitNormal(PrimitiveIndex(), Attr);
        WorldNormal = normalize(mul(WorldNormal, (float3x3) WorldToObject3x4()));
        float3 Normal = bIsTangent ? CalculateBumpNormal(MaterialInput.Normal, WorldNormal, Attr) : WorldNormal;
        
        RayDesc BounceRay = (RayDesc)0;
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
	
	// world pos to screen uv conversion
    float3 WorldPos = WorldRayOrigin() + WorldRayDirection() * Payload.HitT;
    CalculateReflectionMaterial(Payload, WorldPos, Attr);
}

[shader("anyhit")]
void RTDefaultAnyHit(inout UHDefaultPayload Payload, in Attribute Attr)
{
	// fetch material data and cutoff if it's alpha test
	float2 UV0 = GetHitUV0(PrimitiveIndex(), Attr);
	
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
                Payload.HitWorldNormalTrans = TransPayload.HitWorldNormal;
                Payload.HitSpecularTrans = TransPayload.HitSpecular;
                Payload.HitEmissiveTrans = float4(TransPayload.HitEmissive, MaterialInput.Opacity);
                Payload.HitScreenUVTrans = TransPayload.HitScreenUV;
                Payload.HitWorldPosTrans = WorldPos;
                Payload.HitRefractOffset = TransPayload.HitRefractOffset;
                Payload.IsInsideScreen = TransPayload.IsInsideScreen;
                Payload.HitInstanceIndex = InstanceIndex();
                Payload.PackedData0 = TransPayload.PackedData0;
                Payload.RayDir = TransPayload.RayDir;
                
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