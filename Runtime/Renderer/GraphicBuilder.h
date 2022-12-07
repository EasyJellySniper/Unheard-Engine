#pragma once
#include "../Engine/Graphic.h"
#include <unordered_map>
#include "ShaderClass/ShaderClass.h"

// graphic builder for Unheard Engine
class UHGraphicBuilder
{
public:
	UHGraphicBuilder(UHGraphic* InGraphic, VkCommandBuffer InCommandBuffer);

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
	void BeginCommandBuffer();

	// end a command buffer
	void EndCommandBuffer();

	// begin a pass
	void BeginRenderPass(VkRenderPass InRenderPass, VkFramebuffer InFramebuffer, VkExtent2D InExtent, VkClearValue InClearValue);

	// begin a pass (multiple RTs)
	void BeginRenderPass(VkRenderPass InRenderPass, VkFramebuffer InFramebuffer, VkExtent2D InExtent, const std::vector<VkClearValue>& InClearValue);

	// begin a pass (without clearing)
	void BeginRenderPass(VkRenderPass InRenderPass, VkFramebuffer InFramebuffer, VkExtent2D InExtent);

	// end a pass
	void EndRenderPass();

	// execute a command buffer
	void ExecuteCmd(VkFence InFence, VkSemaphore InWaitSemaphore, VkSemaphore InFinishSemaphore);

	// present to swap chain
	bool Present(VkSemaphore InFinishSemaphore, uint32_t InImageIdx);

	// bind states
	void BindGraphicState(UHGraphicState* InState);
	void BindRTState(const UHGraphicState* InState);

	// set viewport
	void SetViewport(VkExtent2D InExtent);

	// set scissor
	void SetScissor(VkExtent2D InExtent);

	// bind vertex buffer
	void BindVertexBuffer(VkBuffer InBuffer);

	// bind index buffer
	void BindIndexBuffer(UHMesh* InMesh);

	// draw index
	void DrawIndexed(uint32_t IndicesCount);

	// bind descriptors
	void BindDescriptorSet(VkPipelineLayout InLayout, VkDescriptorSet InSet);
	void BindRTDescriptorSet(VkPipelineLayout InLayout, const std::vector<VkDescriptorSet>& InSets);

	// transition image
	void ResourceBarrier(UHTexture* InTexture, VkImageLayout OldLayout, VkImageLayout NewLayout, uint32_t BaseMipLevel = 0, uint32_t BaseArrayLayer = 0);
	void ResourceBarrier(std::vector<UHTexture*> InTextures, VkImageLayout OldLayout, VkImageLayout NewLayout, uint32_t BaseMipLevel = 0, uint32_t BaseArrayLayer = 0);

	// blit image
	void Blit(UHTexture* SrcImage, UHTexture* DstImage, VkFilter InFilter = VK_FILTER_LINEAR);
	void Blit(UHTexture* SrcImage, UHTexture* DstImage, VkExtent2D SrcExtent, VkExtent2D DstExtent, uint32_t SrcMip = 0, uint32_t DstMip = 0, VkFilter InFilter = VK_FILTER_LINEAR);

	// copy texture
	void CopyTexture(UHTexture* SrcImage, UHTexture* DstImage, uint32_t MipLevel = 0, uint32_t DstArray = 0);

	// draw full screen quad
	void DrawFullScreenQuad();

	// trace ray
	void TraceRay(VkExtent2D InExtent, UHRenderBuffer<UHShaderRecord>* InRayGenTable, UHRenderBuffer<UHShaderRecord>* InHitGroupTable);

	// write time stamp
	void WriteTimeStamp(VkQueryPool InPool, uint32_t InQuery);

#if WITH_DEBUG
	int32_t DrawCalls;
#endif

private:
	UHGraphic* Gfx;
	VkCommandBuffer CmdList;
	VkDevice LogicalDevice;

	// lookup table for stage flag and access flag
	std::unordered_map<VkImageLayout, VkPipelineStageFlags> LayoutToStageFlags;
	std::unordered_map<VkImageLayout, VkAccessFlags> LayoutToAccessFlags;

	VkExtent2D PrevViewport;
	VkExtent2D PrevScissor;
	UHGraphicState* PrevGraphicState;
	VkBuffer PrevVertexBuffer;
	UHMesh* PrevIndexBufferSource;
};