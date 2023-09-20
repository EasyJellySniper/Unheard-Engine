#include "PreviewScene.h"

#if WITH_EDITOR
#include "../../Runtime/Renderer/GraphicBuilder.h"

UHPreviewScene::UHPreviewScene(HINSTANCE InInstance, HWND InHwnd, UHGraphic* InGraphic, UHPreviewSceneType InType)
	: TargetWindow(InHwnd)
	, Gfx(InGraphic)
	, MainSurface(VK_NULL_HANDLE)
	, SwapChain(VK_NULL_HANDLE)
	, SwapChainRenderPass(VK_NULL_HANDLE)
	, SwapChainExtent(VkExtent2D())
	, SwapChainSemaphore(VK_NULL_HANDLE)
	, PreviewSceneType(InType)
	, CurrentFrame(0)
	, CurrentMip(0)
{
	// pass window handle and create surface
	VkWin32SurfaceCreateInfoKHR Win32CreateInfo{};
	Win32CreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	Win32CreateInfo.hwnd = InHwnd;
	Win32CreateInfo.hinstance = InInstance;
	if (vkCreateWin32SurfaceKHR(InGraphic->GetInstance(), &Win32CreateInfo, nullptr, &MainSurface) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create preview scene surface!.\n");
	}

	// setup format and present mode for swap chain
	UHSwapChainDetails SwapChainSupport = Gfx->QuerySwapChainSupport(Gfx->PhysicalDevice);
	VkSurfaceFormatKHR Format{ VK_FORMAT_B8G8R8A8_SRGB , VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	VkPresentModeKHR PresentMode = VK_PRESENT_MODE_FIFO_KHR;

	RECT R;
	GetClientRect(TargetWindow, &R);
	VkExtent2D Extent{ static_cast<uint32_t>(R.right - R.left), static_cast<uint32_t>(R.bottom - R.top) };
	SwapChainExtent = Extent;

	// create info for swap chain
	VkSwapchainCreateInfoKHR CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	CreateInfo.surface = MainSurface;
	CreateInfo.minImageCount = 2;
	CreateInfo.imageFormat = Format.format;
	CreateInfo.imageColorSpace = Format.colorSpace;
	CreateInfo.imageExtent = Extent;
	CreateInfo.imageArrayLayers = 1;
	CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	CreateInfo.queueFamilyIndexCount = 0;
	CreateInfo.pQueueFamilyIndices = nullptr;
	CreateInfo.preTransform = SwapChainSupport.Capabilities2.surfaceCapabilities.currentTransform;
	CreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	CreateInfo.presentMode = PresentMode;
	CreateInfo.clipped = VK_TRUE;
	CreateInfo.oldSwapchain = VK_NULL_HANDLE;

	VkDevice LogicalDevice = Gfx->GetLogicalDevice();
	if (vkCreateSwapchainKHR(LogicalDevice, &CreateInfo, nullptr, &SwapChain) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create preview scene swap chain!\n");
	}

	// store swap chain image
	vkGetSwapchainImagesKHR(LogicalDevice, SwapChain, &CreateInfo.minImageCount, nullptr);

	std::vector<VkImage> SwapChainImages;
	SwapChainImages.resize(CreateInfo.minImageCount);
	vkGetSwapchainImagesKHR(LogicalDevice, SwapChain, &CreateInfo.minImageCount, SwapChainImages.data());

	// create render pass for swap chain, it will be blit from other source, so transfer to drc_bit first
	SwapChainRenderPass = Gfx->CreateRenderPass(Format.format, UHTransitionInfo());

	// create swap chain RTs
	SwapChainRT.resize(CreateInfo.minImageCount);
	SwapChainFrameBuffer.resize(CreateInfo.minImageCount);
	for (size_t Idx = 0; Idx < CreateInfo.minImageCount; Idx++)
	{
		SwapChainRT[Idx] = Gfx->RequestRenderTexture("PreviewSceneSwapChain" + std::to_string(Idx), SwapChainImages[Idx], Extent, Format.format, false);
		SwapChainFrameBuffer[Idx] = Gfx->CreateFrameBuffer(SwapChainRT[Idx]->GetImageView(), SwapChainRenderPass, Extent);
	}

	VkSemaphoreCreateInfo SemaphoreInfo{};
	SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	if (vkCreateSemaphore(LogicalDevice, &SemaphoreInfo, nullptr, &SwapChainSemaphore) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to allocate preview scene semaphore!\n");
	}

	// init debug view shaders
	DebugViewShader = MakeUnique<UHDebugViewShader>(Gfx, "PreviewSceneDebugViewShader", SwapChainRenderPass);
	DebugViewData = Gfx->RequestRenderBuffer<uint32_t>();
	DebugViewData->CreateBuffer(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	UHSamplerInfo PointClampedInfo(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	PointClampedInfo.MaxAnisotropy = 1;
	PointClampedInfo.MipBias = 0;
	UHSampler* PointSampler = Gfx->RequestTextureSampler(PointClampedInfo);
	DebugViewShader->BindSampler(PointSampler, 2);
}

void UHPreviewScene::Release()
{
	UH_SAFE_RELEASE(DebugViewData);
	DebugViewData.reset();
	DebugViewShader->Release();
	VkDevice LogicalDevice = Gfx->GetLogicalDevice();

	for (size_t Idx = 0; Idx < SwapChainFrameBuffer.size(); Idx++)
	{
		Gfx->RequestReleaseRT(SwapChainRT[Idx]);
		vkDestroyFramebuffer(LogicalDevice, SwapChainFrameBuffer[Idx], nullptr);
	}

	vkDestroySemaphore(LogicalDevice, SwapChainSemaphore, nullptr);
	vkDestroyRenderPass(LogicalDevice, SwapChainRenderPass, nullptr);
	vkDestroySwapchainKHR(LogicalDevice, SwapChain, nullptr);
	SwapChainRT.clear();
	SwapChainFrameBuffer.clear();
	vkDestroySurfaceKHR(Gfx->GetInstance(), MainSurface, nullptr);
}

void UHPreviewScene::Render()
{
	VkCommandBuffer PreviewCmd = Gfx->BeginOneTimeCmd();
	UHGraphicBuilder PreviewBuilder(Gfx, PreviewCmd);

	uint32_t ImageIndex;
	vkAcquireNextImageKHR(Gfx->GetLogicalDevice(), SwapChain, UINT64_MAX, SwapChainSemaphore, VK_NULL_HANDLE, &ImageIndex);

	VkClearValue ClearColor = { {0.0f,0.0f,0.0f,0.0f} };
	PreviewBuilder.BeginRenderPass(SwapChainRenderPass, SwapChainFrameBuffer[ImageIndex], SwapChainExtent, ClearColor);

	// render based on the preview scene type
	if (PreviewSceneType == TexturePreview)
	{
		PreviewBuilder.SetViewport(SwapChainExtent);
		PreviewBuilder.SetScissor(SwapChainExtent);
		PreviewBuilder.BindVertexBuffer(VK_NULL_HANDLE);

		UHGraphicState* State = DebugViewShader->GetState();
		PreviewBuilder.BindGraphicState(State);
		PreviewBuilder.BindDescriptorSet(DebugViewShader->GetPipelineLayout(), DebugViewShader->GetDescriptorSet(CurrentFrame));
		PreviewBuilder.DrawFullScreenQuad();
	}

	PreviewBuilder.EndRenderPass();

	PreviewBuilder.ResourceBarrier(SwapChainRT[ImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	Gfx->EndOneTimeCmd(PreviewCmd);

	PreviewBuilder.Present(SwapChain, Gfx->GetGraphicsQueue(), VK_NULL_HANDLE, ImageIndex, -1);
	CurrentFrame = (CurrentFrame + 1) % GMaxFrameInFlight;
}

void UHPreviewScene::SetPreviewTexture(UHTexture* InTexture)
{
	Gfx->WaitGPU();
	DebugViewShader->BindImage(InTexture, 1);
}

void UHPreviewScene::SetPreviewMip(uint32_t InMip)
{
	Gfx->WaitGPU();
	CurrentMip = InMip;
	DebugViewData->UploadAllData(&CurrentMip);
	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		DebugViewShader->BindConstant(DebugViewData, 0, Idx);
	}
}

#endif