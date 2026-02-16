#pragma once
#include "RenderingTypes.h"
#include "../Classes/RenderBuffer.h"
#include "../Classes/RenderTexture.h"
#include "../Classes/Sampler.h"
#include "../Classes/TextureCube.h"
#include "../Classes/AccelerationStructure.h"

// define shared resource in renderer, the goal is to reduce parameter sending between renderer and shader
extern UniquePtr<UHRenderBuffer<UHSystemConstants>> GSystemConstantBuffer[GMaxFrameInFlight];
extern UniquePtr<UHRenderBuffer<UHObjectConstants>> GObjectConstantBuffer[GMaxFrameInFlight];
extern UniquePtr<UHRenderBuffer<UHObjectConstants>> GOcclusionConstantBuffer[GMaxFrameInFlight];
extern UniquePtr<UHRenderBuffer<UHDirectionalLightConstants>> GDirectionalLightBuffer[GMaxFrameInFlight];
extern UniquePtr<UHRenderBuffer<UHPointLightConstants>> GPointLightBuffer[GMaxFrameInFlight];
extern UniquePtr<UHRenderBuffer<UHSpotLightConstants>> GSpotLightBuffer[GMaxFrameInFlight];

// light culling
extern UniquePtr<UHRenderBuffer<uint32_t>> GPointLightListBuffer;
extern UniquePtr<UHRenderBuffer<uint32_t>> GPointLightListTransBuffer;
extern UniquePtr<UHRenderBuffer<uint32_t>> GSpotLightListBuffer;
extern UniquePtr<UHRenderBuffer<uint32_t>> GSpotLightListTransBuffer;

// render textures
extern UHRenderTexture* GSceneDiffuse;
extern UHRenderTexture* GSceneNormal;
extern UHRenderTexture* GSceneMaterial;
extern UHRenderTexture* GSceneResult;
extern UHRenderTexture* GSceneMip;
// extra scene data buffer
extern UHRenderTexture* GSceneData;
extern UHRenderTexture* GSceneDepth;
// mixed depth, which means translucent depth is rendered on the top of opaque depth
extern UHRenderTexture* GSceneMixedDepth;
extern UHRenderTexture* GMotionVectorRT;
extern UHRenderTexture* GPostProcessRT;
extern UHRenderTexture* GPreviousSceneResult;
extern UHRenderTexture* GOpaqueSceneResult;
extern UHRenderTexture* GHistoryDepth;
extern UHRenderTexture* GHistoryNormal;

// accessor for GBuffers
extern std::vector<UHRenderTexture*> GSceneBuffers;
extern std::vector<UHRenderTexture*> GSceneBuffersWithDepth;

// ray-tracing
extern UHRenderTexture* GRTShadowData;
extern UHRenderTexture* GRTSoftShadow;
extern UHRenderTexture* GRTReceiveLightBits;
extern UHRenderTexture* GRTReflectionResult;
extern UHRenderTexture* GSmoothSceneNormal;
extern UniquePtr<UHAccelerationStructure> GTopLevelAS[GMaxFrameInFlight];

// cubemaps
extern UHTextureCube* GSkyLightCube;

// samplers
extern UHSampler* GPointClampedSampler;
extern UHSampler* GLinearClampedSampler;
extern UHSampler* GSkyCubeSampler;
extern UHSampler* GPointClamped3DSampler;
extern UHSampler* GLinearClamped3DSampler;

// SH9 data
extern UniquePtr<UHRenderBuffer<UHSphericalHarmonicData>> GSH9Data;

// fallback textures
extern UHTexture2D* GBlackTexture;
extern UHTexture2D* GWhiteTexture;
extern UHTexture2D* GTransparentTexture;
extern UHTextureCube* GBlackCube;
extern UHRenderTexture* GBlackTextureArray;
extern UHRenderTexture* GWhiteTextureArray;
extern UHTexture2D* GMaxUIntTexture;

// occlusion data
extern UniquePtr<UHRenderBuffer<uint32_t>> GOcclusionResult[GMaxFrameInFlight];

// instance lights buffer
extern UniquePtr<UHRenderBuffer<UHInstanceLights>> GInstanceLightsBuffer[GMaxFrameInFlight];

// --------------------------- mesh shader data

// mesh shader data for use
extern std::vector<UniquePtr<UHRenderBuffer<UHMeshShaderData>>> GMeshShaderData[GMaxFrameInFlight];
extern std::vector<UniquePtr<UHRenderBuffer<UHMeshShaderData>>> GMotionOpaqueShaderData[GMaxFrameInFlight];
extern std::vector<UniquePtr<UHRenderBuffer<UHMeshShaderData>>> GMotionTranslucentShaderData[GMaxFrameInFlight];

// this is also used for ray tracing
extern UniquePtr<UHRenderBuffer<UHRendererInstance>> GRendererInstanceBuffer;

// indirect lighting, RT buffers + cache + result buffer
extern UHRenderTexture* GRTIndirectDiffuse;
extern UHRenderTexture* GRTIndirectDiffuseHistory;
extern UHRenderTexture* GRTIndirectOcclusion;
extern UHRenderTexture* GRTIndirectOcclusionHistory;
extern UHRenderTexture* GRTSkyData;
extern UHRenderTexture* GRTSkyDiscoverAngle;

// common clear colors
extern VkClearColorValue GBlackClearColor;
extern VkClearColorValue GWhiteClearColor;
extern VkClearColorValue GTransparentClearColor;

extern std::vector<UHTexture*> GetGBuffersSRV();

// wrapper function to create textures
extern UHTexture2D* CreateTexture2D(UHGraphic* InGfx, uint32_t InWidth, uint32_t InHeight, UHTextureFormat InFormat, std::string InName);
extern UHTextureCube* CreateTextureCube(UHGraphic* InGfx, uint32_t InWidth, uint32_t InHeight, UHTextureFormat InFormat
	, std::string InName, bool bIsReadWrite = false);