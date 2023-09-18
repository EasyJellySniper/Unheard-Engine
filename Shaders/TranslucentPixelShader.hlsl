#define UHDIRLIGHT_BIND t6
#define UHPOINTLIGHT_BIND t7
#include "../Shaders/UHInputs.hlsli"
#include "../Shaders/UHCommon.hlsli"

Texture2D RTShadow : register(t8);
ByteAddressBuffer PointLightListTrans : register(t9);
SamplerState LinearClamppedSampler : register(s10);
TextureCube EnvCube : register(t11);
SamplerState EnvSampler : register(s12);

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
	BaseColor = saturate(BaseColor - BaseColor * Metallic);

#if WITH_TANGENT_SPACE
	float3 BumpNormal = MaterialInput.Normal;

	// tangent to world space
	BumpNormal = mul(BumpNormal, Vin.WorldTBN);
	BumpNormal *= (bIsFrontFace) ? 1 : -1;
#else
	float3 BumpNormal = normalize(Vin.Normal);
	BumpNormal *= (bIsFrontFace) ? 1 : -1;
#endif

	// specular color and roughness
	float3 Specular = MaterialInput.Specular;
	Specular = ComputeSpecularColor(Specular, MaterialInput.Diffuse, Metallic);

	float3 IndirectSpecular = 0;
#if WITH_ENVCUBE
	// if per-object env cube is used, calculate it here, and adds the result to emissive
	float3 EyeVector = normalize(Vin.WorldPos - UHCameraPos);
	float3 R = reflect(EyeVector, BumpNormal);
	float NdotV = abs(dot(BumpNormal, -EyeVector));

	float Smoothness = 1.0f - Roughness;
	float SpecFade = Smoothness * Smoothness;
	float SpecMip = (1.0f - SpecFade) * GEnvCubeMipMapCount;

	IndirectSpecular = EnvCube.SampleLevel(EnvSampler, R, SpecMip).rgb * UHAmbientSky * SpecFade * SchlickFresnel(Specular, lerp(0, NdotV, MaterialInput.FresnelFactor));

	// since indirect spec will be added directly and can't be scaled with NdotL, expose material parameter to scale down it
	IndirectSpecular *= MaterialInput.ReflectionFactor;
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
    LightInfo.WorldPos = Vin.WorldPos;
    LightInfo.ShadowMask = ShadowMask;
	
	for (uint Ldx = 0; Ldx < UHNumDirLights; Ldx++)
	{
        LightInfo.LightColor = UHDirLights[Ldx].Color.rgb;
        LightInfo.LightDir = UHDirLights[Ldx].Dir;
        Result += LightBRDF(LightInfo);
    }
	
	    // ------------------------------------------------------------------------------------------ point lights accumulation
	// point lights accumulation, fetch the tile index here, note that the system culls at half resolution
    uint TileX = uint(Vin.Position.x) / UHLIGHTCULLING_TILE / UHLIGHTCULLING_UPSCALE;
    uint TileY = uint(Vin.Position.y) / UHLIGHTCULLING_TILE / UHLIGHTCULLING_UPSCALE;
    uint TileIndex = TileX + TileY * UHLightTileCountX;
    uint TileOffset = GetPointLightOffset(TileIndex);
    uint PointLightCount = PointLightListTrans.Load(TileOffset);
    TileOffset += 4;
	
    float3 WorldToLight;
    float LightAtten;
    float AttenNoise = GetAttenuationNoise(Vin.Position.xy);
    
    for (Ldx = 0; Ldx < PointLightCount; Ldx++)
    {
        uint PointLightIdx = PointLightListTrans.Load(TileOffset);
       
        UHPointLight PointLight = UHPointLights[PointLightIdx];
        LightInfo.LightColor = PointLight.Color.rgb;
		
        WorldToLight = Vin.WorldPos - PointLight.Position;
        LightInfo.LightDir = normalize(WorldToLight);
		
		// square distance attenuation
        LightAtten = 1.0f - saturate(length(WorldToLight) / PointLight.Radius + AttenNoise);
        LightAtten *= LightAtten;
        LightInfo.ShadowMask = LightAtten * ShadowMask;
		
        Result += LightBRDF(LightInfo);
        TileOffset += 4;
    }

	// indirect light accumulation
	Result += LightIndirect(BaseColor, BumpNormal, Occlusion);
	Result += MaterialInput.Emissive.rgb + IndirectSpecular;

	// output result with opacity
	return float4(Result, MaterialInput.Opacity);
}