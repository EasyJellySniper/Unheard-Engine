#include "RenderBuilder.h"

std::unordered_map<VkImageLayout, VkPipelineStageFlags> UHRenderBuilder::LayoutToStageFlags;
std::unordered_map<VkImageLayout, VkAccessFlags> UHRenderBuilder::LayoutToAccessFlags;

UHRenderBuilder::UHRenderBuilder(UHGraphic* InGraphic, VkCommandBuffer InCommandBuffer, bool bIsComputeBuilder)
	: Gfx(InGraphic)
	, CmdList(InCommandBuffer)
	, LogicalDevice(InGraphic ? InGraphic->GetLogicalDevice() : nullptr)
	, bIsCompute(bIsComputeBuilder)
	, PrevViewport(VkExtent2D())
	, PrevScissor(VkExtent2D())
	, PrevGraphicState(nullptr)
	, PrevComputeState(nullptr)
	, PrevVertexBuffer(nullptr)
	, PrevIndexBufferSource(nullptr)
#if WITH_EDITOR
	, DrawCalls(0)
	, OccludedCalls(0)
#endif
	, bNeedSetViewport(false)
	, bNeedSetScissorRect(false)
{
	// resource barrier lookup build, not the best way but can get rid of plenty if-else blocks
	if (LayoutToAccessFlags.size() == 0)
	{
		LayoutToAccessFlags[VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL] = VK_ACCESS_TRANSFER_READ_BIT;
		LayoutToAccessFlags[VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL] = VK_ACCESS_TRANSFER_WRITE_BIT;
		LayoutToAccessFlags[VK_IMAGE_LAYOUT_PRESENT_SRC_KHR] = VK_ACCESS_NONE_KHR;
		LayoutToAccessFlags[VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL] = VK_ACCESS_NONE;
		LayoutToAccessFlags[VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL] = VK_ACCESS_SHADER_READ_BIT;
		LayoutToAccessFlags[VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL] = VK_ACCESS_NONE;
		LayoutToAccessFlags[VK_IMAGE_LAYOUT_UNDEFINED] = VK_ACCESS_NONE;
		LayoutToAccessFlags[VK_IMAGE_LAYOUT_GENERAL] = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
	}

	if (LayoutToStageFlags.size() == 0)
	{
		LayoutToStageFlags[VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL] = VK_PIPELINE_STAGE_TRANSFER_BIT;
		LayoutToStageFlags[VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL] = VK_PIPELINE_STAGE_TRANSFER_BIT;
		LayoutToStageFlags[VK_IMAGE_LAYOUT_PRESENT_SRC_KHR] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		LayoutToStageFlags[VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL] = (bIsCompute) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		LayoutToStageFlags[VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL] = (bIsCompute) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		LayoutToStageFlags[VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL] = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		LayoutToStageFlags[VK_IMAGE_LAYOUT_UNDEFINED] = VK_PIPELINE_STAGE_TRANSFER_BIT;
		LayoutToStageFlags[VK_IMAGE_LAYOUT_GENERAL] = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	}
}

VkCommandBuffer UHRenderBuilder::GetCmdList()
{
	return CmdList;
}

void UHRenderBuilder::WaitFence(VkFence InFence)
{
	vkWaitForFences(LogicalDevice, 1, &InFence, VK_TRUE, UINT64_MAX);
}

void UHRenderBuilder::ResetFence(VkFence InFence)
{
	vkResetFences(LogicalDevice, 1, &InFence);
}

VkFramebuffer UHRenderBuilder::GetCurrentSwapChainBuffer(VkSemaphore InSemaphore, uint32_t& OutImageIndex) const
{
	VkSwapchainKHR SwapChain = Gfx->GetSwapChain();

	// get current swap chain index
	uint32_t ImageIndex;
	vkAcquireNextImageKHR(LogicalDevice, SwapChain, UINT64_MAX, InSemaphore, nullptr, &ImageIndex);

	OutImageIndex = ImageIndex;
	return Gfx->GetSwapChainBuffer(ImageIndex);
}

void UHRenderBuilder::ResetCommandBuffer()
{
	vkResetCommandBuffer(CmdList, 0);
}

void UHRenderBuilder::BeginCommandBuffer(VkCommandBufferInheritanceInfo* InInfo)
{
	// begin command buffer
	VkCommandBufferBeginInfo BeginInfo{};
	BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	// assume secondary cmd is in render pass
	if (InInfo != nullptr)
	{
		BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		if (InInfo->renderPass != nullptr)
		{
			BeginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
		}
		BeginInfo.pInheritanceInfo = InInfo;
	}

	if (vkBeginCommandBuffer(CmdList, &BeginInfo) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to begin recording command buffer!\n");
	}
}

void UHRenderBuilder::EndCommandBuffer()
{
	if (vkEndCommandBuffer(CmdList) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to record command buffer!\n");
	}
}

// begin a pass
void UHRenderBuilder::BeginRenderPass(const UHRenderPassObject& InRenderPassObj, VkExtent2D InExtent, VkClearValue InClearValue
	, VkSubpassContents InSubPassContent)
{
	std::vector<VkClearValue> ClearValue{ InClearValue };
	BeginRenderPass(InRenderPassObj, InExtent, ClearValue, InSubPassContent);
}

// begin a pass
void UHRenderBuilder::BeginRenderPass(const UHRenderPassObject& InRenderPassObj, VkExtent2D InExtent, const std::vector<VkClearValue>& InClearValue
	, VkSubpassContents InSubPassContent)
{
	// begin render pass
	VkRenderPassBeginInfo RenderPassInfo{};
	RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	RenderPassInfo.renderPass = InRenderPassObj.RenderPass;
	RenderPassInfo.framebuffer = InRenderPassObj.FrameBuffer;
	RenderPassInfo.renderArea.offset = { 0, 0 };
	RenderPassInfo.renderArea.extent = InExtent;
	RenderPassInfo.clearValueCount = static_cast<uint32_t>(InClearValue.size());
	RenderPassInfo.pClearValues = InClearValue.data();

	// this should be just clearing the buffer
	vkCmdBeginRenderPass(CmdList, &RenderPassInfo, InSubPassContent);

	// set to new layout
	for (UHTexture* Tex : InRenderPassObj.ColorTextures)
	{
		Tex->SetImageLayout(InRenderPassObj.FinalLayout);
	}

	if (InRenderPassObj.DepthTexture)
	{
		InRenderPassObj.DepthTexture->SetImageLayout(InRenderPassObj.FinalDepthLayout);
	}
}

// begin a pass (without clearing)
void UHRenderBuilder::BeginRenderPass(const UHRenderPassObject& InRenderPassObj, VkExtent2D InExtent
	, VkSubpassContents InSubPassContent)
{
	std::vector<VkClearValue> ClearValue{};
	BeginRenderPass(InRenderPassObj, InExtent, ClearValue, InSubPassContent);
}

// end a pass
void UHRenderBuilder::EndRenderPass()
{
	vkCmdEndRenderPass(CmdList);
}

void UHRenderBuilder::ExecuteCmd(VkQueue InQueue, VkFence InFence, VkSemaphore InWaitSemaphore, VkSemaphore InFinishSemaphore)
{
	std::vector<VkSemaphore> WaitSemaphores;
	if (InWaitSemaphore != nullptr)
	{
		WaitSemaphores.push_back(InWaitSemaphore);
	}

	const VkPipelineStageFlags WaitStage = (bIsCompute) ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	std::vector<VkPipelineStageFlags> WaitStages = { WaitStage };
	ExecuteCmd(InQueue, InFence, WaitSemaphores, WaitStages, InFinishSemaphore);
}

void UHRenderBuilder::ExecuteCmd(VkQueue InQueue, VkFence InFence
	, const std::vector<VkSemaphore>& InWaitSemaphores
	, const std::vector<VkPipelineStageFlags>& InWaitStageFlags
	, VkSemaphore InFinishSemaphore)
{
	// summit to queue
	VkSubmitInfo SubmitInfo{};
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	// wait available semaphore before begin to submit
	if (InWaitSemaphores.size() > 0)
	{
		SubmitInfo.waitSemaphoreCount = static_cast<uint32_t>(InWaitSemaphores.size());
		SubmitInfo.pWaitSemaphores = InWaitSemaphores.data();
		SubmitInfo.pWaitDstStageMask = InWaitStageFlags.data();
	}

	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &CmdList;

	// the signal after finish
	if (InFinishSemaphore != nullptr)
	{
		SubmitInfo.signalSemaphoreCount = 1;
		SubmitInfo.pSignalSemaphores = &InFinishSemaphore;
	}

	// similar to D3D12CommandQueue::Execute()
	if (vkQueueSubmit(InQueue, 1, &SubmitInfo, InFence) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to submit draw command buffer!\n");
		return;
	}
}

bool UHRenderBuilder::Present(VkSwapchainKHR InSwapChain, VkQueue InQueue, VkSemaphore InFinishSemaphore, uint32_t InImageIdx)
{
	// present to swap chain, after render finish fence is signaled, it will present
	VkPresentInfoKHR PresentInfo{};
	PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	if (InFinishSemaphore != nullptr)
	{
		PresentInfo.waitSemaphoreCount = 1;
		PresentInfo.pWaitSemaphores = &InFinishSemaphore;
	}

	PresentInfo.swapchainCount = 1;
	PresentInfo.pSwapchains = &InSwapChain;
	PresentInfo.pImageIndices = &InImageIdx;
	PresentInfo.pResults = nullptr;

	VkResult PresentResult = vkQueuePresentKHR(InQueue, &PresentInfo);

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

void UHRenderBuilder::BindGraphicState(UHGraphicState* InState)
{
	// prevent duplicate bind, should be okay with checking pointer only
	// since each state is guaranteed to be unique during initialization
	if (InState == PrevGraphicState)
	{
		return;
	}

	vkCmdBindPipeline(CmdList, VK_PIPELINE_BIND_POINT_GRAPHICS, InState->GetPassPipeline());
	PrevGraphicState = InState;
}

void UHRenderBuilder::BindRTState(const UHGraphicState* InState)
{
	vkCmdBindPipeline(CmdList, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, InState->GetRTPipeline());
}

void UHRenderBuilder::BindComputeState(UHComputeState* InState)
{
	// prevent duplicate bind, should be okay with checking pointer only
	// since each state is guaranteed to be unique during initialization
	if (InState == PrevComputeState)
	{
		return;
	}

	vkCmdBindPipeline(CmdList, VK_PIPELINE_BIND_POINT_COMPUTE, InState->GetPassPipeline());
	PrevComputeState = InState;
}

// set viewport
void UHRenderBuilder::SetViewport(VkExtent2D InExtent)
{
	// prevent duplicate setup
	if (PrevViewport.width == InExtent.width && PrevViewport.height == InExtent.height
		&& !bNeedSetViewport)
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
void UHRenderBuilder::SetScissor(VkExtent2D InExtent)
{
	// prevent duplicate setup
	if (PrevScissor.width == InExtent.width && PrevScissor.height == InExtent.height
		&& !bNeedSetScissorRect)
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
void UHRenderBuilder::BindVertexBuffer(VkBuffer InBuffer)
{
	// need to set VB as null if it's really null
	if (PrevVertexBuffer == InBuffer && InBuffer != nullptr)
	{
		return;
	}

	VkBuffer VBs[] = { InBuffer };
	VkDeviceSize Offsets[] = { 0 };
	vkCmdBindVertexBuffers(CmdList, 0, 1, VBs, Offsets);
	PrevVertexBuffer = InBuffer;
}

// bind IB
void UHRenderBuilder::BindIndexBuffer(UHMesh* InMesh)
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

void UHRenderBuilder::DrawVertex(uint32_t VertexCount)
{
	vkCmdDraw(CmdList, VertexCount, 1, 0, 0);

#if WITH_EDITOR
	DrawCalls++;
#endif
}

// draw indexed
void UHRenderBuilder::DrawIndexed(uint32_t IndicesCount, bool bOcclusionTest)
{
	vkCmdDrawIndexed(CmdList, IndicesCount, 1, 0, 0, 0);

#if WITH_EDITOR
	if (bOcclusionTest)
	{
		OccludedCalls++;
	}
	else
	{
		DrawCalls++;
	}
#endif
}

void UHRenderBuilder::BindDescriptorSet(VkPipelineLayout InLayout, VkDescriptorSet InSet)
{
	vkCmdBindDescriptorSets(CmdList, VK_PIPELINE_BIND_POINT_GRAPHICS, InLayout, 0, 1, &InSet, 0, nullptr);
}

void UHRenderBuilder::BindDescriptorSet(VkPipelineLayout InLayout, const std::vector<VkDescriptorSet>& InSets, uint32_t FirstSet)
{
	vkCmdBindDescriptorSets(CmdList, VK_PIPELINE_BIND_POINT_GRAPHICS, InLayout, FirstSet, static_cast<uint32_t>(InSets.size()), InSets.data(), 0, nullptr);
}

void UHRenderBuilder::BindDescriptorSetCompute(VkPipelineLayout InLayout, VkDescriptorSet InSet)
{
	vkCmdBindDescriptorSets(CmdList, VK_PIPELINE_BIND_POINT_COMPUTE, InLayout, 0, 1, &InSet, 0, nullptr);
}

void UHRenderBuilder::BindRTDescriptorSet(VkPipelineLayout InLayout, const std::vector<VkDescriptorSet>& InSets)
{
	vkCmdBindDescriptorSets(CmdList, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, InLayout, 0, static_cast<uint32_t>(InSets.size()), InSets.data(), 0, nullptr);
}

// ResourceBarrier: Single transition version
void UHRenderBuilder::ResourceBarrier(UHTexture* InTexture, VkImageLayout OldLayout, VkImageLayout NewLayout, uint32_t BaseMipLevel, uint32_t BaseArrayLayer)
{
	std::vector<UHTexture*> Tex = { InTexture };
	ResourceBarrier(Tex, OldLayout, NewLayout, BaseMipLevel, BaseArrayLayer);
}

// ResourceBarrier: Multiple textures but the same layout transition
void UHRenderBuilder::ResourceBarrier(std::vector<UHTexture*> InTextures, VkImageLayout OldLayout, VkImageLayout NewLayout, uint32_t BaseMipLevel, uint32_t BaseArrayLayer)
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
		InTextures[Idx]->SetImageLayout(NewLayout, BaseMipLevel);
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

void UHRenderBuilder::ResourceBarrier(VkBuffer InBuffer, const uint64_t BufferSize
	, VkAccessFlagBits SrcAccess, VkAccessFlagBits DstAccess, VkPipelineStageFlagBits SrcStage, VkPipelineStageFlagBits DstStage)
{
	VkBufferMemoryBarrier Barrier{};
	Barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	Barrier.buffer = InBuffer;
	Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	Barrier.size = BufferSize;
	Barrier.srcAccessMask = SrcAccess;
	Barrier.dstAccessMask = DstAccess;

	vkCmdPipelineBarrier(
		CmdList,
		SrcStage, DstStage,
		0,
		0, nullptr,
		1, &Barrier,
		0, nullptr);
}

// ResourceBarrier: Can push different textures and layouts at once
void UHRenderBuilder::PushResourceBarrier(const UHImageBarrier InBarrier)
{
	VkImageLayout OldLayout = InBarrier.Texture->GetImageLayout(InBarrier.BaseMipLevel);
	if (OldLayout == InBarrier.NewLayout)
	{
		// no need to transition the same layout
		return;
	}

	// flag to fallback to old resource barrier, for debug only
	const bool bFallbackToOldResourceBarrier = false;
	if (bFallbackToOldResourceBarrier)
	{
		ResourceBarrier(InBarrier.Texture, InBarrier.Texture->GetImageLayout(InBarrier.BaseMipLevel), InBarrier.NewLayout, InBarrier.BaseMipLevel);
		return;
	}

	ImageBarriers.push_back(InBarrier);
}

void UHRenderBuilder::FlushResourceBarrier()
{
	if (ImageBarriers.size() == 0)
	{
		return;
	}

	VkDependencyInfo DependencyInfo{};
	DependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	DependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(ImageBarriers.size());
	
	std::vector<VkImageMemoryBarrier2> Barriers(ImageBarriers.size());
	for (size_t Idx = 0; Idx < ImageBarriers.size(); Idx++)
	{
		VkImageMemoryBarrier2 TempBarrier{};
		TempBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

		// old layout must be properly set.
		VkImageLayout OldLayout = ImageBarriers[Idx].Texture->GetImageLayout(ImageBarriers[Idx].BaseMipLevel);
		TempBarrier.oldLayout = OldLayout;
		TempBarrier.newLayout = ImageBarriers[Idx].NewLayout;
		TempBarrier.srcAccessMask = LayoutToAccessFlags[TempBarrier.oldLayout];
		TempBarrier.dstAccessMask = LayoutToAccessFlags[TempBarrier.newLayout];
		TempBarrier.srcStageMask = LayoutToStageFlags[TempBarrier.oldLayout];
		TempBarrier.dstStageMask = LayoutToStageFlags[TempBarrier.newLayout];
		TempBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		TempBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		TempBarrier.image = ImageBarriers[Idx].Texture->GetImage();
		TempBarrier.subresourceRange = ImageBarriers[Idx].Texture->GetImageViewInfo().subresourceRange;

		// this barrier will transition 1 mip slice only for now
		TempBarrier.subresourceRange.baseMipLevel = ImageBarriers[Idx].BaseMipLevel;
		TempBarrier.subresourceRange.levelCount = 1;
		TempBarrier.subresourceRange.layerCount = 1;

		ImageBarriers[Idx].Texture->SetImageLayout(TempBarrier.newLayout, ImageBarriers[Idx].BaseMipLevel);
		Barriers[Idx] = std::move(TempBarrier);
	}
	DependencyInfo.pImageMemoryBarriers = Barriers.data();

	vkCmdPipelineBarrier2(CmdList, &DependencyInfo);
	ImageBarriers.clear();
}

void UHRenderBuilder::Blit(UHTexture* SrcImage, UHTexture* DstImage, VkFilter InFilter)
{
	Blit(SrcImage, DstImage, SrcImage->GetExtent(), DstImage->GetExtent(), 0, 0, InFilter);
}

// blit for custom dst offset
void UHRenderBuilder::Blit(UHTexture* SrcImage, UHTexture* DstImage, VkExtent2D SrcExtent, VkExtent2D DstExtent, VkExtent2D DstOffset, VkFilter InFilter)
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
	BlitInfo.dstSubresource = DstLayer;

	// offset setting
	if (SrcInfo.viewType == VK_IMAGE_VIEW_TYPE_2D)
	{
		BlitInfo.srcOffsets[1].x = SrcExtent.width;
		BlitInfo.srcOffsets[1].y = SrcExtent.height;
		BlitInfo.srcOffsets[1].z = 1;
	}

	if (DstInfo.viewType == VK_IMAGE_VIEW_TYPE_2D)
	{
		BlitInfo.dstOffsets[0].x = DstOffset.width;
		BlitInfo.dstOffsets[0].y = DstOffset.height;

		BlitInfo.dstOffsets[1].x = DstOffset.width + DstExtent.width;
		BlitInfo.dstOffsets[1].y = DstOffset.height + DstExtent.height;
		BlitInfo.dstOffsets[1].z = 1;
	}

	vkCmdBlitImage(CmdList, SrcImage->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, DstImage->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		, 1, &BlitInfo, InFilter);

#if WITH_EDITOR
	// vkCmdBlitImage should be a kind of draw call, add to profile 
	DrawCalls++;
#endif
}

// blit with custom extent
void UHRenderBuilder::Blit(UHTexture* SrcImage, UHTexture* DstImage, VkExtent2D SrcExtent, VkExtent2D DstExtent, uint32_t SrcMip, uint32_t DstMip, VkFilter InFilter)
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
		UHE_LOG(L"[UHRenderBuilder::Blit] Inconsistent view type when calling Blit!\n");
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

#if WITH_EDITOR
	// vkCmdBlitImage should be a kind of draw call, add to profile 
	DrawCalls++;
#endif
}

void UHRenderBuilder::CopyTexture(UHTexture* SrcImage, UHTexture* DstImage, uint32_t MipLevel, uint32_t DstArray, uint32_t SrcArray)
{
	// copy should work on the same dimension
	if (SrcImage->GetExtent().width != DstImage->GetExtent().width || SrcImage->GetExtent().height != DstImage->GetExtent().height)
	{
		UHE_LOG(L"[UHRenderBuilder::CopyTexture] Dimension mismatched when copying texture!\n");
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
	CopyInfo.srcSubresource.layerCount = 1;
	CopyInfo.srcSubresource.baseArrayLayer = SrcArray;
	CopyInfo.dstSubresource = DstLayer;
	CopyInfo.dstSubresource.mipLevel = MipLevel;
	CopyInfo.dstSubresource.layerCount = 1;
	CopyInfo.dstSubresource.baseArrayLayer = DstArray;

	CopyInfo.extent.width = SrcImage->GetExtent().width >> MipLevel;
	CopyInfo.extent.height = SrcImage->GetExtent().height >> MipLevel;
	CopyInfo.extent.depth = 1;

	vkCmdCopyImage(CmdList, SrcImage->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, DstImage->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &CopyInfo);
}

void UHRenderBuilder::DrawFullScreenQuad()
{
	// simply draw 6 vertices, post process shader will setup with SV_VertexID
	vkCmdDraw(CmdList, 6, 1, 0, 0);

#if WITH_EDITOR
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

void UHRenderBuilder::TraceRay(VkExtent2D InExtent, UHRenderBuffer<UHShaderRecord>* InRayGenTable, UHRenderBuffer<UHShaderRecord>* InMissTable
	, UHRenderBuffer<UHShaderRecord>* InHitGroupTable)
{
	VkStridedDeviceAddressRegionKHR RayGenAddress{};
	RayGenAddress.deviceAddress = GetDeviceAddress(LogicalDevice, InRayGenTable->GetBuffer());
	RayGenAddress.size = InRayGenTable->GetBufferSize();
	RayGenAddress.stride = RayGenAddress.size;

	VkStridedDeviceAddressRegionKHR MissAddress{};
	MissAddress.deviceAddress = GetDeviceAddress(LogicalDevice, InMissTable->GetBuffer());
	MissAddress.size = InMissTable->GetBufferSize();
	MissAddress.stride = MissAddress.size;

	VkStridedDeviceAddressRegionKHR HitGroupAddress{};
	HitGroupAddress.deviceAddress = GetDeviceAddress(LogicalDevice, InHitGroupTable->GetBuffer());
	HitGroupAddress.size = InHitGroupTable->GetBufferSize();
	HitGroupAddress.stride = InHitGroupTable->GetBufferStride();

	VkStridedDeviceAddressRegionKHR NullAddress{};
	GVkCmdTraceRaysKHR(CmdList, &RayGenAddress, &MissAddress, &HitGroupAddress, &NullAddress, InExtent.width, InExtent.height, 1);
}

void UHRenderBuilder::WriteTimeStamp(VkQueryPool InPool, uint32_t InQuery)
{
#if WITH_EDITOR
	vkCmdResetQueryPool(CmdList, InPool, InQuery, 1);
	vkCmdWriteTimestamp(CmdList, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, InPool, InQuery);
#endif
}

void UHRenderBuilder::ExecuteBundles(const UHParallelSubmitter& InSubmitter)
{
	if (InSubmitter.WorkerBundles.size() == 0)
	{
		return;
	}

	// push secondary cmds
	vkCmdExecuteCommands(CmdList, static_cast<uint32_t>(InSubmitter.WorkerBundles.size()), InSubmitter.WorkerBundles.data());

	// after execute secondary commands, the viewport/scissor rect might be undefined for certain hardwares
	// mark reset viewport/scissor rect for them
	bNeedSetViewport = true;
	bNeedSetScissorRect = true;
}

void UHRenderBuilder::ClearUAVBuffer(VkBuffer InBuffer, uint32_t InValue)
{
	vkCmdFillBuffer(CmdList, InBuffer, 0, VK_WHOLE_SIZE, InValue);
}

void UHRenderBuilder::ClearRenderTexture(UHRenderTexture* InTexture)
{
	VkClearColorValue ClearColor = { {0.0f,0.0f,0.0f,0.0f} };
	VkImageSubresourceRange Range = InTexture->GetImageViewInfo().subresourceRange;
	// always clears the first mip for now
	Range.levelCount = 1;
	vkCmdClearColorImage(CmdList, InTexture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ClearColor, 1, &Range);
}

void UHRenderBuilder::Dispatch(uint32_t Gx, uint32_t Gy, uint32_t Gz)
{
	vkCmdDispatch(CmdList, Gx, Gy, Gz);
}

void UHRenderBuilder::DispatchMesh(uint32_t Gx, uint32_t Gy, uint32_t Gz)
{
	GVkCmdDrawMeshTasksEXT(CmdList, Gx, Gy, Gz);
#if WITH_EDITOR
	DrawCalls++;
#endif
}

// occlusion query functions
void UHRenderBuilder::ResetOcclusionQuery(UHGPUQuery* InQuery, uint32_t Idx, uint32_t Count)
{
	if (InQuery == nullptr)
	{
		return;
	}

	vkCmdResetQueryPool(CmdList, InQuery->GetQueryPool(), Idx, Count);
}

void UHRenderBuilder::BeginOcclusionQuery(UHGPUQuery* InQuery, uint32_t Idx)
{
	if (InQuery == nullptr)
	{
		return;
	}

	// set VK_QUERY_CONTROL_PRECISE_BIT when necessary
	vkCmdBeginQuery(CmdList, InQuery->GetQueryPool(), Idx, 0);
}

void UHRenderBuilder::EndOcclusionQuery(UHGPUQuery* InQuery, uint32_t Idx)
{
	if (InQuery == nullptr)
	{
		return;
	}

	vkCmdEndQuery(CmdList, InQuery->GetQueryPool(), Idx);
}

void UHRenderBuilder::BeginPredication(uint32_t Idx, VkBuffer InBuffer, bool bReversed)
{
	VkConditionalRenderingBeginInfoEXT Info{};
	Info.sType = VK_STRUCTURE_TYPE_CONDITIONAL_RENDERING_BEGIN_INFO_EXT;
	Info.offset = Idx * sizeof(uint32_t);
	Info.buffer = InBuffer;
	Info.flags = bReversed ? VK_CONDITIONAL_RENDERING_INVERTED_BIT_EXT : 0;

	GVkCmdBeginConditionalRenderingEXT(CmdList, &Info);
}

void UHRenderBuilder::EndPredication()
{
	GVkCmdEndConditionalRenderingEXT(CmdList);
}

void UHRenderBuilder::ResetGPUQuery(UHGPUQuery* InQuery)
{
	vkResetQueryPool(LogicalDevice, InQuery->GetQueryPool(), 0, InQuery->GetQueryCount());
}