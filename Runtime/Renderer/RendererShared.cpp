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
UHRenderTexture* GSceneMip;
UHRenderTexture* GSceneData;
UHRenderTexture* GSceneDepth;
UHRenderTexture* GSceneMixedDepth;
UHRenderTexture* GMotionVectorRT;
UHRenderTexture* GPostProcessRT;
UHRenderTexture* GPreviousSceneResult;

// accessor for GBuffers
std::vector<UHRenderTexture*> GSceneBuffers;
std::vector<UHRenderTexture*> GSceneBuffersWithDepth;

// shadow buffers, can be reused after opaque lighting pass is done
UHRenderTexture* GRTShadowData;
UHRenderTexture* GRTSoftShadow;
UHRenderTexture* GRTReceiveLightBits;
// GRTReflectionResult can be reused after translucent pass is done
UHRenderTexture* GRTReflectionResult;
UHRenderTexture* GSmoothSceneNormal;
UniquePtr<UHAccelerationStructure> GTopLevelAS[GMaxFrameInFlight];

// for refraction use
UHRenderTexture* GOpaqueSceneResult;

// depth/normal history
UHRenderTexture* GHistoryDepth;
UHRenderTexture* GHistoryNormal;

UHTextureCube* GSkyLightCube;

UHSampler* GPointClampedSampler;
UHSampler* GLinearClampedSampler;
UHSampler* GSkyCubeSampler;
UHSampler* GPointClamped3DSampler;
UHSampler* GLinearClamped3DSampler;
UniquePtr<UHRenderBuffer<UHSphericalHarmonicData>> GSH9Data;

UHTexture2D* GBlackTexture;
UHTexture2D* GWhiteTexture;
UHTexture2D* GTransparentTexture;
UHTextureCube* GBlackCube;
UHRenderTexture* GBlackTextureArray;
UHRenderTexture* GWhiteTextureArray;
UHTexture2D* GMaxUIntTexture;

// occlusion data
UniquePtr<UHRenderBuffer<uint32_t>> GOcclusionResult[GMaxFrameInFlight];
// instance lights buffer
UniquePtr<UHRenderBuffer<UHInstanceLights>> GInstanceLightsBuffer[GMaxFrameInFlight];

std::vector<UniquePtr<UHRenderBuffer<UHMeshShaderData>>> GMeshShaderData[GMaxFrameInFlight];
std::vector<UniquePtr<UHRenderBuffer<UHMeshShaderData>>> GMotionOpaqueShaderData[GMaxFrameInFlight];
std::vector<UniquePtr<UHRenderBuffer<UHMeshShaderData>>> GMotionTranslucentShaderData[GMaxFrameInFlight];
UniquePtr<UHRenderBuffer<UHRendererInstance>> GRendererInstanceBuffer;

// indirect lighting
UHRenderTexture* GRTIndirectDiffuse;
UHRenderTexture* GRTIndirectDiffuseHistory;
UHRenderTexture* GRTIndirectOcclusion;
UHRenderTexture* GRTIndirectOcclusionHistory;
UHRenderTexture* GRTSkyData;
UHRenderTexture* GRTSkyDiscoverAngle;

VkClearColorValue GBlackClearColor = { 0.0f,0.0f,0.0f,1.0f };
VkClearColorValue GWhiteClearColor = { 1.0f,1.0f,1.0f,1.0f };
VkClearColorValue GTransparentClearColor = { 0.0f,0.0f,0.0f,0.0f };

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