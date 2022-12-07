#include "GraphicBuilder.h"

UHGraphicBuilder::UHGraphicBuilder(UHGraphic* InGraphic, VkCommandBuffer InCommandBuffer)
	: Gfx(InGraphic)
	, CmdList(InCommandBuffer)
	, LogicalDevice(InGraphic->GetLogicalDevice())
	, PrevViewport(VkExtent2D())
	, PrevScissor(VkExtent2D())
	, PrevGraphicState(nullptr)
	, PrevVertexBuffer(VK_NULL_HANDLE)
	, PrevIndexBufferSource(nullptr)
#if WITH_DEBUG
	, DrawCalls(0)
#endif
{
	// resource barrier lookup build, not the best way but can get rid of plenty if-else blocks
	LayoutToAccessFlags[VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL] = VK_ACCESS_TRANSFER_READ_BIT;
	LayoutToAccessFlags[VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL] = VK_ACCESS_TRANSFER_WRITE_BIT;
	LayoutToAccessFlags[VK_IMAGE_LAYOUT_PRESENT_SRC_KHR] = VK_ACCESS_NONE_KHR;
	LayoutToAccessFlags[VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL] = VK_ACCESS_NONE;
	LayoutToAccessFlags[VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL] = VK_ACCESS_SHADER_READ_BIT;
	LayoutToAccessFlags[VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL] = VK_ACCESS_NONE;
	LayoutToAccessFlags[VK_IMAGE_LAYOUT_UNDEFINED] = VK_ACCESS_NONE;
	LayoutToAccessFlags[VK_IMAGE_LAYOUT_GENERAL] = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

	LayoutToStageFlags[VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL] = VK_PIPELINE_STAGE_TRANSFER_BIT;
	LayoutToStageFlags[VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL] = VK_PIPELINE_STAGE_TRANSFER_BIT;
	LayoutToStageFlags[VK_IMAGE_LAYOUT_PRESENT_SRC_KHR] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	LayoutToStageFlags[VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	LayoutToStageFlags[VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL] = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	LayoutToStageFlags[VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL] = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	LayoutToStageFlags[VK_IMAGE_LAYOUT_UNDEFINED] = VK_PIPELINE_STAGE_TRANSFER_BIT;
	LayoutToStageFlags[VK_IMAGE_LAYOUT_GENERAL] = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
}

VkCommandBuffer UHGraphicBuilder::GetCmdList()
{
	return CmdList;
}

void UHGraphicBuilder::WaitFence(VkFence InFence)
{
	vkWaitForFences(LogicalDevice, 1, &InFence, VK_TRUE, UINT64_MAX);
}

void UHGraphicBuilder::ResetFence(VkFence InFence)
{
	vkResetFences(LogicalDevice, 1, &InFence);
}

VkFramebuffer UHGraphicBuilder::GetCurrentSwapChainBuffer(VkSemaphore InSemaphore, uint32_t& OutImageIndex) const
{
	VkSwapchainKHR SwapChain = Gfx->GetSwapChain();

	// get current swap chain index
	uint32_t ImageIndex;
	vkAcquireNextImageKHR(LogicalDevice, SwapChain, UINT64_MAX, InSemaphore, VK_NULL_HANDLE, &ImageIndex);

	OutImageIndex = ImageIndex;
	return Gfx->GetSwapChainBuffer(ImageIndex);
}

void UHGraphicBuilder::ResetCommandBuffer()
{
	vkResetCommandBuffer(CmdList, 0);
}

void UHGraphicBuilder::BeginCommandBuffer()
{
	// begin command buffer
	VkCommandBufferBeginInfo BeginInfo{};
	BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	BeginInfo.flags = 0; // Optional
	BeginInfo.pInheritanceInfo = nullptr; // Optional

	if (vkBeginCommandBuffer(CmdList, &BeginInfo) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to begin recording command buffer!\n");
	}
}

void UHGraphicBuilder::EndCommandBuffer()
{
	if (vkEndCommandBuffer(CmdList) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to record command buffer!\n");
	}
}

// begin a pass
void UHGraphicBuilder::BeginRenderPass(VkRenderPass InRenderPass, VkFramebuffer InFramebuffer, VkExtent2D InExtent, VkClearValue InClearValue)
{
	std::vector<VkClearValue> ClearValue{ InClearValue };
	BeginRenderPass(InRenderPass, InFramebuffer, InExtent, ClearValue);
}

// begin a pass
void UHGraphicBuilder::BeginRenderPass(VkRenderPass InRenderPass, VkFramebuffer InFramebuffer, VkExtent2D InExtent, const std::vector<VkClearValue>& InClearValue)
{
	// begin render pass
	VkRenderPassBeginInfo RenderPassInfo{};
	RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	RenderPassInfo.renderPass = InRenderPass;
	RenderPassInfo.framebuffer = InFramebuffer;
	RenderPassInfo.renderArea.offset = { 0, 0 };
	RenderPassInfo.renderArea.extent = InExtent;
	RenderPassInfo.clearValueCount = static_cast<uint32_t>(InClearValue.size());
	RenderPassInfo.pClearValues = InClearValue.data();

	// this should be just clearing the buffer
	vkCmdBeginRenderPass(CmdList, &RenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

// begin a pass (without clearing)
void UHGraphicBuilder::BeginRenderPass(VkRenderPass InRenderPass, VkFramebuffer InFramebuffer, VkExtent2D InExtent)
{
	std::vector<VkClearValue> ClearValue{};
	BeginRenderPass(InRenderPass, InFramebuffer, InExtent, ClearValue);
}

// end a pass
void UHGraphicBuilder::EndRenderPass()
{
	vkCmdEndRenderPass(CmdList);
}

void UHGraphicBuilder::ExecuteCmd(VkFence InFence, VkSemaphore InWaitSemaphore, VkSemaphore InFinishSemaphore)
{
	VkQueue GraphicsQueue = Gfx->GetGraphicQueue();

	// summit to queue
	VkSubmitInfo SubmitInfo{};
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	// wait available semaphore before begin to submit
	VkSemaphore WaitSemaphores[] = { InWaitSemaphore };
	VkPipelineStageFlags WaitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	SubmitInfo.waitSemaphoreCount = 1;
	SubmitInfo.pWaitSemaphores = WaitSemaphores;
	SubmitInfo.pWaitDstStageMask = WaitStages;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &CmdList;

	// the signal after finish
	VkSemaphore SignalSemaphores[] = { InFinishSemaphore };
	SubmitInfo.signalSemaphoreCount = 1;
	SubmitInfo.pSignalSemaphores = SignalSemaphores;

	// similar to D3D12CommandQueue::Execute()
	if (vkQueueSubmit(GraphicsQueue, 1, &SubmitInfo, InFence) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to submit draw command buffer!\n");
		return;
	}
}

bool UHGraphicBuilder::Present(VkSemaphore InFinishSemaphore, uint32_t InImageIdx)
{
	VkSwapchainKHR SwapChain = Gfx->GetSwapChain();
	VkQueue GraphicsQueue = Gfx->GetGraphicQueue();

	// the signal after finish
	VkSemaphore SignalSemaphores[] = { InFinishSemaphore };

	// present to swap chain, after render finish fence is signaled, it will present
	VkPresentInfoKHR PresentInfo{};
	PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	PresentInfo.waitSemaphoreCount = 1;
	PresentInfo.pWaitSemaphores = SignalSemaphores;

	VkSwapchainKHR SwapChains[] = { SwapChain };
	PresentInfo.swapchainCount = 1;
	PresentInfo.pSwapchains = SwapChains;
	PresentInfo.pImageIndices = &InImageIdx;
	PresentInfo.pResults = nullptr;

	VkResult PresentResult = vkQueuePresentKHR(GraphicsQueue, &PresentInfo);

	// return if present succeed
	if (PresentResult == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
	{
		UHE_LOG(L"Lost full screen control!\n");
		return false;
	}
	else if (PresentResult == VK_ERROR_DEVICE_LOST)
	{
		UHE_LOG(L"Device lost!\n");
		return false;
	}

	return true;
}

void UHGraphicBuilder::BindGraphicState(UHGraphicState* InState)
{
	// prevent duplicate bind, should be okay with checking pointer only
	// since each state is guaranteed to be unique during initialization
	if (InState == PrevGraphicState)
	{
		return;
	}

	vkCmdBindPipeline(CmdList, VK_PIPELINE_BIND_POINT_GRAPHICS, InState->GetGraphicPipeline());
	PrevGraphicState = InState;
}

void UHGraphicBuilder::BindRTState(const UHGraphicState* InState)
{
	vkCmdBindPipeline(CmdList, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, InState->GetRTPipeline());
}

// set viewport
void UHGraphicBuilder::SetViewport(VkExtent2D InExtent)
{
	// prevent duplicate setup
	if (PrevViewport.width == InExtent.width && PrevViewport.height == InExtent.height)
	{
		return;
	}

	VkViewport Viewport{};
	Viewport.x = 0.0f;
	Viewport.y = 0.0f;
	Viewport.width = static_cast<float>(InExtent.width);
	Viewport.height = static_cast<float>(InExtent.height);
	Viewport.minDepth = 0.0f;
	Viewport.maxDepth = 1.0f;

	vkCmdSetViewport(CmdList, 0, 1, &Viewport);
	PrevViewport = InExtent;
}

// set scissor
void UHGraphicBuilder::SetScissor(VkExtent2D InExtent)
{
	// prevent duplicate setup
	if (PrevScissor.width == InExtent.width && PrevScissor.height == InExtent.height)
	{
		return;
	}

	VkRect2D Scissor{};
	Scissor.offset = { 0, 0 };
	Scissor.extent = InExtent;
	vkCmdSetScissor(CmdList, 0, 1, &Scissor);
	PrevScissor = InExtent;
}

// bind VB
void UHGraphicBuilder::BindVertexBuffer(VkBuffer InBuffer)
{
	if (PrevVertexBuffer == InBuffer)
	{
		return;
	}

	VkBuffer VBs[] = { InBuffer };
	VkDeviceSize Offsets[] = { 0 };
	vkCmdBindVertexBuffers(CmdList, 0, 1, VBs, Offsets);
	PrevVertexBuffer = InBuffer;
}

// bind IB
void UHGraphicBuilder::BindIndexBuffer(UHMesh* InMesh)
{
	if (PrevIndexBufferSource == InMesh)
	{
		return;
	}

	// select index format based on its stride
	if (InMesh->IsIndexBufer32Bit())
	{
		vkCmdBindIndexBuffer(CmdList, InMesh->GetIndexBuffer()->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
	}
	else
	{
		vkCmdBindIndexBuffer(CmdList, InMesh->GetIndexBuffer16()->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
	}

	PrevIndexBufferSource = InMesh;
}

// draw indexed
void UHGraphicBuilder::DrawIndexed(uint32_t IndicesCount)
{
	vkCmdDrawIndexed(CmdList, IndicesCount, 1, 0, 0, 0);

#if WITH_DEBUG
	DrawCalls++;
#endif
}

void UHGraphicBuilder::BindDescriptorSet(VkPipelineLayout InLayout, VkDescriptorSet InSet)
{
	vkCmdBindDescriptorSets(CmdList, VK_PIPELINE_BIND_POINT_GRAPHICS, InLayout, 0, 1, &InSet, 0, nullptr);
}

void UHGraphicBuilder::BindRTDescriptorSet(VkPipelineLayout InLayout, const std::vector<VkDescriptorSet>& InSets)
{
	vkCmdBindDescriptorSets(CmdList, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, InLayout, 0, static_cast<uint32_t>(InSets.size()), InSets.data(), 0, nullptr);
}

void UHGraphicBuilder::ResourceBarrier(UHTexture* InTexture, VkImageLayout OldLayout, VkImageLayout NewLayout, uint32_t BaseMipLevel, uint32_t BaseArrayLayer)
{
	std::vector<UHTexture*> Tex = { InTexture };
	ResourceBarrier(Tex, OldLayout, NewLayout, BaseMipLevel, BaseArrayLayer);
}

void UHGraphicBuilder::ResourceBarrier(std::vector<UHTexture*> InTextures, VkImageLayout OldLayout, VkImageLayout NewLayout, uint32_t BaseMipLevel, uint32_t BaseArrayLayer)
{
	if (InTextures.size() == 0)
	{
		return;
	}

	std::vector<VkImageMemoryBarrier> Barriers(InTextures.size());

	for (size_t Idx = 0; Idx < InTextures.size(); Idx++)
	{
		VkImageMemoryBarrier Barrier{};
		Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		Barrier.oldLayout = OldLayout;
		Barrier.newLayout = NewLayout;
		Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		// similar to the setting in UHRenderTexture::CreateImageView, so get the image view info for setting
		VkImageViewCreateInfo ImageViewInfo = InTextures[Idx]->GetImageViewInfo();

		Barrier.image = InTextures[Idx]->GetImage();
		Barrier.subresourceRange = ImageViewInfo.subresourceRange;

		// this barrier will transition 1 mip slice only for now
		Barrier.subresourceRange.baseMipLevel = BaseMipLevel;
		Barrier.subresourceRange.levelCount = 1;
		Barrier.subresourceRange.layerCount = 1;
		Barrier.subresourceRange.baseArrayLayer = BaseArrayLayer;
		Barrier.srcAccessMask = LayoutToAccessFlags[OldLayout];
		Barrier.dstAccessMask = LayoutToAccessFlags[NewLayout];

		Barriers[Idx] = Barrier;
	}

	VkPipelineStageFlags SourceStage = LayoutToStageFlags[OldLayout];
	VkPipelineStageFlags DestStage = LayoutToStageFlags[NewLayout];

	vkCmdPipelineBarrier(
		CmdList,
		SourceStage, DestStage,
		0,
		0, nullptr,
		0, nullptr,
		static_cast<uint32_t>(Barriers.size()), Barriers.data());
}

void UHGraphicBuilder::Blit(UHTexture* SrcImage, UHTexture* DstImage, VkFilter InFilter)
{
	Blit(SrcImage, DstImage, SrcImage->GetExtent(), DstImage->GetExtent(), 0, 0, InFilter);
}

// blit with custom extent
void UHGraphicBuilder::Blit(UHTexture* SrcImage, UHTexture* DstImage, VkExtent2D SrcExtent, VkExtent2D DstExtent, uint32_t SrcMip, uint32_t DstMip, VkFilter InFilter)
{
	// setup src & dst layer before blit
	VkImageViewCreateInfo SrcInfo = SrcImage->GetImageViewInfo();
	VkImageSubresourceLayers SrcLayer{};
	SrcLayer.aspectMask = SrcInfo.subresourceRange.aspectMask;
	SrcLayer.baseArrayLayer = SrcInfo.subresourceRange.baseArrayLayer;
	SrcLayer.layerCount = SrcInfo.subresourceRange.layerCount;
	SrcLayer.mipLevel = SrcInfo.subresourceRange.baseMipLevel;

	VkImageViewCreateInfo DstInfo = DstImage->GetImageViewInfo();
	VkImageSubresourceLayers DstLayer{};
	DstLayer.aspectMask = DstInfo.subresourceRange.aspectMask;
	DstLayer.baseArrayLayer = DstInfo.subresourceRange.baseArrayLayer;
	DstLayer.layerCount = DstInfo.subresourceRange.layerCount;
	DstLayer.mipLevel = DstInfo.subresourceRange.baseMipLevel;

	// image type must be equal
	if (SrcInfo.viewType != DstInfo.viewType)
	{
		UHE_LOG(L"Inconsistent view type when calling Blit!\n");
		return;
	}

	VkImageBlit BlitInfo{};
	BlitInfo.srcSubresource = SrcLayer;
	BlitInfo.srcSubresource.mipLevel = SrcMip;
	BlitInfo.dstSubresource = DstLayer;
	BlitInfo.dstSubresource.mipLevel = DstMip;

	// offset setting
	if (SrcInfo.viewType == VK_IMAGE_VIEW_TYPE_2D)
	{
		BlitInfo.srcOffsets[1].x = SrcExtent.width;
		BlitInfo.srcOffsets[1].y = SrcExtent.height;
		BlitInfo.srcOffsets[1].z = 1;
	}

	if (DstInfo.viewType == VK_IMAGE_VIEW_TYPE_2D)
	{
		BlitInfo.dstOffsets[1].x = DstExtent.width;
		BlitInfo.dstOffsets[1].y = DstExtent.height;
		BlitInfo.dstOffsets[1].z = 1;
	}

	vkCmdBlitImage(CmdList, SrcImage->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, DstImage->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		, 1, &BlitInfo, InFilter);

#if WITH_DEBUG
	// vkCmdBlitImage should be a kind of draw call, add to profile 
	DrawCalls++;
#endif
}

void UHGraphicBuilder::CopyTexture(UHTexture* SrcImage, UHTexture* DstImage, uint32_t MipLevel, uint32_t DstArray)
{
	// copy should work on the same dimension
	if (SrcImage->GetExtent().width != DstImage->GetExtent().width || SrcImage->GetExtent().height != DstImage->GetExtent().height)
	{
		UHE_LOG(L"Dimension mismatched when copying texture!\n");
		return;
	}

	VkImageViewCreateInfo SrcInfo = SrcImage->GetImageViewInfo();
	VkImageSubresourceLayers SrcLayer{};
	SrcLayer.aspectMask = SrcInfo.subresourceRange.aspectMask;
	SrcLayer.baseArrayLayer = SrcInfo.subresourceRange.baseArrayLayer;
	SrcLayer.layerCount = SrcInfo.subresourceRange.layerCount;
	SrcLayer.mipLevel = SrcInfo.subresourceRange.baseMipLevel;

	VkImageViewCreateInfo DstInfo = DstImage->GetImageViewInfo();
	VkImageSubresourceLayers DstLayer{};
	DstLayer.aspectMask = DstInfo.subresourceRange.aspectMask;
	DstLayer.baseArrayLayer = DstInfo.subresourceRange.baseArrayLayer;
	DstLayer.layerCount = DstInfo.subresourceRange.layerCount;
	DstLayer.mipLevel = DstInfo.subresourceRange.baseMipLevel;

	// copy one mip/one layer at one time
	// doesn't need to offset here, this works differently than Blit
	VkImageCopy CopyInfo{};
	CopyInfo.srcSubresource = SrcLayer;
	CopyInfo.srcSubresource.mipLevel = MipLevel;
	CopyInfo.dstSubresource = DstLayer;
	CopyInfo.dstSubresource.mipLevel = MipLevel;
	CopyInfo.dstSubresource.layerCount = 1;
	CopyInfo.dstSubresource.baseArrayLayer = DstArray;

	CopyInfo.extent.width = SrcImage->GetExtent().width >> MipLevel;
	CopyInfo.extent.height = SrcImage->GetExtent().height >> MipLevel;
	CopyInfo.extent.depth = 1;

	vkCmdCopyImage(CmdList, SrcImage->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, DstImage->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &CopyInfo);
}

void UHGraphicBuilder::DrawFullScreenQuad()
{
	// simply draw 6 vertices, post process shader will setup with SV_VertexID
	vkCmdDraw(CmdList, 6, 1, 0, 0);

#if WITH_DEBUG
	DrawCalls++;
#endif
}

VkDeviceAddress GetDeviceAddress(VkDevice InDevice, VkBuffer InBuffer)
{
	VkBufferDeviceAddressInfo AddressInfo{};
	AddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	AddressInfo.buffer = InBuffer;

	return vkGetBufferDeviceAddress(InDevice, &AddressInfo);
}

void UHGraphicBuilder::TraceRay(VkExtent2D InExtent, UHRenderBuffer<UHShaderRecord>* InRayGenTable, UHRenderBuffer<UHShaderRecord>* InHitGroupTable)
{
	VkStridedDeviceAddressRegionKHR RayGenAddress{};
	RayGenAddress.deviceAddress = GetDeviceAddress(LogicalDevice, InRayGenTable->GetBuffer());
	RayGenAddress.size = InRayGenTable->GetBufferSize();
	RayGenAddress.stride = RayGenAddress.size;

	VkStridedDeviceAddressRegionKHR HitGroupAddress{};
	HitGroupAddress.deviceAddress = GetDeviceAddress(LogicalDevice, InHitGroupTable->GetBuffer());
	HitGroupAddress.size = InHitGroupTable->GetBufferSize();
	HitGroupAddress.stride = InHitGroupTable->GetBufferStride();

	VkStridedDeviceAddressRegionKHR NullAddress{};

	PFN_vkCmdTraceRaysKHR TraceRays = (PFN_vkCmdTraceRaysKHR)vkGetInstanceProcAddr(Gfx->GetInstance(), "vkCmdTraceRaysKHR");
	TraceRays(CmdList, &RayGenAddress, &NullAddress, &HitGroupAddress, &NullAddress, InExtent.width, InExtent.height, 1);
}

void UHGraphicBuilder::WriteTimeStamp(VkQueryPool InPool, uint32_t InQuery)
{
#if WITH_DEBUG
	vkCmdResetQueryPool(CmdList, InPool, InQuery, 1);
	vkCmdWriteTimestamp(CmdList, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, InPool, InQuery);
#endif
}