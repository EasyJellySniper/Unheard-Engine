#include "RendererShared.h"

UniquePtr<UHRenderBuffer<UHSystemConstants>> GSystemConstantBuffer[GMaxFrameInFlight];
UniquePtr<UHRenderBuffer<UHObjectConstants>> GObjectConstantBuffer[GMaxFrameInFlight];
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
UHRenderTexture* GSceneTranslucentVertexNormal;
UHRenderTexture* GMotionVectorRT;
UHRenderTexture* GPrevMotionVectorRT;
UHRenderTexture* GPostProcessRT;
UHRenderTexture* GPreviousSceneResult;
UHRenderTexture* GRTShadowResult;
UHRenderTexture* GRTSharedTextureRG16F;

UHTextureCube* GSkyLightCube;

UHSampler* GPointClampedSampler;
UHSampler* GLinearClampedSampler;
UHSampler* GAnisoClampedSampler;
UHSampler* GSkyCubeSampler;
UniquePtr<UHRenderBuffer<UHSphericalHarmonicData>> GSH9Data;