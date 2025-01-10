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
extern UHRenderTexture* GSceneDepth;
extern UHRenderTexture* GSceneTranslucentDepth;
extern UHRenderTexture* GSceneVertexNormal;
extern UHRenderTexture* GMotionVectorRT;
extern UHRenderTexture* GPostProcessRT;
extern UHRenderTexture* GPreviousSceneResult;
extern UHRenderTexture* GOpaqueSceneResult;
extern UHRenderTexture* GTranslucentBump;
extern UHRenderTexture* GTranslucentSmoothness;

// ray-tracing
extern UHRenderTexture* GRTShadowResult;
extern UHRenderTexture* GRTSharedTextureRG;
extern UHRenderTexture* GRTReflectionResult;
extern UHRenderTexture* GSmoothReflectVector;
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