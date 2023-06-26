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
const uint32_t GNumOfGBuffers = 5;

struct UHSystemConstants
{
	XMFLOAT4X4 UHViewProj;
	XMFLOAT4X4 UHViewProjInv;
	XMFLOAT4X4 UHViewProj_NonJittered;
	XMFLOAT4X4 UHViewProjInv_NonJittered;
	XMFLOAT4X4 UHPrevViewProj_NonJittered;
	XMFLOAT4 UHResolution;
	XMFLOAT4 UHShadowResolution;
	XMFLOAT3 UHCameraPos;
	uint32_t NumDirLights;

	XMFLOAT3 UHAmbientSky;
	float JitterOffsetX;
	XMFLOAT3 UHAmbientGround;
	float JitterOffsetY;

	XMFLOAT3 UHCameraDir;
	uint32_t UHNumRTInstances;
};

struct UHObjectConstants
{
	XMFLOAT4X4 UHWorld;
	XMFLOAT4X4 UHWorldIT;
	XMFLOAT4X4 UHPrevWorld;
	uint32_t InstanceIndex;
};

struct UHMaterialConstants
{
	XMFLOAT4 DiffuseColor;
	XMFLOAT3 EmissiveColor;
	float AmbientOcclusion;
	XMFLOAT3 SpecularColor;
	float Metallic;
	float Roughness;
	float BumpScale;
	float Cutoff;
	float EnvCubeMipMapCount;
	float FresnelFactor;
	int32_t OpacityTexIndex;
	int32_t OpacitySamplerIndex;
	float ReflectionFactor;
};

struct UHDirectionalLightConstants
{
	// assume intensity is multiplied to Color before sending to GPU
	XMFLOAT4 Color;
	XMFLOAT3 Dir;
	float Padding;
};

enum UHRenderPassTypes
{
	UpdateTopLevelAS = 0,
	RayTracingOcclusionTest,
	DepthPrePass,
	BasePass,
	RayTracingShadow,
	LightPass,
	SkyPass,
	MotionPass,
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

	UHDepthInfo()
		: UHDepthInfo(true, true, VK_COMPARE_OP_GREATER)
	{

	}

	UHDepthInfo(bool bInEnableDepthTest, bool bInEnableDepthWrite, VkCompareOp InDepthFunc)
		: bEnableDepthTest(bInEnableDepthTest)
		, bEnableDepthWrite(bInEnableDepthWrite)
		, DepthFunc(InDepthFunc)
	{

	}

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
	UHRenderPassInfo()
		: UHRenderPassInfo(VK_NULL_HANDLE, UHDepthInfo(), VK_CULL_MODE_NONE, UHBlendMode::Opaque, nullptr, nullptr, 1, VK_NULL_HANDLE)
	{

	}

	// value for cullmode and blend mode is from different objects, don't set them in constructor for flexible usage
	UHRenderPassInfo(VkRenderPass InRenderPass, UHDepthInfo InDepthInfo, VkCullModeFlagBits InCullInfo, UHBlendMode InBlendMode
		, UHShader* InVS, UHShader* InPS, int32_t InRTCount, VkPipelineLayout InPipelineLayout)
		: CullMode(InCullInfo)
		, BlendMode(InBlendMode)
		, RenderPass(InRenderPass)
		, DepthInfo(InDepthInfo)
		, VS(InVS)
		, PS(InPS)
		, GS(nullptr)
		, RTCount(InRTCount)
		, PipelineLayout(InPipelineLayout)
	{

	}

	bool operator==(const UHRenderPassInfo& InInfo)
	{
		bool bVSEqual = true;
		if (InInfo.VS && VS)
		{
			bVSEqual = (*InInfo.VS == *VS);
		}

		bool bPSEqual = true;
		if (InInfo.PS && PS)
		{
			bPSEqual = (*InInfo.PS == *PS);
		}

		bool bGSEqual = true;
		if (InInfo.GS && GS)
		{
			bGSEqual = (*InInfo.GS == *GS);
		}

		return InInfo.CullMode == CullMode
			&& InInfo.BlendMode == BlendMode
			&& InInfo.RenderPass == RenderPass
			&& bVSEqual
			&& bPSEqual
			&& bGSEqual
			&& InInfo.RTCount == RTCount
			&& InInfo.PipelineLayout == PipelineLayout;
	}

	VkCullModeFlagBits CullMode;
	UHBlendMode BlendMode;
	VkRenderPass RenderPass;
	UHDepthInfo DepthInfo;
	UHShader* VS;
	UHShader* PS;
	UHShader* GS;
	int32_t RTCount;
	VkPipelineLayout PipelineLayout;
};

// ray tracing info class
struct UHRayTracingInfo
{
	UHRayTracingInfo()
		: PipelineLayout(VK_NULL_HANDLE)
		, MaxRecursionDepth(1)
		, RayGenShader(nullptr)
		, ClosestHitShader(nullptr)
		, MissShader(nullptr)
		, PayloadSize(4)
		, AttributeSize(8)
	{

	}

	bool operator==(const UHRayTracingInfo& InInfo)
	{
		if (InInfo.RayGenShader != RayGenShader)
		{
			return false;
		}

		if (InInfo.ClosestHitShader != ClosestHitShader)
		{
			return false;
		}

		if (InInfo.AnyHitShaders.size() != AnyHitShaders.size())
		{
			return false;
		}
		else
		{
			for (size_t Idx = 0; Idx < AnyHitShaders.size(); Idx++)
			{
				if (AnyHitShaders[Idx] != InInfo.AnyHitShaders[Idx])
				{
					return false;
				}
			}
		}

		if (InInfo.MissShader != MissShader)
		{
			return false;
		}

		return InInfo.PipelineLayout == PipelineLayout
			&& InInfo.MaxRecursionDepth == MaxRecursionDepth
			&& InInfo.PayloadSize == PayloadSize
			&& InInfo.AttributeSize == AttributeSize;
	}

	VkPipelineLayout PipelineLayout;
	uint32_t MaxRecursionDepth;
	UHShader* RayGenShader;
	UHShader* ClosestHitShader;
	std::vector<UHShader*> AnyHitShaders;
	UHShader* MissShader;
	uint32_t PayloadSize;
	uint32_t AttributeSize;
};

// structure for managing render pass
struct UHRenderPassObject
{
	UHRenderPassObject()
		: FrameBuffer(VK_NULL_HANDLE)
		, RenderPass(VK_NULL_HANDLE)
	{

	}

	void Release(VkDevice LogicalDevice)
	{
		vkDestroyFramebuffer(LogicalDevice, FrameBuffer, nullptr);
		vkDestroyRenderPass(LogicalDevice, RenderPass, nullptr);
	}

	void ReleaseFrameBuffer(VkDevice LogicalDevice)
	{
		// release frame buffer only, used for resizing
		vkDestroyFramebuffer(LogicalDevice, FrameBuffer, nullptr);
	}

	VkFramebuffer FrameBuffer;
	VkRenderPass RenderPass;
};

// class for recording render states (is dirty or something else?)
// use with class that needs upload gpu data (Material, Renderer, Lighting...etc)
class UHRenderState
{
public:
	UHRenderState()
		: BufferDataIndex(0)
	{
		// always dirty at the beginning
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			bIsRenderDirties[Idx] = true;
			bIsRayTracingDirties[Idx] = false;
			bIsMotionDirties[Idx] = false;
		}
	}

	void SetRenderDirties(bool bIsDirty)
	{
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			bIsRenderDirties[Idx] = bIsDirty;
		}
	}

	void SetRenderDirty(bool bIsDirty, int32_t FrameIdx)
	{
		bIsRenderDirties[FrameIdx] = bIsDirty;
	}

	void SetRayTracingDirties(bool bIsDirty)
	{
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			bIsRayTracingDirties[Idx] = bIsDirty;
		}
	}

	void SetRayTracingDirty(bool bIsDirty, int32_t FrameIdx)
	{
		bIsRayTracingDirties[FrameIdx] = bIsDirty;
	}

	void SetMotionDirties(bool bIsDirty)
	{
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			bIsMotionDirties[Idx] = bIsDirty;
		}
	}

	void SetMotionDirty(bool bIsDirty, int32_t FrameIdx)
	{
		bIsMotionDirties[FrameIdx] = bIsDirty;
	}

	bool IsRenderDirty(int32_t FrameIdx)
	{
		return bIsRenderDirties[FrameIdx];
	}

	bool IsRayTracingDirty(int32_t FrameIdx)
	{
		return bIsRayTracingDirties[FrameIdx];
	}

	bool IsMotionDirty(int32_t FrameIdx)
	{
		return bIsMotionDirties[FrameIdx];
	}

	void SetBufferDataIndex(int32_t InIndex)
	{
		BufferDataIndex = InIndex;
	}

	int32_t GetBufferDataIndex() const
	{
		return BufferDataIndex;
	}

private:
	std::array<bool, GMaxFrameInFlight> bIsRenderDirties;
	std::array<bool, GMaxFrameInFlight> bIsRayTracingDirties;
	std::array<bool, GMaxFrameInFlight> bIsMotionDirties;
	int32_t BufferDataIndex;
};