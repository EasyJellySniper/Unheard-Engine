#pragma once
#include "GraphicFunction.h"
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
#include "../Classes/GPUQuery.h"
#include "Asset.h"
#include "Config.h"
#include "../CoreGlobals.h"
#include "../Classes/AccelerationStructure.h"
#include "../Classes/GPUMemory.h"

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
		, DepthLoadOp(VK_ATTACHMENT_LOAD_OP_CLEAR)
		, StoreOp(VK_ATTACHMENT_STORE_OP_STORE)
		, InitialLayout(VK_IMAGE_LAYOUT_UNDEFINED)
		, FinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{

	}

	UHTransitionInfo(VkAttachmentLoadOp InDepthLoadOp)
		: LoadOp(VK_ATTACHMENT_LOAD_OP_CLEAR)
		, DepthLoadOp(InDepthLoadOp)
		, StoreOp(VK_ATTACHMENT_STORE_OP_STORE)
		, InitialLayout(VK_IMAGE_LAYOUT_UNDEFINED)
		, FinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{

	}

	// override constructor for LoadOp/FinalLayout vary
	UHTransitionInfo(VkAttachmentLoadOp InLoadOp, VkImageLayout InFinalLayout)
		: LoadOp(InLoadOp)
		, DepthLoadOp(VK_ATTACHMENT_LOAD_OP_LOAD)
		, StoreOp(VK_ATTACHMENT_STORE_OP_STORE)
		, InitialLayout(VK_IMAGE_LAYOUT_UNDEFINED)
		, FinalLayout(InFinalLayout)
	{

	}

	// override constructor for LoadOp/Initial Layout/FinalLayout vary
	UHTransitionInfo(VkAttachmentLoadOp InLoadOp, VkImageLayout InInitialLayout, VkImageLayout InFinalLayout)
		: LoadOp(InLoadOp)
		, DepthLoadOp(VK_ATTACHMENT_LOAD_OP_LOAD)
		, StoreOp(VK_ATTACHMENT_STORE_OP_STORE)
		, InitialLayout(InInitialLayout)
		, FinalLayout(InFinalLayout)
	{

	}

	VkAttachmentLoadOp LoadOp;
	VkAttachmentLoadOp DepthLoadOp;
	VkAttachmentStoreOp StoreOp;
	VkImageLayout InitialLayout;
	VkImageLayout FinalLayout;
};

class UHEngine;
class UHPreviewScene;

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
	UHRenderPassObject CreateRenderPass(UHTransitionInfo InTransitionInfo) const;
	UHRenderPassObject CreateRenderPass(UHTexture* InFormat, UHTransitionInfo InTransitionInfo, UHTexture* InDepthFormat = nullptr) const;
	UHRenderPassObject CreateRenderPass(UHTransitionInfo InTransitionInfo, UHTexture* InDepthFormat) const;
	UHRenderPassObject CreateRenderPass(std::vector<UHTexture*> InFormat, UHTransitionInfo InTransitionInfo, UHTexture* InDepthFormat = nullptr) const;

	// create frame buffer, imageless/single/multiple
	VkFramebuffer CreateFrameBuffer(VkRenderPass InRenderPass, VkExtent2D InExtent) const;
	VkFramebuffer CreateFrameBuffer(VkImageView InImageView, VkRenderPass InRenderPass, VkExtent2D InExtent, int32_t Layers = 1) const;
	VkFramebuffer CreateFrameBuffer(std::vector<VkImageView> InImageView, VkRenderPass InRenderPass, VkExtent2D InExtent, int32_t Layers = 1) const;

	// create query pool
	UHGPUQuery* RequestGPUQuery(uint32_t Count, VkQueryType QueueType);
	void RequestReleaseGPUQuery(UHGPUQuery* InQuery);

	// request a managed render texture
	UHRenderTexture* RequestRenderTexture(std::string InName, VkExtent2D InExtent, UHTextureFormat InFormat, bool bIsReadWrite = false, bool bUseMipmap = false);
	UHRenderTexture* RequestRenderTexture(std::string InName, VkImage InImage, VkExtent2D InExtent, UHTextureFormat InFormat
		, bool bIsReadWrite = false, bool bUseMipmap = false);
	void RequestReleaseRT(UHRenderTexture* InRT);

	// request a managed texture 2d/cube
	UHTexture2D* RequestTexture2D(UniquePtr<UHTexture2D>& LoadedTex, bool bUseSharedMemory);
	void RequestReleaseTexture2D(UHTexture2D* InTex);

	UHTextureCube* RequestTextureCube(std::string InName, std::vector<UHTexture2D*> InTextures);
	UHTextureCube* RequestTextureCube(UniquePtr<UHTextureCube>& LoadedCube);
	void RequestReleaseTextureCube(UHTextureCube* InCube);

	// request a managed material
	UHMaterial* RequestMaterial();
	UHMaterial* RequestMaterial(std::filesystem::path InPath);

	// request a unique render buffer, due to the template, this can not be managed by Graphic class
	template<typename T>
	UniquePtr<UHRenderBuffer<T>> RequestRenderBuffer(uint64_t InElementCount, VkBufferUsageFlags InUsage, UHGPUMemory* SharedMemory = nullptr) const
	{
		UniquePtr<UHRenderBuffer<T>> NewBuffer = MakeUnique<UHRenderBuffer<T>>();
		NewBuffer->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

		if (SharedMemory != nullptr)
		{
			NewBuffer->CreateBuffer(InElementCount, InUsage, SharedMemory);
		}
		else
		{
			NewBuffer->CreateBuffer(InElementCount, InUsage);
		}

		return std::move(NewBuffer);
	}

	// request acceleration structure
	UniquePtr<UHAccelerationStructure> RequestAccelerationStructure();

	// request a shader reference based on input
	bool CreateShaderModule(UniquePtr<UHShader>& NewShader, std::filesystem::path OutputShaderPath);
	uint32_t RequestShader(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName
		, std::vector<std::string> InMacro = std::vector<std::string>());
	uint32_t RequestMaterialShader(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName
		, UHMaterialCompileData InData, std::vector<std::string> InMacro = std::vector<std::string>());
	void RequestReleaseShader(uint32_t InShaderID);

	// request graphic/RT state
	UHGraphicState* RequestGraphicState(UHRenderPassInfo InInfo);
	void RequestReleaseGraphicState(UHGraphicState* InState);
	UHGraphicState* RequestRTState(UHRayTracingInfo InInfo);
	UHComputeState* RequestComputeState(UHComputePassInfo InInfo);

	// request a texture sampler
	UHSampler* RequestTextureSampler(UHSamplerInfo InInfo);

	// get instance
	VkInstance GetInstance() const;

	// get physical device
	VkPhysicalDevice GetPhysicalDevice() const;

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

	// get swap chain count
	uint32_t GetSwapChainCount() const;

	// get extent
	VkExtent2D GetSwapChainExtent() const;

	// swap chain format
	VkFormat GetSwapChainFormat() const;

	// get swap chain render pass
	VkRenderPass GetSwapChainRenderPass() const;

	// get device mem props
	VkPhysicalDeviceMemoryProperties GetDeviceMemProps() const;

	// get shader record size
	uint32_t GetShaderRecordSize() const;

	// get gpu time stamp period
	float GetGPUTimeStampPeriod() const;

	bool IsDepthPrePassEnabled() const;
	bool IsRayTracingEnabled() const;
	bool IsDebugLayerEnabled() const;
	bool IsHDRAvailable() const;
	bool Is24BitDepthSupported() const;
	bool IsAMDIntegratedGPU() const;
	bool IsMeshShaderSupported() const;

	// get all samplers
	std::vector<UHSampler*> GetSamplers() const;

	// get shared memory
	UHGPUMemory* GetMeshSharedMemory() const;
	UHGPUMemory* GetImageSharedMemory() const;

	// debug cmd functions
	void BeginCmdDebug(VkCommandBuffer InBuffer, std::string InName);
	void EndCmdDebug(VkCommandBuffer InBuffer);

	// one-time use command buffer functions, mainly for initialization
	VkCommandBuffer BeginOneTimeCmd();
	void EndOneTimeCmd(VkCommandBuffer InBuffer);
	VkQueue GetGraphicsQueue() const;

#if WITH_EDITOR
	uint32_t GetMinImageCount() const;
	bool RecreateImGui();
	VkPipeline GetImGuiPipeline() const;
	void SetDepthPrepassActive(bool bInFlag);
#endif

private:

	/** ====================================================== Functions ====================================================== **/

	// debug only functions
#if WITH_EDITOR
	// check validation layer support
	bool CheckValidationLayerSupport();
#endif

	// create instance
	bool CreateInstance();

	// create device
	bool CheckDeviceExtension(VkPhysicalDevice InDevice, std::vector<const char*>& RequiredExtensions);
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
#if WITH_EDITOR
	// validation layer list
	const std::vector<const char*> ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
	VkDescriptorPool ImGuiDescriptorPool;
	VkPipeline ImGuiPipeline;
#endif

	UHAssetManager* AssetManagerInterface;
	UHConfigManager* ConfigInterface;
	uint32_t ShaderRecordSize;
	float GPUTimeStampPeriod;
	bool bEnableDepthPrePass;
	bool bEnableRayTracing;
	bool bSupportHDR;
	bool bSupport24BitDepth;
	bool bIsAMDIntegratedGPU;
	bool bSupportMeshShader;

protected:
	// system managed pools
	std::vector<UniquePtr<UHShader>> ShaderPools;
	std::vector<UniquePtr<UHGraphicState>> StatePools;
	std::vector<UniquePtr<UHRenderTexture>> RTPools;
	std::vector<UniquePtr<UHSampler>> SamplerPools;
	std::vector<UniquePtr<UHTexture2D>> Texture2DPools;
	std::vector<UniquePtr<UHTextureCube>> TextureCubePools;
	std::vector<UniquePtr<UHMaterial>> MaterialPools;
	std::vector<UniquePtr<UHGPUQuery>> QueryPools;

	// shared GPU memory
	UniquePtr<UHGPUMemory> MeshBufferSharedMemory = nullptr;
	UniquePtr<UHGPUMemory> ImageSharedMemory = nullptr;

#if WITH_EDITOR
	uint32_t MinImageCount;

	// give access for some classes in debug build
	friend UHEngine;
	friend UHPreviewScene;
#endif
};