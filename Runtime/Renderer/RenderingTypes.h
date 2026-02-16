#pragma once
#include "../Classes/Types.h"
#include "../Classes/MaterialLayout.h"
#include "../Classes/Shader.h"
#include <array>

// header for define frame resource type
// these structs should keep syncing with shader defines

// let cpu be advanced a few more frame, 2 seems a sweetpot, going futher brings latency and extra memory consumption
const uint32_t GMaxFrameInFlight = 2;
const uint32_t GNumOfPostProcessRT = 2;

// gbuffer counts
const uint32_t GNumOfGBuffers = 6;
const uint32_t GNumOfGBuffersSRV = 4;
const uint32_t GNumOfGBuffersTrans = 5;

// thread group number
const uint32_t GThreadGroup2D_X = 8;
const uint32_t GThreadGroup2D_Y = 8;
const uint32_t GThreadGroup1D = 64;
const uint32_t GThreadGroup3D_X = 4;
const uint32_t GThreadGroup3D_Y = 4;
const uint32_t GThreadGroup3D_Z = 4;

// descriptor set space number
const uint32_t GTextureTableSpace = 1;
const uint32_t GSamplerTableSpace = 2;

// hit group number
const uint32_t GRayGenTableSlot = 0;
const uint32_t GMissTableSlot = 1;
const uint32_t GHitGroupTableSlot = 2;
const uint32_t GHitGroupShaderPerSlot = 2;
const float G_PI = 3.141592653589793f;

struct UHSystemConstants
{
	XMFLOAT4X4 ViewProj;
	XMFLOAT4X4 ViewProjInv;
	XMFLOAT4X4 ViewProj_NonJittered;
	XMFLOAT4X4 ViewProjInv_NonJittered;
	XMFLOAT4X4 PrevViewProj_NonJittered;
	XMFLOAT4X4 ProjInv;
	XMFLOAT4X4 ProjInv_NonJittered;
	XMFLOAT4X4 View;
	XMFLOAT4 Resolution;
	XMFLOAT4 ShadowResolution;
	XMFLOAT3 CameraPos;
	uint32_t NumDirLights;

	XMFLOAT3 AmbientSky;
	float JitterOffsetX;
	XMFLOAT3 AmbientGround;
	float JitterOffsetY;

	XMFLOAT3 CameraDir;
	uint32_t NumRTInstances;

	float JitterScaleMin;
	float JitterScaleFactor;
	uint32_t NumPointLights;
	uint32_t LightTileCountX;

	uint32_t MaxPointLightPerTile;
	uint32_t NumSpotLights;
	uint32_t MaxSpotLightPerTile;
	uint32_t FrameNumber;
	
	// the feature shall be able to store up to 32 flag bits without issues
	uint32_t SystemRenderFeature;
	float DirectionalShadowRayTMax;
	uint32_t LinearClampSamplerIndex;
	uint32_t SkyCubeSamplerIndex;

	uint32_t PointClampSamplerIndex;
	uint32_t RTReflectionQuality;
	float RTReflectionRayTMax;
	float RTReflectionSmoothCutoff;

	float EnvCubeMipMapCount;
	uint32_t DefaultAnisoSamplerIndex;
	uint32_t OpaqueSceneTextureIndex;
	float FinalReflectionStrength;

	XMFLOAT3 SceneCenter;
	float NearPlane;

	XMFLOAT3 SceneExtent;
	float RTCullingDistance;

	uint32_t MaxReflectionRecursion;
	float ScreenMipCount;
	float RTReflectionMipCount;
	float FarPlane;
};

struct UHObjectConstants
{
	XMFLOAT4X4 GWorld;
	XMFLOAT4X4 GWorldIT;
	XMFLOAT4X4 GPrevWorld;
	uint32_t InstanceIndex;
	XMFLOAT3 WorldPos;
	XMFLOAT3 BoundExtent;
	
	// align to 256 bytes
	float CPUPadding[9];
};

struct UHDirectionalLightConstants
{
	// intensity is multiplied to Color before sending to GPU
	XMFLOAT4 Color = XMFLOAT4();
	XMFLOAT3 Dir = XMFLOAT3();
	int32_t IsEnabled = 1;
};

struct UHPointLightConstants
{
	// intensity is multiplied to Color before sending to GPU
	XMFLOAT4 Color = XMFLOAT4();
	float Radius = 0.0f;
	XMFLOAT3 Position = XMFLOAT3();
	int32_t IsEnabled = 1;
};

struct UHSpotLightConstants
{
	XMFLOAT4X4 WorldToLight = XMFLOAT4X4();
	// intensity is multiplied to Color before sending to GPU
	XMFLOAT4 Color = XMFLOAT4();
	XMFLOAT3 Dir = XMFLOAT3();
	float Radius = 0.0f;
	float Angle = 0.0f;
	XMFLOAT3 Position = XMFLOAT3();
	float InnerAngle = 0.0f;
	int32_t IsEnabled = 1;
};

struct UHSphericalHarmonicData
{
	XMFLOAT4 cAr;
	XMFLOAT4 cAg;
	XMFLOAT4 cAb;
	XMFLOAT4 cBr;
	XMFLOAT4 cBg;
	XMFLOAT4 cBb;
	XMFLOAT4 cC;
};

struct UHSphericalHarmonicConstants
{
	uint32_t MipLevel;
	float Weight; // this should be set as 4.0f * PI / SampleCount in C++ side
};

enum class UHRenderPassTypes
{
	OcclusionResolve = 0,
	DepthPrePass,
	OcclusionPass,
	BasePass,
	UpdateTopLevelAS,
	CollectLightPass,
	GenerateSH9,
	RayTracingShadow,
	SoftShadowPass,
	SmoothSceneNormalPass,
	RayTracingSkyLight,
	RayTracingIndirectLight,
	RayTracingReflection,
	ReflectionBlurPass,
	LightCulling,
	LightPass,
	IndirectLightPass,
	SkyPass,
	MotionPass,
	PreReflectionPass,
	ReflectionPass,
	TranslucentPass,
	ToneMappingPass,
	TemporalAAPass,
	HistoryCopyingPass,
	PresentToSwapChain,
	UHRenderPassMax
};

struct UHDepthInfo
{
	bool bEnableDepthTest;
	bool bEnableDepthWrite;
	VkCompareOp DepthFunc;

	UHDepthInfo();
	UHDepthInfo(bool bInEnableDepthTest, bool bInEnableDepthWrite, VkCompareOp InDepthFunc);

	friend bool operator==(const UHDepthInfo& InInfo, const UHDepthInfo& Info)
	{
		return InInfo.bEnableDepthTest == Info.bEnableDepthTest
			&& InInfo.bEnableDepthWrite == Info.bEnableDepthWrite
			&& InInfo.DepthFunc == Info.DepthFunc;
	}
};

// render pass info for setup graphic states
struct UHRenderPassInfo
{
	UHRenderPassInfo();

	// value for cullmode and blend mode is from different objects, don't set them in constructor for flexible usage
	UHRenderPassInfo(VkRenderPass InRenderPass, UHDepthInfo InDepthInfo, UHCullMode InCullInfo, UHBlendMode InBlendMode
		, uint32_t InVS, uint32_t InPS, int32_t InRTCount, VkPipelineLayout InPipelineLayout);

	bool operator==(const UHRenderPassInfo& InInfo);

	UHCullMode CullMode;
	UHBlendMode BlendMode;
	VkRenderPass RenderPass;
	UHDepthInfo DepthInfo;
	uint32_t VS;
	uint32_t PS;
	uint32_t GS;
	uint32_t AS;
	uint32_t MS;
	int32_t RTCount;
	VkPipelineLayout PipelineLayout;

	bool bDrawLine;
	bool bDrawWireFrame;
	bool bForceBlendOff;
	bool bEnableColorWrite;
	std::vector<bool> bIsIntegerBuffer;
};

// compute pass info for setup compute states
struct UHComputePassInfo
{
	UHComputePassInfo();
	UHComputePassInfo(VkPipelineLayout InPipelineLayout);

	bool operator==(const UHComputePassInfo& InInfo);

	uint32_t CS;
	VkPipelineLayout PipelineLayout;
};

// ray tracing info class
struct UHRayTracingInfo
{
	UHRayTracingInfo();

	bool operator==(const UHRayTracingInfo& InInfo);

	VkPipelineLayout PipelineLayout;
	uint32_t MaxRecursionDepth;
	uint32_t RayGenShader;
	std::vector<uint32_t> ClosestHitShaders;
	std::vector<uint32_t> AnyHitShaders;
	std::vector<uint32_t> MissShaders;
	uint32_t PayloadSize;
	uint32_t AttributeSize;
};

// structure for managing render pass
class UHTexture;
struct UHRenderPassObject
{
	UHRenderPassObject();
	void Release(VkDevice LogicalDevice);
	void ReleaseRenderPass(VkDevice LogicalDevice);
	void ReleaseFrameBuffer(VkDevice LogicalDevice);

	VkFramebuffer FrameBuffer;
	VkRenderPass RenderPass;
	std::vector<UHTexture*> ColorTextures;
	UHTexture* DepthTexture;
	VkImageLayout FinalLayout;
	VkImageLayout FinalDepthLayout;
};

// class for recording render states (is dirty or something else?)
// use with class that needs upload gpu data (Material, Renderer, Lighting...etc)
class UHRenderState
{
public:
	UHRenderState();

	void SetRenderDirties(bool bIsDirty);
	void SetRenderDirty(bool bIsDirty, int32_t FrameIdx);

	void SetMotionDirties(bool bIsDirty);
	void SetMotionDirty(bool bIsDirty, int32_t FrameIdx);

	bool IsRenderDirty(int32_t FrameIdx) const;
	bool IsMotionDirty(int32_t FrameIdx) const;

	void SetBufferDataIndex(int32_t InIndex);
	int32_t GetBufferDataIndex() const;

private:
	bool bIsRenderDirties[GMaxFrameInFlight];
	bool bIsMotionDirties[GMaxFrameInFlight];
	int32_t BufferDataIndex;
};

#if WITH_EDITOR
enum class UHDebugBoundType
{
	DebugNone = -1,
	DebugBox,
	DebugSphere,
	DebugCone
};

struct UHDebugBoundConstant
{
	XMFLOAT3 Position;
	UHDebugBoundType BoundType;

	XMFLOAT3 Extent;
	float Radius;

	XMFLOAT3 Color;
	float Angle;
	XMFLOAT3 Dir;
	float Padding;

	XMFLOAT3 Right;
	float Padding2;

	XMFLOAT3 Up;
};
#endif

enum class UHSystemRenderFeatureBits
{
	FeatureEnvCube = 1 << 0,
	FeatureHDR = 1 << 1,
	FeatureUseSmoothNormalForRaytracing = 1 << 2,
	FeatureRTIndirectLight = 1 << 3,
	FeatureDebug = 1 << 31,
};

// constant types
enum class UHConstantTypes
{
	System = 0,
	Object,
	Material,
	ConstantTypeMax
};

// renderer instance data
struct UHRendererInstance
{
	uint32_t MeshIndex;
	uint32_t IndiceType;
};

// mesh shader data
struct UHMeshShaderData
{
	uint32_t RendererIndex;
	uint32_t MeshletIndex;
	uint32_t bDoOcclusionTest;
};

// UHInstanceLights to store light indices per-instance
// the workflow will do intersection test in compute shader
const uint32_t GMaxPointSpotLightPerInstance = 16;
struct UHInstanceLights
{
	uint32_t PointLightIndices[GMaxPointSpotLightPerInstance];
	uint32_t SpotLightIndices[GMaxPointSpotLightPerInstance];
};