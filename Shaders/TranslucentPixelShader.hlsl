#define UHDIRLIGHT_BIND t6
#define UHPOINTLIGHT_BIND t7
#define UHSPOTLIGHT_BIND t8
#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"
#include "../Shaders/UHLightCommon.hlsli"
#include "../Shaders/UHMaterialCommon.hlsli"

#define SH9_BIND t13
#include "../Shaders/UHSphericalHamonricCommon.hlsli"

Texture2D ScreenShadowTexture : register(t9);
Texture2D ScreenReflectionTexture : register(t10);
ByteAddressBuffer PointLightListTrans : register(t11);
ByteAddressBuffer SpotLightListTrans : register(t12);
TextureCube EnvCube : register(t14);

// texture/sampler tables for bindless rendering
Texture2D UHTextureTable[] : register(t0, space1);
SamplerState UHSamplerTable[] : register(t0, space2);

cbuffer PassConstant : register(UHMAT_BIND)
{
	//%UHS_CBUFFERDEFINE
}

UHMaterialInputs GetMaterialInput(float2 UV0)
{
	// material input code will be generated in C++ side
	//%UHS_INPUT
}

float4 TranslucentPS(VertexOutput Vin, bool bIsFrontFace : SV_IsFrontFace) : SV_Target
{
	// similar to BasePixelShader but it's forward rendering
	// so I have to calculate light here

	// fetch material input
	UHMaterialInputs MaterialInput = GetMaterialInput(Vin.UV0);
    SamplerState LinearClamppedSampler = UHSamplerTable[GLinearClampSamplerIndex];

	float3 BaseColor = MaterialInput.Diffuse;
	float Occlusion = MaterialInput.Occlusion;
	float Metallic = MaterialInput.Metallic;
	float Roughness = MaterialInput.Roughness;
    float Opacity = MaterialInput.Opacity;

    float Smoothness = 1.0f - Roughness;
    float SmoothnessSquare = Smoothness * Smoothness;
    
    float2 ScreenUV = Vin.Position.xy * GResolution.zw;
    float3 WorldPos = Vin.WorldPos;
    
    // Calc eye vector when necessary
    float3 EyeVector = 0;
    bool bEnvCubeEnabled = (GSystemRenderFeature & UH_ENV_CUBE);
    bool bIsTangentSpace = (GMaterialFeature & UH_TANGENT_SPACE);
    bool bRefraction = (GMaterialFeature & UH_REFRACTION);

    UHBRANCH
    if (bEnvCubeEnabled || bRefraction)
    {
        EyeVector = normalize(WorldPos - GCameraPos);
    }

    float3 BumpNormal = 0;
    float2 RefractOffset = 0;
#if TANGENT_SPACE
    BumpNormal = MaterialInput.Normal;
    
    // calc refract offset by roughness, it should look more blurry with high roughness
    UHBRANCH
    if (bRefraction)
    {
        RefractOffset = BumpNormal.xy * GResolution.zw * 100.0f;
    }
    
	// tangent to world space
    BumpNormal = mul(BumpNormal, Vin.WorldTBN) * ((bIsFrontFace) ? 1 : -1);
#else
    BumpNormal = normalize(Vin.Normal) * ((bIsFrontFace) ? 1 : -1);
    
    // calc refract offset by roughness, it should look more blurry with high roughness
    UHBRANCH
    if (bRefraction)
    {
        // refraction is used as index here
        float3 RefractRay = refract(EyeVector, BumpNormal, MaterialInput.Refraction);
        float EyeLength = length(WorldPos - GCameraPos);
        
        // calculate an offset and prevent it's distorting too much at long distance
        RefractOffset = (RefractRay.xy - EyeVector.xy) / max(EyeLength, 0.01f);
    }
#endif
    
    // Base Color PBR
    BaseColor = saturate(BaseColor - BaseColor * Metallic);

	// specular color and roughness
	float3 Specular = MaterialInput.Specular;
	Specular = ComputeSpecularColor(Specular, MaterialInput.Diffuse, Metallic);
    
    // Refraction, available with bump normal only
	UHBRANCH
    if (bRefraction)
    {
        // apply refract offset to UV, refraction parameter is used as scaling for bump normal
        float2 RefractUV = ScreenUV + RefractOffset * lerp(1, MaterialInput.Refraction, bIsTangentSpace);
        if (RefractUV.x != saturate(RefractUV.x) || RefractUV.y != saturate(RefractUV.y))
        {
            RefractUV = ScreenUV;
        }
    
        float3 OpaqueSceneColor = UHTextureTable[GOpaqueSceneTextureIndex].SampleLevel(LinearClamppedSampler, RefractUV, 0).rgb;
        float3 RefractionResult = OpaqueSceneColor * BaseColor;
        RefractionResult += MaterialInput.Emissive.rgb;
    
        // Early return and do not proceed to lighting, refraction should rely on the lit from scene color to prevent over bright.
        return float4(RefractionResult, Opacity);
    }
    
    // ------------------------------------------------------------------------------------------ reflection
    float3 IndirectSpecular = 0;
    float SpecFade = SmoothnessSquare;
    float SpecMip = (1.0f - SpecFade) * GEnvCubeMipMapCount;
    
    // calc fresnel
    float3 R = reflect(EyeVector, BumpNormal);
    float NdotV = abs(dot(BumpNormal, -EyeVector));
    float3 Fresnel = SchlickFresnel(Specular, lerp(0, NdotV, MaterialInput.FresnelFactor));
    
	UHBRANCH
    if (bEnvCubeEnabled)
    {
        IndirectSpecular = EnvCube.SampleLevel(UHSamplerTable[GSkyCubeSamplerIndex], R, SpecMip).rgb * GAmbientSky;
    }
    
    float4 DynamicReflection = ScreenReflectionTexture.SampleLevel(UHSamplerTable[GLinearClampSamplerIndex], ScreenUV, SpecMip);
    IndirectSpecular = lerp(IndirectSpecular, DynamicReflection.rgb, DynamicReflection.a);
    IndirectSpecular *= SpecFade * Fresnel * Occlusion * GFinalReflectionStrength;

	// ------------------------------------------------------------------------------------------ dir lights accumulation
	// sample shadows
    float ShadowMask = ScreenShadowTexture.Sample(LinearClamppedSampler, ScreenUV).r;
    // get noise
    float AttenNoise = GetAttenuationNoise(Vin.Position.xy);
    
	float3 Result = 0;
    
	// light calculation, be sure to normalize normal vector before using it	
    // light functions will also fill necessary data
    UHLightInfo LightInfo;
    LightInfo.Diffuse = BaseColor;
    LightInfo.Specular = float4(Specular, Roughness);
    LightInfo.Normal = normalize(BumpNormal);
    LightInfo.WorldPos = WorldPos;
    LightInfo.ShadowMask = ShadowMask;
    LightInfo.AttenNoise = AttenNoise;
	
	for (uint Ldx = 0; Ldx < GNumDirLights; Ldx++)
	{
        Result += CalculateDirLight(UHDirLights[Ldx], LightInfo);
    }
	
	// ------------------------------------------------------------------------------------------ point lights accumulation
	// point lights accumulation, fetch the tile index here, note that the system culls at half resolution
    uint TileX = CoordToTileX(uint(Vin.Position.x + 0.5f));
    uint TileY = CoordToTileY(uint(Vin.Position.y + 0.5f));
    uint TileIndex = TileX + TileY * GLightTileCountX;
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint PointLightCount = PointLightListTrans.Load(TileOffset);
    TileOffset += 4;
    
    for (Ldx = 0; Ldx < PointLightCount; Ldx++)
    {
        uint PointLightIdx = PointLightListTrans.Load(TileOffset);
        TileOffset += 4;
       
        UHPointLight PointLight = UHPointLights[PointLightIdx];
        Result += CalculatePointLight(PointLight, LightInfo);
    }
	
	// ------------------------------------------------------------------------------------------ spot lights accumulation
	// similar as the point light but giving it angle attenuation as well
    TileOffset = GetSpotLightOffset(TileIndex);
    uint SpotLightCount = SpotLightListTrans.Load(TileOffset);
    TileOffset += 4;
    
    UHLOOP
    for (Ldx = 0; Ldx < SpotLightCount; Ldx++)
    {
        uint SpotLightIdx = SpotLightListTrans.Load(TileOffset);
        TileOffset += 4;
        
        UHSpotLight SpotLight = UHSpotLights[SpotLightIdx];
        Result += CalculateSpotLight(SpotLight, LightInfo);
    }

	// indirect light accumulation
	Result += ShadeSH9(BaseColor, float4(BumpNormal, 1.0f), Occlusion);
	Result += MaterialInput.Emissive.rgb + IndirectSpecular;

	// output result with opacity
	return float4(Result, Opacity);
}