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
// extra scene data buffer, it is uint16 for now and can be changed in the future
// with the first bit to tell whether a pixel has bump normal, and remaining 15 bits for storing instance ID (up to ~32768)
extern UHRenderTexture* GSceneData;
extern UHRenderTexture* GSceneDepth;
// mixed depth, which means translucent depth is rendered on the top of opaque depth
extern UHRenderTexture* GSceneMixedDepth;
extern UHRenderTexture* GMotionVectorRT;
extern UHRenderTexture* GPostProcessRT;
extern UHRenderTexture* GPreviousSceneResult;
extern UHRenderTexture* GOpaqueSceneResult;
// translucent bump and smoothness, at this point they still need to be separated unless RT passes are designed for opaque only
extern UHRenderTexture* GTranslucentBump;
extern UHRenderTexture* GTranslucentSmoothness;
// accessor for GBuffers
extern std::vector<UHRenderTexture*> GSceneBuffers;
extern std::vector<UHRenderTexture*> GSceneBuffersWithDepth;
extern std::vector<UHRenderTexture*> GSceneBuffersTrans;
extern std::vector<UHRenderTexture*> GSceneBuffersTransWithDepth;

// ray-tracing
extern UHRenderTexture* GRTShadowResult;
extern UHRenderTexture* GRTSharedTextureRG;
extern UHRenderTexture* GRTReflectionResult;
extern UHRenderTexture* GSmoothSceneNormal;
extern UniquePtr<UHAccelerationStructure> GTopLevelAS[GMaxFrameInFlight];

// cubemaps
extern UHTextureCube* GSkyLightCube;

// samplers
extern UHSampler* GPointClampedSampler;
extern UHSampler* GLinearClampedSampler;
extern UHSampler* GSkyCubeSampler;

// SH9 data
extern UniquePtr<UHRenderBuffer<UHSphericalHarmonicData>> GSH9Data;

// fallback textures
extern UHTexture2D* GBlackTexture;
extern UHTexture2D* GWhiteTexture;
extern UHTextureCube* GBlackCube;

// occlusion data
extern UniquePtr<UHRenderBuffer<uint32_t>> GOcclusionResult[GMaxFrameInFlight];

// instance lights buffer
extern UniquePtr<UHRenderBuffer<UHInstanceLights>> GInstanceLightsBuffer[GMaxFrameInFlight];

extern std::vector<UHTexture*> GetGBuffersSRV();

// --------------------------- mesh shader data

// mesh shader data for use
extern std::vector<UniquePtr<UHRenderBuffer<UHMeshShaderData>>> GMeshShaderData[GMaxFrameInFlight];
extern std::vector<UniquePtr<UHRenderBuffer<UHMeshShaderData>>> GMotionOpaqueShaderData[GMaxFrameInFlight];
extern std::vector<UniquePtr<UHRenderBuffer<UHMeshShaderData>>> GMotionTranslucentShaderData[GMaxFrameInFlight];

// this is also used for ray tracing
extern UniquePtr<UHRenderBuffer<UHRendererInstance>> GRendererInstanceBuffer;

// wrapper function to create textures
extern UHTexture2D* CreateTexture2D(UHGraphic* InGfx, uint32_t InWidth, uint32_t InHeight, UHTextureFormat InFormat, std::string InName);
extern UHTextureCube* CreateTextureCube(UHGraphic* InGfx, uint32_t InWidth, uint32_t InHeight, UHTextureFormat InFormat
	, std::string InName, bool bIsReadWrite = false);