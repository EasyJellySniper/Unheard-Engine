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
UHRenderTexture* GPostProcessRT;
UHRenderTexture* GPreviousSceneResult;
UHRenderTexture* GRTShadowResult;
UHRenderTexture* GRTSharedTextureRG16F;
UHRenderTexture* GHalfDepth;
UHRenderTexture* GHalfTranslucentDepth;
UHRenderTexture* GQuarterBlurredScene;
UHRenderTexture* GOpaqueSceneResult;
UHRenderTexture* GGaussianFilterTempRT0;
UHRenderTexture* GGaussianFilterTempRT1;

UHTextureCube* GSkyLightCube;

UHSampler* GPointClampedSampler;
UHSampler* GLinearClampedSampler;
UHSampler* GAnisoClampedSampler;
UHSampler* GSkyCubeSampler;
UniquePtr<UHRenderBuffer<UHSphericalHarmonicData>> GSH9Data;

int32_t GRefractionClearIndex = UHINDEXNONE;
int32_t GRefractionBlurredIndex = UHINDEXNONE;

UHTexture2D* GWhiteTexture;
UHTextureCube* GBlackCube;