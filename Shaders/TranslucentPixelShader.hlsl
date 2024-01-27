#define UHDIRLIGHT_BIND t6
#define UHPOINTLIGHT_BIND t7
#define UHSPOTLIGHT_BIND t8
#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"
#include "../Shaders/UHLightCommon.hlsli"

#define SH9_BIND t12
#include "../Shaders/UHSphericalHamonricCommon.hlsli"

Texture2D RTShadow : register(t9);
ByteAddressBuffer PointLightListTrans : register(t10);
ByteAddressBuffer SpotLightListTrans : register(t11);
SamplerState LinearClamppedSampler : register(s13);
TextureCube EnvCube : register(t14);
SamplerState EnvSampler : register(s15);

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

	float3 BaseColor = MaterialInput.Diffuse;
	float Occlusion = MaterialInput.Occlusion;
	float Metallic = MaterialInput.Metallic;
	float Roughness = MaterialInput.Roughness;
    float Opacity = MaterialInput.Opacity;

    float Smoothness = 1.0f - Roughness;
    float SmoothnessSquare = Smoothness * Smoothness;
    
    float2 ScreenUV = Vin.Position.xy * UHResolution.zw;
    float3 WorldPos = Vin.WorldPos;
    
#if WITH_REFRACTION || WITH_ENVCUBE
    // Calc eye vector when necessary
    float3 EyeVector = normalize(WorldPos - UHCameraPos);
#endif    

#if WITH_TANGENT_SPACE
	float3 BumpNormal = MaterialInput.Normal;

	// tangent to world space
	BumpNormal = mul(BumpNormal, Vin.WorldTBN);
	BumpNormal *= (bIsFrontFace) ? 1 : -1;
#else
	float3 BumpNormal = normalize(Vin.Normal);
	BumpNormal *= (bIsFrontFace) ? 1 : -1;
#endif
    
    // Base Color PBR
    BaseColor = saturate(BaseColor - BaseColor * Metallic);

	// specular color and roughness
	float3 Specular = MaterialInput.Specular;
	Specular = ComputeSpecularColor(Specular, MaterialInput.Diffuse, Metallic);

	float3 IndirectSpecular = 0;
#if WITH_ENVCUBE
	// if per-object env cube is used, calculate it here, and adds the result to emissive
	float3 R = reflect(EyeVector, BumpNormal);
	float NdotV = abs(dot(BumpNormal, -EyeVector));
	float SpecFade = SmoothnessSquare;
	float SpecMip = (1.0f - SpecFade) * GEnvCubeMipMapCount;

	IndirectSpecular = EnvCube.SampleLevel(EnvSampler, R, SpecMip).rgb * UHAmbientSky * SpecFade * SchlickFresnel(Specular, lerp(0, NdotV, MaterialInput.FresnelFactor));

	// since indirect spec will be added directly and can't be scaled with NdotL, expose material parameter to scale down it
	IndirectSpecular *= MaterialInput.ReflectionFactor;
#endif
    
    // Refraction
#if WITH_REFRACTION
    // Calc refract vector
    float3 RefractEyeVec = refract(EyeVector, BumpNormal, MaterialInput.Refraction);
    float2 RefractOffset = (RefractEyeVec.xy - EyeVector.xy);
    float2 RefractUV = ScreenUV + RefractOffset * 0.1f;
    
    UHBRANCH
    if (RefractUV.x != saturate(RefractUV.x) || RefractUV.y != saturate(RefractUV.y))
    {
        // if it's outside the screen range, do not refract. I don't want to see clamping.
        RefractUV = ScreenUV;
    }
    
    float3 SceneColor = UHTextureTable[GRefractionClearIndex].SampleLevel(LinearClamppedSampler, RefractUV, 0).rgb;
    float3 BlurredSceneColor = UHTextureTable[GRefractionBlurIndex].SampleLevel(LinearClamppedSampler, RefractUV, 0).rgb;
    
    // Use the scene color for refraction, the original BaseColor will be used as tint color instead
    // Roughness will be used as a factor for lerp clear/blurred scene
    float3 RefractionColor = lerp(BlurredSceneColor, SceneColor, SmoothnessSquare * SmoothnessSquare);
    float3 RefractionResult = RefractionColor * BaseColor;
    RefractionResult += MaterialInput.Emissive.rgb + IndirectSpecular;
    
    // Early return and do not proceed to lighting, refraction should rely on the lit opaque scene.
    return float4(RefractionResult, Opacity);
#endif

	// ------------------------------------------------------------------------------------------ dir lights accumulation
	// sample shadows
	float ShadowMask = 1.0f;
#if WITH_RTSHADOWS
	float2 UV = Vin.Position.xy * UHResolution.zw;
	ShadowMask = RTShadow.Sample(LinearClamppedSampler, UV).r;
#endif

	// light calculation, be sure to normalize vector before using it
	float3 Result = 0;
	
    UHLightInfo LightInfo;
    LightInfo.Diffuse = BaseColor;
    LightInfo.Specular = float4(Specular, Roughness);
    LightInfo.Normal = normalize(BumpNormal);
    LightInfo.WorldPos = WorldPos;
    LightInfo.ShadowMask = ShadowMask;
	
	for (uint Ldx = 0; Ldx < UHNumDirLights; Ldx++)
	{
        UHBRANCH
        if (!UHDirLights[Ldx].bIsEnabled)
        {
            continue;
        }
        
        LightInfo.LightColor = UHDirLights[Ldx].Color.rgb;
        LightInfo.LightDir = UHDirLights[Ldx].Dir;
        Result += LightBRDF(LightInfo);
    }
	
	// ------------------------------------------------------------------------------------------ point lights accumulation
	// point lights accumulation, fetch the tile index here, note that the system culls at half resolution
    uint TileX = uint(Vin.Position.x + 0.5f) / UHLIGHTCULLING_TILE / UHLIGHTCULLING_UPSCALE;
    uint TileY = uint(Vin.Position.y + 0.5f) / UHLIGHTCULLING_TILE / UHLIGHTCULLING_UPSCALE;
    uint TileIndex = TileX + TileY * UHLightTileCountX;
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint PointLightCount = PointLightListTrans.Load(TileOffset);
    TileOffset += 4;
	
    float3 LightToWorld;
    float LightAtten;
    float AttenNoise = GetAttenuationNoise(Vin.Position.xy);
    
    for (Ldx = 0; Ldx < PointLightCount; Ldx++)
    {
        uint PointLightIdx = PointLightListTrans.Load(TileOffset);
       
        UHPointLight PointLight = UHPointLights[PointLightIdx];
        UHBRANCH
        if (!PointLight.bIsEnabled)
        {
            continue;
        }
        LightInfo.LightColor = PointLight.Color.rgb;
		
        LightToWorld = WorldPos - PointLight.Position;
        LightInfo.LightDir = normalize(LightToWorld);
		
		// square distance attenuation
        LightAtten = 1.0f - saturate(length(LightToWorld) / PointLight.Radius + AttenNoise);
        LightAtten *= LightAtten;
        LightInfo.ShadowMask = LightAtten * ShadowMask;
		
        Result += LightBRDF(LightInfo);
        TileOffset += 4;
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
        UHBRANCH
        if (!SpotLight.bIsEnabled)
        {
            continue;
        }
        
        LightInfo.LightColor = SpotLight.Color.rgb;
        LightInfo.LightDir = SpotLight.Dir;
        LightToWorld = WorldPos - SpotLight.Position;
        
        // squared distance attenuation
        LightAtten = 1.0f - saturate(length(LightToWorld) / SpotLight.Radius + AttenNoise);
        LightAtten *= LightAtten;
        
        // squared spot angle attenuation
        float Rho = dot(SpotLight.Dir, normalize(LightToWorld));
        float SpotFactor = (Rho - cos(SpotLight.Angle)) / (cos(SpotLight.InnerAngle) - cos(SpotLight.Angle));
        SpotFactor = saturate(SpotFactor);
        
        LightAtten *= SpotFactor * SpotFactor;
        LightInfo.ShadowMask = LightAtten * ShadowMask;
		
        Result += LightBRDF(LightInfo);
    }

	// indirect light accumulation
	Result += ShadeSH9(BaseColor, float4(BumpNormal, 1.0f), Occlusion);
	Result += MaterialInput.Emissive.rgb + IndirectSpecular;

	// output result with opacity
	return float4(Result, Opacity);
}