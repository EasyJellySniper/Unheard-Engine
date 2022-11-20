#pragma once
#define VK_USE_PLATFORM_WIN32_KHR	// must be before vulkan/vulkan.h for win32 surface
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <optional>
#include "../../UnheardEngine.h"
#include "../Classes/Settings.h"
#include "../Classes/RenderTexture.h"
#include "../Classes/RenderBuffer.h"
#include "../Classes/Shader.h"
#include "../Classes/GraphicState.h"
#include "../Classes/Sampler.h"
#include "../Classes/Texture2D.h"
#include "../Classes/TextureCube.h"
#include "Asset.h"
#include "Config.h"
#include "../CoreGlobals.h"
#include "../Classes/AccelerationStructure.h"

// queue family structure
struct UHQueueFamily
{
	// optional: A C++17 feature to check if it has value. Since we can't use magic number to distinguish between the case of a value existing or not

	// graphic queue
	std::optional<uint32_t> GraphicsFamily;

	// compute queue
	std::optional<uint32_t> ComputesFamily;
};

// struct for swap chain data
struct UHSwapChainDetails
{
	UHSwapChainDetails()
		: Capabilities2(VkSurfaceCapabilities2KHR())
	{
	}

	VkSurfaceCapabilities2KHR Capabilities2;
	std::vector<VkSurfaceFormat2KHR> Formats2;
	std::vector<VkPresentModeKHR> PresentModes;
};

// struct for buffer transition info
struct UHTransitionInfo
{
	// default to clearing RT and color attachment
	UHTransitionInfo()
		: LoadOp(VK_ATTACHMENT_LOAD_OP_CLEAR)
		, StoreOp(VK_ATTACHMENT_STORE_OP_STORE)
		, InitialLayout(VK_IMAGE_LAYOUT_UNDEFINED)
		, FinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{

	}

	// override constructor for LoadOp/FinalLayout vary
	UHTransitionInfo(VkAttachmentLoadOp InLoadOp, VkImageLayout InFinalLayout)
		: LoadOp(InLoadOp)
		, StoreOp(VK_ATTACHMENT_STORE_OP_STORE)
		, InitialLayout(VK_IMAGE_LAYOUT_UNDEFINED)
		, FinalLayout(InFinalLayout)
	{

	}

	// override constructor for LoadOp/Initial Layout/FinalLayout vary
	UHTransitionInfo(VkAttachmentLoadOp InLoadOp, VkImageLayout InInitialLayout, VkImageLayout InFinalLayout)
		: LoadOp(InLoadOp)
		, StoreOp(VK_ATTACHMENT_STORE_OP_STORE)
		, InitialLayout(InInitialLayout)
		, FinalLayout(InFinalLayout)
	{

	}

	VkAttachmentLoadOp LoadOp;
	VkAttachmentStoreOp StoreOp;
	VkImageLayout InitialLayout;
	VkImageLayout FinalLayout;
};

// Unheard engine graphics class, mainly for device creation
class UHGraphic
{
public:
	UHGraphic(UHAssetManager* InAssetManager, UHConfigManager* InConfig);

	// function to init graphics
	bool InitGraphics(HWND Hwnd);

	// function to release graphics
	void Release();

	// resize swap chain
	bool ResizeSwapChain();

	// toggle full screen
	void ToggleFullScreen(bool InFullScreenState);

	// generic wait gpu function
	void WaitGPU();

	// create render pass object, allow imageless and both multiple and single creation
	VkRenderPass CreateRenderPass(UHTransitionInfo InTransitionInfo) const;
	VkRenderPass CreateRenderPass(VkFormat InFormat, UHTransitionInfo InTransitionInfo, VkFormat InDepthFormat = VK_FORMAT_UNDEFINED) const;
	VkRenderPass CreateRenderPass(std::vector<VkFormat> InFormat, UHTransitionInfo InTransitionInfo, VkFormat InDepthFormat = VK_FORMAT_UNDEFINED) const;

	// create frame buffer, imageless/single/multiple
	VkFramebuffer CreateFrameBuffer(VkRenderPass InRenderPass, VkExtent2D InExtent) const;
	VkFramebuffer CreateFrameBuffer(VkImageView InImageView, VkRenderPass InRenderPass, VkExtent2D InExtent) const;
	VkFramebuffer CreateFrameBuffer(std::vector<VkImageView> InImageView, VkRenderPass InRenderPass, VkExtent2D InExtent) const;

	// request a managed render texture
	UHRenderTexture* RequestRenderTexture(std::string InName, VkExtent2D InExtent, VkFormat InFormat, bool bIsLinear, bool bIsReadWrite = false, bool bIsShadowRT = false);
	UHRenderTexture* RequestRenderTexture(std::string InName, VkImage InImage, VkExtent2D InExtent, VkFormat InFormat, bool bIsLinear, bool bIsReadWrite = false
		, bool bIsShadowRT = false);
	void RequestReleaseRT(UHRenderTexture* InRT);

	// request a managed texture 2d/cube
	UHTexture2D* RequestTexture2D(std::string InName, std::string InSourcePath, uint32_t Width, uint32_t Height, std::vector<uint8_t> InData, bool bIsLinear = false);
	UHTextureCube* RequestTextureCube(std::string InName, std::vector<UHTexture2D*> InTextures);

	// request a managed material
	UHMaterial* RequestMaterial();
	UHMaterial* RequestMaterial(std::filesystem::path InPath);

	// request a unique render buffer, due to the template, this can not be managed by Graphic class
	template<typename T>
	std::unique_ptr<UHRenderBuffer<T>> RequestRenderBuffer() const
	{
		std::unique_ptr<UHRenderBuffer<T>> NewBuffer = std::make_unique<UHRenderBuffer<T>>();
		NewBuffer->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);
		return std::move(NewBuffer);
	}

	// request acceleration structure
	std::unique_ptr<UHAccelerationStructure> RequestAccelerationStructure();

	// request a shader reference based on input
	UHShader* RequestShader(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName
		, std::vector<std::string> InMacro = std::vector<std::string>());

	// request graphic/RT state
	UHGraphicState* RequestGraphicState(UHRenderPassInfo InInfo);
	UHGraphicState* RequestRTState(UHRayTracingInfo InInfo);

	// request a texture sampler
	UHSampler* RequestTextureSampler(UHSamplerInfo InInfo);

	// get instance
	VkInstance GetInstance() const;

	// get logical device
	VkDevice GetLogicalDevice() const;

	// get queue family
	UHQueueFamily GetQueueFamily() const;

	// get swap chain
	VkSwapchainKHR GetSwapChain() const;

	// get swap chain image
	UHRenderTexture* GetSwapChainRT(int32_t ImageIdx) const;

	// get swap chain buffer
	VkFramebuffer GetSwapChainBuffer(int32_t ImageIdx) const;

	// get extent
	VkExtent2D GetSwapChainExtent() const;

	// swap chain format
	VkFormat GetSwapChainFormat() const;

	// get swap chain render pass
	VkRenderPass GetSwapChainRenderPass() const;

	// get graphic queue
	VkQueue GetGraphicQueue() const;

	// get compute queue
	VkQueue GetComputeQueue() const;

	// get device mem props
	VkPhysicalDeviceMemoryProperties GetDeviceMemProps() const;

	// get shader record size
	uint32_t GetShaderRecordSize() const;

	// get all samplers
	std::vector<UHSampler*> GetSamplers() const;

	// debug cmd functions
	void BeginCmdDebug(VkCommandBuffer InBuffer, std::string InName);
	void EndCmdDebug(VkCommandBuffer InBuffer);

	// one-time use command buffer functions, mainly for initialization
	VkCommandBuffer BeginOneTimeCmd();
	void EndOneTimeCmd(VkCommandBuffer InBuffer);

private:

	/** ====================================================== Functions ====================================================== **/

	// debug only functions
#if WITH_DEBUG
	// check validation layer support
	bool CheckValidationLayerSupport();
#endif

	// create instance
	bool CreateInstance();

	// create device
	bool CheckDeviceExtension(VkPhysicalDevice InDevice, const std::vector<const char*>& RequiredExtensions);
	bool CreatePhysicalDevice();

	// create queue family
	bool CreateQueueFamily();

	// create logical device
	bool CreateLogicalDevice();

	// create window surface
	bool CreateWindowSurface();

	// query swap chain support
	UHSwapChainDetails QuerySwapChainSupport(VkPhysicalDevice InDevice) const;

	// clear swap chain
	void ClearSwapChain();

	// create swap chain
	bool CreateSwapChain();

	// Vulkan callback functions
	PFN_vkAcquireFullScreenExclusiveModeEXT EnterFullScreenCallback;
	PFN_vkReleaseFullScreenExclusiveModeEXT LeaveFullScreenCallback;
	PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT GetSurfacePresentModes2Callback;

#if WITH_DEBUG
	PFN_vkCmdBeginDebugUtilsLabelEXT BeginCmdDebugLabelCallback;
	PFN_vkCmdEndDebugUtilsLabelEXT EndCmdDebugLabelCallback;
#endif


	/** ====================================================== Variables ====================================================== **/

	// window cache
	HWND WindowCache;

	// vulkan instance define
	VkInstance VulkanInstance;

	// vulkan physical device (GPU to use), don't need to cleanup this since it's also destroyed by vkDestroyInstance
	VkPhysicalDevice PhysicalDevice;

	// device memory properties cache
	VkPhysicalDeviceMemoryProperties PhysicalDeviceMemoryProperties;

	// logical device of vulkan app
	VkDevice LogicalDevice;

	// main graphic queue, similar to ID3D12CommandQueue
	VkQueue GraphicsQueue;

	// main compute queue, similar to ID3D12CommandQueue
	VkQueue ComputesQueue;

	// queue family info
	UHQueueFamily QueueFamily;

	// command pool for creation
	VkCommandPool CreationCommandPool;

	// window surface class
	VkSurfaceKHR MainSurface;

	// creating swap chain
	VkSwapchainKHR SwapChain;

	// swap chain render texture
	std::vector<UHRenderTexture*> SwapChainRT;

	// swap chain frame buffer, this doesn't contain in UHRenderTexture for some reasons
	std::vector<VkFramebuffer> SwapChainFrameBuffer;

	// render pass is an object which contains frame buffer attachment info, similar to the resource transition part of D3D12
	VkRenderPass SwapChainRenderPass;

	// if to use debug validation layer 
	bool bUseValidationLayers;

	// if it's fullscreen, this variable should be kept sync with PresentationSettings.bFullScreen
	bool bIsFullScreen;

	// setup extension we need, need to enable win32 surface
	// also need properties/capabilities2 for exclusive fullscreen
	std::vector<const char*> InstanceExtensions;

	// setup device extension we need, this will be checked in physical device choices first and used in logical device creation
	std::vector<const char*> DeviceExtensions;
	std::vector<const char*> RayTracingExtensions;

	// debug only variables
#if WITH_DEBUG
	// validation layer list
	const std::vector<const char*> ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
#endif

	UHAssetManager* AssetManagerInterface;
	UHConfigManager* ConfigInterface;
	uint32_t ShaderRecordSize;

	// system managed pools
	std::vector<std::unique_ptr<UHShader>> ShaderPools;
	std::vector<std::unique_ptr<UHGraphicState>> StatePools;
	std::vector<std::unique_ptr<UHRenderTexture>> RTPools;
	std::vector<std::unique_ptr<UHSampler>> SamplerPools;
	std::vector<std::unique_ptr<UHTexture2D>> Texture2DPools;
	std::vector<std::unique_ptr<UHTextureCube>> TextureCubePools;
	std::vector<std::unique_ptr<UHMaterial>> MaterialPools;
};