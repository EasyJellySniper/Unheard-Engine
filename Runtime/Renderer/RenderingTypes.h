#pragma once
#include "../Classes/Types.h"
#include "../Classes/MaterialLayout.h"
#include "../Classes/Shader.h"
#include <array>

// header for define frame resource type
// these structs should keep syncing with shader defines

// let cpu be advanced a few more frame, 2 seems a sweetpot, going futher brings latency and extra memory consumption
const uint32_t GMaxFrameInFlight = 2;

// gbuffer counts, not including scene result
const uint32_t GNumOfGBuffers = 6;

// thread group number
const uint32_t GThreadGroup2D_X = 8;
const uint32_t GThreadGroup2D_Y = 8;

// descriptor set space number
const uint32_t GTextureTableSpace = 1;
const uint32_t GSamplerTableSpace = 2;

// hit group number
const uint32_t GRayGenTableSlot = 0;
const uint32_t GMissTableSlot = 1;
const uint32_t GHitGroupTableSlot = 2;
const uint32_t GHitGroupShaderPerSlot = 2;

struct UHSystemConstants
{
	XMFLOAT4X4 UHViewProj;
	XMFLOAT4X4 UHViewProjInv;
	XMFLOAT4X4 UHViewProj_NonJittered;
	XMFLOAT4X4 UHViewProjInv_NonJittered;
	XMFLOAT4X4 UHPrevViewProj_NonJittered;
	XMFLOAT4X4 UHProjInv;
	XMFLOAT4X4 UHProjInv_NonJittered;
	XMFLOAT4X4 UHView;
	XMFLOAT4 UHResolution;
	XMFLOAT4 UHShadowResolution;
	XMFLOAT3 UHCameraPos;
	uint32_t UHNumDirLights;

	XMFLOAT3 UHAmbientSky;
	float JitterOffsetX;
	XMFLOAT3 UHAmbientGround;
	float JitterOffsetY;

	XMFLOAT3 UHCameraDir;
	uint32_t UHNumRTInstances;

	float JitterScaleMin;
	float JitterScaleFactor;
	uint32_t UHNumPointLights;
	uint32_t UHLightTileCountX;

	uint32_t UHMaxPointLightPerTile;
};

struct UHObjectConstants
{
	XMFLOAT4X4 UHWorld;
	XMFLOAT4X4 UHWorldIT;
	XMFLOAT4X4 UHPrevWorld;
	uint32_t InstanceIndex;
	float Padding[15];
};

struct UHDirectionalLightConstants
{
	// intensity is multiplied to Color before sending to GPU
	XMFLOAT4 Color;
	XMFLOAT3 Dir;
	float Padding;
};

struct UHPointLightConstants
{
	// intensity is multiplied to Color before sending to GPU
	XMFLOAT4 Color;
	float Radius;
	XMFLOAT3 Position;
};

enum UHRenderPassTypes
{
	DepthPrePass = 0,
	BasePass,
	UpdateTopLevelAS,
	RayTracingShadow,
	LightCulling,
	LightPass,
	SkyPass,
	MotionPass,
	TranslucentPass,
	ToneMappingPass,
	TemporalAAPass,
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
	int32_t RTCount;
	VkPipelineLayout PipelineLayout;
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
	uint32_t MissShader;
	uint32_t PayloadSize;
	uint32_t AttributeSize;
};

// structure for managing render pass
struct UHRenderPassObject
{
	UHRenderPassObject();
	void Release(VkDevice LogicalDevice);
	void ReleaseRenderPass(VkDevice LogicalDevice);
	void ReleaseFrameBuffer(VkDevice LogicalDevice);

	VkFramebuffer FrameBuffer;
	VkRenderPass RenderPass;
};

// class for recording render states (is dirty or something else?)
// use with class that needs upload gpu data (Material, Renderer, Lighting...etc)
class UHRenderState
{
public:
	UHRenderState();

	void SetRenderDirties(bool bIsDirty);
	void SetRenderDirty(bool bIsDirty, int32_t FrameIdx);

	void SetRayTracingDirties(bool bIsDirty);
	void SetRayTracingDirty(bool bIsDirty, int32_t FrameIdx);

	void SetMotionDirties(bool bIsDirty);
	void SetMotionDirty(bool bIsDirty, int32_t FrameIdx);

	bool IsRenderDirty(int32_t FrameIdx) const;
	bool IsRayTracingDirty(int32_t FrameIdx) const;
	bool IsMotionDirty(int32_t FrameIdx) const;

	void SetBufferDataIndex(int32_t InIndex);
	int32_t GetBufferDataIndex() const;

private:
	std::array<bool, GMaxFrameInFlight> bIsRenderDirties;
	std::array<bool, GMaxFrameInFlight> bIsRayTracingDirties;
	std::array<bool, GMaxFrameInFlight> bIsMotionDirties;
	int32_t BufferDataIndex;
};