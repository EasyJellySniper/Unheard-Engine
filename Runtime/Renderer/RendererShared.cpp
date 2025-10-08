#include "RendererShared.h"
#include "Runtime/Engine/Graphic.h"

UniquePtr<UHRenderBuffer<UHSystemConstants>> GSystemConstantBuffer[GMaxFrameInFlight];
UniquePtr<UHRenderBuffer<UHObjectConstants>> GObjectConstantBuffer[GMaxFrameInFlight];
UniquePtr<UHRenderBuffer<UHObjectConstants>> GOcclusionConstantBuffer[GMaxFrameInFlight];
UniquePtr<UHRenderBuffer<UHDirectionalLightConstants>> GDirectionalLightBuffer[GMaxFrameInFlight];
UniquePtr<UHRenderBuffer<UHPointLightConstants>> GPointLightBuffer[GMaxFrameInFlight];
UniquePtr<UHRenderBuffer<UHSpotLightConstants>> GSpotLightBuffer[GMaxFrameInFlight];

UniquePtr<UHRenderBuffer<uint32_t>> GPointLightListBuffer;
UniquePtr<UHRenderBuffer<uint32_t>> GPointLightListTransBuffer;
UniquePtr<UHRenderBuffer<uint32_t>> GSpotLightListBuffer;
UniquePtr<UHRenderBuffer<uint32_t>> GSpotLightListTransBuffer;

UHRenderTexture* GSceneDiffuse;
UHRenderTexture* GSceneNormal;
UHRenderTexture* GSceneMaterial;
UHRenderTexture* GSceneResult;
UHRenderTexture* GSceneExtraData;
UHRenderTexture* GSceneDepth;
UHRenderTexture* GSceneMixedDepth;
UHRenderTexture* GMotionVectorRT;
UHRenderTexture* GPostProcessRT;
UHRenderTexture* GPreviousSceneResult;

UHRenderTexture* GRTShadowResult;
// GRTSharedTextureRG can be reused after soft shadow is done
UHRenderTexture* GRTSharedTextureRG;
// GRTReflectionResult can be reused after translucent pass is done
UHRenderTexture* GRTReflectionResult;
UHRenderTexture* GSmoothSceneNormal;
UniquePtr<UHAccelerationStructure> GTopLevelAS[GMaxFrameInFlight];

// for refraction use
UHRenderTexture* GOpaqueSceneResult;

// these two are used for RT reflection, can be reused after RT reflection pass
UHRenderTexture* GTranslucentBump;
UHRenderTexture* GTranslucentSmoothness;

UHTextureCube* GSkyLightCube;

UHSampler* GPointClampedSampler;
UHSampler* GLinearClampedSampler;
UHSampler* GSkyCubeSampler;
UniquePtr<UHRenderBuffer<UHSphericalHarmonicData>> GSH9Data;

UHTexture2D* GBlackTexture;
UHTexture2D* GWhiteTexture;
UHTextureCube* GBlackCube;

// occlusion data
UniquePtr<UHRenderBuffer<uint32_t>> GOcclusionResult[GMaxFrameInFlight];
// instance lights buffer
UniquePtr<UHRenderBuffer<UHInstanceLights>> GInstanceLightsBuffer[GMaxFrameInFlight];

std::vector<UniquePtr<UHRenderBuffer<UHMeshShaderData>>> GMeshShaderData[GMaxFrameInFlight];
std::vector<UniquePtr<UHRenderBuffer<UHMeshShaderData>>> GMotionOpaqueShaderData[GMaxFrameInFlight];
std::vector<UniquePtr<UHRenderBuffer<UHMeshShaderData>>> GMotionTranslucentShaderData[GMaxFrameInFlight];
UniquePtr<UHRenderBuffer<UHRendererInstance>> GRendererInstanceBuffer;

inline std::vector<UHTexture*> GetGBuffersSRV()
{
	// get the GBuffer used in SRV
	std::vector<UHTexture*> GBuffers = { GSceneDiffuse, GSceneNormal, GSceneMaterial, GSceneDepth };
	return GBuffers;
}

inline UHTexture2D* CreateTexture2D(UHGraphic* InGfx, uint32_t InWidth, uint32_t InHeight
	, UHTextureFormat InFormat, std::string InName)
{
	VkExtent2D Size;
	Size.width = InWidth;
	Size.height = InHeight;

	UHTextureSettings TexSetting{};
	TexSetting.bIsLinear = true;
	TexSetting.bUseMipmap = false;

	UniquePtr<UHTexture2D> Tex = MakeUnique<UHTexture2D>(InName, "", Size, InFormat, TexSetting);
	return InGfx->RequestTexture2D(Tex, false);
}

inline UHTextureCube* CreateTextureCube(UHGraphic* InGfx, uint32_t InWidth, uint32_t InHeight
	, UHTextureFormat InFormat, std::string InName, bool bIsReadWrite)
{
	VkExtent2D Size;
	Size.width = InWidth;
	Size.height = InHeight;

	UHTextureSettings TexSetting{};
	TexSetting.bIsLinear = true;
	TexSetting.bUseMipmap = false;

	UniquePtr<UHTextureCube> Cube = MakeUnique<UHTextureCube>(InName, Size, InFormat, TexSetting);
	return InGfx->RequestTextureCube(Cube);
}