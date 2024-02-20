#include "RenderingTypes.h"

// ---------------------------------------------------- UHDepthInfo
UHDepthInfo::UHDepthInfo()
	: UHDepthInfo(true, true, VK_COMPARE_OP_GREATER)
{

}

UHDepthInfo::UHDepthInfo(bool bInEnableDepthTest, bool bInEnableDepthWrite, VkCompareOp InDepthFunc)
	: bEnableDepthTest(bInEnableDepthTest)
	, bEnableDepthWrite(bInEnableDepthWrite)
	, DepthFunc(InDepthFunc)
{

}


// ---------------------------------------------------- UHRenderPassInfo
UHRenderPassInfo::UHRenderPassInfo()
	: UHRenderPassInfo(nullptr, UHDepthInfo(), UHCullMode::CullNone, UHBlendMode::Opaque, -1, -1, 1, nullptr)
{

}

// value for cullmode and blend mode is from different objects, don't set them in constructor for flexible usage
UHRenderPassInfo::UHRenderPassInfo(VkRenderPass InRenderPass, UHDepthInfo InDepthInfo, UHCullMode InCullInfo, UHBlendMode InBlendMode
	, uint32_t InVS, uint32_t InPS, int32_t InRTCount, VkPipelineLayout InPipelineLayout)
	: CullMode(InCullInfo)
	, BlendMode(InBlendMode)
	, RenderPass(InRenderPass)
	, DepthInfo(InDepthInfo)
	, VS(InVS)
	, PS(InPS)
	, GS(-1)
	, RTCount(InRTCount)
	, PipelineLayout(InPipelineLayout)
	, bDrawLine(false)
	, bDrawWireFrame(false)
	, bForceBlendOff(false)
{

}

bool UHRenderPassInfo::operator==(const UHRenderPassInfo& InInfo)
{
	bool bVSEqual = true;
	bVSEqual = (InInfo.VS == VS);

	bool bPSEqual = true;
	bPSEqual = (InInfo.PS == PS);

	bool bGSEqual = true;
	bGSEqual = (InInfo.GS == GS);

	return InInfo.CullMode == CullMode
		&& InInfo.BlendMode == BlendMode
		&& InInfo.RenderPass == RenderPass
		&& bVSEqual
		&& bPSEqual
		&& bGSEqual
		&& InInfo.RTCount == RTCount
		&& InInfo.PipelineLayout == PipelineLayout
		&& InInfo.bDrawLine == bDrawLine
		&& InInfo.bDrawWireFrame == bDrawWireFrame;
}


// ---------------------------------------------------- UHComputePassInfo
UHComputePassInfo::UHComputePassInfo()
	: CS(-1)
	, PipelineLayout(nullptr)
{

}

UHComputePassInfo::UHComputePassInfo(VkPipelineLayout InPipelineLayout)
	: CS(-1)
	, PipelineLayout(InPipelineLayout)
{

}

bool UHComputePassInfo::operator==(const UHComputePassInfo& InInfo)
{
	bool bCSEqual = true;
	bCSEqual = (InInfo.CS == CS);

	return bCSEqual && InInfo.PipelineLayout == PipelineLayout;
}


// ---------------------------------------------------- UHRayTracingInfo
UHRayTracingInfo::UHRayTracingInfo()
	: PipelineLayout(nullptr)
	, MaxRecursionDepth(1)
	, RayGenShader(-1)
	, MissShader(-1)
	, PayloadSize(4)
	, AttributeSize(8)
{

}

bool UHRayTracingInfo::operator==(const UHRayTracingInfo& InInfo)
{
	if (InInfo.RayGenShader != RayGenShader)
	{
		return false;
	}

	if (InInfo.ClosestHitShaders.size() != ClosestHitShaders.size())
	{
		return false;
	}
	else
	{
		for (size_t Idx = 0; Idx < ClosestHitShaders.size(); Idx++)
		{
			if (ClosestHitShaders[Idx] != InInfo.ClosestHitShaders[Idx])
			{
				return false;
			}
		}
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


// ---------------------------------------------------- UHRenderPassObject
UHRenderPassObject::UHRenderPassObject()
	: FrameBuffer(nullptr)
	, RenderPass(nullptr)
	, FinalLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	, FinalDepthLayout(VK_IMAGE_LAYOUT_UNDEFINED)
	, DepthTexture(nullptr)
{

}

void UHRenderPassObject::Release(VkDevice LogicalDevice)
{
	vkDestroyFramebuffer(LogicalDevice, FrameBuffer, nullptr);
	vkDestroyRenderPass(LogicalDevice, RenderPass, nullptr);
	FrameBuffer = nullptr;
	RenderPass = nullptr;
}

void UHRenderPassObject::ReleaseRenderPass(VkDevice LogicalDevice)
{
	vkDestroyRenderPass(LogicalDevice, RenderPass, nullptr);
	RenderPass = nullptr;
}

void UHRenderPassObject::ReleaseFrameBuffer(VkDevice LogicalDevice)
{
	// release frame buffer only, used for resizing
	vkDestroyFramebuffer(LogicalDevice, FrameBuffer, nullptr);
	FrameBuffer = nullptr;
}


// ---------------------------------------------------- UHRenderState
UHRenderState::UHRenderState()
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

void UHRenderState::SetRenderDirties(bool bIsDirty)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		bIsRenderDirties[Idx] = bIsDirty;
	}
}

void UHRenderState::SetRenderDirty(bool bIsDirty, int32_t FrameIdx)
{
	bIsRenderDirties[FrameIdx] = bIsDirty;
}

void UHRenderState::SetRayTracingDirties(bool bIsDirty)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		bIsRayTracingDirties[Idx] = bIsDirty;
	}
}

void UHRenderState::SetRayTracingDirty(bool bIsDirty, int32_t FrameIdx)
{
	bIsRayTracingDirties[FrameIdx] = bIsDirty;
}

void UHRenderState::SetMotionDirties(bool bIsDirty)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		bIsMotionDirties[Idx] = bIsDirty;
	}
}

void UHRenderState::SetMotionDirty(bool bIsDirty, int32_t FrameIdx)
{
	bIsMotionDirties[FrameIdx] = bIsDirty;
}

bool UHRenderState::IsRenderDirty(int32_t FrameIdx) const
{
	return bIsRenderDirties[FrameIdx];
}

bool UHRenderState::IsRayTracingDirty(int32_t FrameIdx) const
{
	return bIsRayTracingDirties[FrameIdx];
}

bool UHRenderState::IsMotionDirty(int32_t FrameIdx) const
{
	return bIsMotionDirties[FrameIdx];
}

void UHRenderState::SetBufferDataIndex(int32_t InIndex)
{
	BufferDataIndex = InIndex;
}

int32_t UHRenderState::GetBufferDataIndex() const
{
	return BufferDataIndex;
}