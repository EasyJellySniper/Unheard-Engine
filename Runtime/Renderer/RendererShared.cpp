#include "RendererShared.h"

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
UHRenderTexture* GSceneDepth;
UHRenderTexture* GSceneTranslucentDepth;
// vertex normal info used in ray tracing, used for pushing ray origin point from surface for a bit to avoid self occluding
UHRenderTexture* GSceneVertexNormal;

UHRenderTexture* GMotionVectorRT;
UHRenderTexture* GPostProcessRT;
UHRenderTexture* GPreviousSceneResult;

UHRenderTexture* GRTShadowResult;
// GRTSharedTextureRG can be reused after soft shadow is done
UHRenderTexture* GRTSharedTextureRG;
// GRTReflectionResult can be reused after translucent pass is done
UHRenderTexture* GRTReflectionResult;
UniquePtr<UHAccelerationStructure> GTopLevelAS[GMaxFrameInFlight];

// these two are for refraction use
UHRenderTexture* GQuarterBlurredScene;
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
UniquePtr<UHRenderBuffer<UHInstanceLights>> GInstanceLightsBuffer;

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