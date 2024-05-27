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
UHRenderTexture* GSceneVertexNormal;

UHRenderTexture* GMotionVectorRT;
UHRenderTexture* GPostProcessRT;
UHRenderTexture* GPreviousSceneResult;

UHRenderTexture* GRTShadowResult;
UHRenderTexture* GRTSharedTextureRG;
UHRenderTexture* GRTReflectionResult;

UHRenderTexture* GQuarterBlurredScene;
UHRenderTexture* GOpaqueSceneResult;

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

inline std::vector<UHTexture*> GetGBuffersSRV()
{
	// get the GBuffer used in SRV
	std::vector<UHTexture*> GBuffers = { GSceneDiffuse, GSceneNormal, GSceneMaterial, GSceneDepth };
	return GBuffers;
}