#pragma once
#include "../../UnheardEngine.h"

#if WITH_EDITOR
#include "../../Runtime/Engine/Graphic.h"
#include "../../Runtime/Renderer/ShaderClass/PostProcessing/DebugViewShader.h"

enum UHPreviewSceneType
{
	TexturePreview
};

// preview scene class for editor use, this basically create another set of surface/swap chain from gfx
class UHPreviewScene
{
public:
	UHPreviewScene(HINSTANCE InInstance, HWND InHwnd, UHGraphic* InGraphic, UHPreviewSceneType InType);
	void Release();
	void Render();

	void SetPreviewTexture(UHTexture* InTexture);
	void SetPreviewMip(uint32_t InMip);

private:
	HWND TargetWindow;
	UHGraphic* Gfx;
	VkSurfaceKHR MainSurface;
	VkSwapchainKHR SwapChain;
	std::vector<UHRenderTexture*> SwapChainRT;
	std::vector<VkFramebuffer> SwapChainFrameBuffer;
	VkRenderPass SwapChainRenderPass;
	VkExtent2D SwapChainExtent;
	VkSemaphore SwapChainSemaphore;
	UHPreviewSceneType PreviewSceneType;

	// shaders
	UniquePtr<UHRenderBuffer<uint32_t>> DebugViewData;
	UHDebugViewShader DebugViewShader;
	uint32_t CurrentMip;

	uint32_t CurrentFrame;
};

#endif