#pragma once
#include "../Engine/Graphic.h"
#include <unordered_map>
#include "ShaderClass/ShaderClass.h"
#include "ParallelSubmitter.h"

struct UHImageBarrier
{
	UHImageBarrier(UHTexture* InTexture, VkImageLayout InNewLayout, uint32_t InBaseMipLevel = 0)
		: Texture(InTexture)
		, NewLayout(InNewLayout)
		, BaseMipLevel(InBaseMipLevel)
	{

	}

	UHTexture* Texture;
	VkImageLayout NewLayout;
	uint32_t BaseMipLevel;
};

// render builder for Unheard Engine
class UHRenderBuilder
{
public:
	UHRenderBuilder(UHGraphic* InGraphic, VkCommandBuffer InCommandBuffer, bool bIsComputeBuilder = false);

	VkCommandBuffer GetCmdList();

	// wait a single fence
	void WaitFence(VkFence InFence);

	// reset a single fence
	void ResetFence(VkFence InFence);

	// get swap chain buffer of this frame
	VkFramebuffer GetCurrentSwapChainBuffer(VkSemaphore InSemaphore, uint32_t& OutImageIndex) const;

	// reset a command buffer
	void ResetCommandBuffer();

	// begin a command buffer
	void BeginCommandBuffer(VkCommandBufferInheritanceInfo* InInfo = nullptr);

	// end a command buffer
	void EndCommandBuffer();

	// begin a pass
	void BeginRenderPass(const UHRenderPassObject& InRenderPassObj, VkExtent2D InExtent, VkClearValue InClearValue
		, VkSubpassContents InSubPassContent = VK_SUBPASS_CONTENTS_INLINE);

	// begin a pass (multiple RTs)
	void BeginRenderPass(const UHRenderPassObject& InRenderPassObj, VkExtent2D InExtent, const std::vector<VkClearValue>& InClearValue
		, VkSubpassContents InSubPassContent = VK_SUBPASS_CONTENTS_INLINE);

	// begin a pass (without clearing)
	void BeginRenderPass(const UHRenderPassObject& InRenderPassObj, VkExtent2D InExtent
		, VkSubpassContents InSubPassContent = VK_SUBPASS_CONTENTS_INLINE);

	// end a pass
	void EndRenderPass();

	// execute a command buffer, single and multiple semaphore wait version
	void ExecuteCmd(VkQueue InQueue, VkFence InFence, VkSemaphore InWaitSemaphore, VkSemaphore InFinishSemaphore);
	void ExecuteCmd(VkQueue InQueue, VkFence InFence
		, const std::vector<VkSemaphore>& InWaitSemaphores
		, const std::vector<VkPipelineStageFlags>& InWaitStageFlags
		, VkSemaphore InFinishSemaphore);

	// present to swap chain
	bool Present(VkSwapchainKHR InSwapChain, VkQueue InQueue, VkSemaphore InFinishSemaphore, uint32_t InImageIdx);

	// bind states
	void BindGraphicState(UHGraphicState* InState);
	void BindRTState(const UHGraphicState* InState);
	void BindComputeState(UHComputeState* InState);

	// set viewport
	void SetViewport(VkExtent2D InExtent);

	// set scissor
	void SetScissor(VkExtent2D InExtent);

	// bind vertex buffer
	void BindVertexBuffer(VkBuffer InBuffer);

	// bind index buffer
	void BindIndexBuffer(UHMesh* InMesh);

	// draw
	void DrawVertex(uint32_t VertexCount);

	// draw index
	void DrawIndexed(uint32_t IndicesCount, bool bOcclusionTest = false);

	// bind descriptors
	void BindDescriptorSet(VkPipelineLayout InLayout, VkDescriptorSet InSet);
	void BindDescriptorSet(VkPipelineLayout InLayout, const std::vector<VkDescriptorSet>& InSets, uint32_t FirstSet = 0);
	void BindDescriptorSetCompute(VkPipelineLayout InLayout, VkDescriptorSet InSet);
	void BindRTDescriptorSet(VkPipelineLayout InLayout, const std::vector<VkDescriptorSet>& InSets);

	// transition image
	void ResourceBarrier(UHTexture* InTexture, VkImageLayout OldLayout, VkImageLayout NewLayout, uint32_t BaseMipLevel = 0, uint32_t BaseArrayLayer = 0);
	void ResourceBarrier(std::vector<UHRenderTexture*> InTextures, VkImageLayout OldLayout, VkImageLayout NewLayout, uint32_t BaseMipLevel = 0, uint32_t BaseArrayLayer = 0);
	void ResourceBarrier(std::vector<UHTexture*> InTextures, VkImageLayout OldLayout, VkImageLayout NewLayout, uint32_t BaseMipLevel = 0, uint32_t BaseArrayLayer = 0);

	// transition buffer
	void ResourceBarrier(VkBuffer InBuffer, const uint64_t BufferSize
		, VkAccessFlagBits SrcAccess, VkAccessFlagBits DstAccess, VkPipelineStageFlagBits SrcStage, VkPipelineStageFlagBits DstStage);

	void PushResourceBarrier(const UHImageBarrier InBarrier);
	void FlushResourceBarrier();

	// blit image
	void Blit(UHTexture* SrcImage, UHTexture* DstImage, VkFilter InFilter = VK_FILTER_LINEAR);
	void Blit(UHTexture* SrcImage, UHTexture* DstImage, VkExtent2D SrcExtent, VkExtent2D DstExtent, VkExtent2D DstOffset, VkFilter InFilter = VK_FILTER_LINEAR);
	void Blit(UHTexture* SrcImage, UHTexture* DstImage, VkExtent2D SrcExtent, VkExtent2D DstExtent, uint32_t SrcMip = 0, uint32_t DstMip = 0, VkFilter InFilter = VK_FILTER_LINEAR);

	// copy texture
	void CopyTexture(UHTexture* SrcImage, UHTexture* DstImage, uint32_t MipLevel = 0, uint32_t DstArray = 0, uint32_t SrcArray = 0);

	// draw full screen quad
	void DrawFullScreenQuad();

	// trace ray, support volume tracing as well
	void TraceRay(VkExtent2D InExtent, UHRenderBuffer<UHShaderRecord>* InRayGenTable, UHRenderBuffer<UHShaderRecord>* InMissTable, UHRenderBuffer<UHShaderRecord>* InHitGroupTable
		, const uint32_t InSlices = 1);

	// write time stamp
	void WriteTimeStamp(VkQueryPool InPool, uint32_t InQuery);

	// execute bundle
	void ExecuteBundles(const UHParallelSubmitter& InSubmitter);

	// clear storage buffer (must be uint32_t)
	void ClearUAVBuffer(VkBuffer InBuffer, uint32_t InValue);

	// clear image
	void ClearRenderTexture(UHRenderTexture* InTexture, VkClearColorValue InClearColor, const int32_t LayerIdx = UHINDEXNONE);

	// dispatch call
	void Dispatch(uint32_t Gx, uint32_t Gy, uint32_t Gz);
	void DispatchMesh(uint32_t Gx, uint32_t Gy, uint32_t Gz);

	// occlusion query
	void ResetOcclusionQuery(UHGPUQuery* InQuery, uint32_t Idx, uint32_t Count);
	void BeginOcclusionQuery(UHGPUQuery* InQuery, uint32_t Idx);
	void EndOcclusionQuery(UHGPUQuery* InQuery, uint32_t Idx);
	void ResetGPUQuery(UHGPUQuery* InQuery);

	// predication
	void BeginPredication(uint32_t Idx, VkBuffer InBuffer, bool bReversed = false);
	void EndPredication();

	// push constant
	void PushConstant(VkPipelineLayout InPipelineLayout, VkShaderStageFlags InShaderStageFlag, uint32_t InDataSize
		, const void* Data);

#if WITH_EDITOR
	int32_t DrawCalls;
	int32_t OccludedCalls;
#endif

private:
	VkImageMemoryBarrier SetupBarrier(UHTexture* InTexture, VkImageLayout OldLayout, VkImageLayout NewLayout
		, uint32_t BaseMipLevel, uint32_t BaseArrayLayer);

	UHGraphic* Gfx;
	VkCommandBuffer CmdList;

	VkDevice LogicalDevice;
	bool bIsCompute;
	bool bNeedSetViewport;
	bool bNeedSetScissorRect;

	// lookup table for stage flag and access flag
	static std::unordered_map<VkImageLayout, VkPipelineStageFlags> LayoutToStageFlags;
	static std::unordered_map<VkImageLayout, VkAccessFlags> LayoutToAccessFlags;
	std::vector<UHImageBarrier> ImageBarriers;

	VkExtent2D PrevViewport;
	VkExtent2D PrevScissor;
	UHGraphicState* PrevGraphicState;
	UHComputeState* PrevComputeState;
	VkBuffer PrevVertexBuffer;
	UHMesh* PrevIndexBufferSource;
};