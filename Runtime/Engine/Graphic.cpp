#include "../../framework.h"
#include "Graphic.h"
#include <sstream>	// file stream
#include <limits>	// std::numeric_limits
#include <algorithm> // for clamp
#include "../Classes/Utility.h"
#include "../Classes/AssetPath.h"

UHGraphic::UHGraphic(UHAssetManager* InAssetManager, UHConfigManager* InConfig)
	: ComputesQueue(VK_NULL_HANDLE)
	, EnterFullScreenCallback(VK_NULL_HANDLE)
	, GetSurfacePresentModes2Callback(VK_NULL_HANDLE)
	, GraphicsQueue(VK_NULL_HANDLE)
	, CreationCommandPool(VK_NULL_HANDLE)
	, LeaveFullScreenCallback(VK_NULL_HANDLE)
#if WITH_DEBUG
	, BeginCmdDebugLabelCallback(VK_NULL_HANDLE)
	, EndCmdDebugLabelCallback(VK_NULL_HANDLE)
#endif
	, LogicalDevice(VK_NULL_HANDLE)
	, SwapChainRenderPass(VK_NULL_HANDLE)
	, MainSurface(VK_NULL_HANDLE)
	, PhysicalDevice(VK_NULL_HANDLE)
	, PhysicalDeviceMemoryProperties(VkPhysicalDeviceMemoryProperties())
	, SwapChain(VK_NULL_HANDLE)
	, VulkanInstance(VK_NULL_HANDLE)
	, WindowCache(nullptr)
	, bIsFullScreen(false)
	, bUseValidationLayers(false)
	, AssetManagerInterface(InAssetManager)
	, ConfigInterface(InConfig)
{
	// extension defines, hard code for now
	InstanceExtensions = { "VK_KHR_surface"
		, "VK_KHR_win32_surface"
		, "VK_KHR_get_surface_capabilities2"
		, "VK_KHR_get_physical_device_properties2" };

	DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME
		, "VK_EXT_full_screen_exclusive"
		, "VK_KHR_spirv_1_4"
		, "VK_KHR_shader_float_controls"
		, "VK_EXT_robustness2" };

	RayTracingExtensions = { "VK_KHR_deferred_host_operations"
		, "VK_KHR_acceleration_structure"
		, "VK_KHR_ray_tracing_pipeline"
		, "VK_KHR_ray_query"
		, "VK_KHR_pipeline_library" };

	// push ray tracing extension
	if (GEnableRayTracing)
	{
		DeviceExtensions.insert(DeviceExtensions.end(), RayTracingExtensions.begin(), RayTracingExtensions.end());
	}
}

// init graphics
bool UHGraphic::InitGraphics(HWND Hwnd)
{
#if WITH_DEBUG
	bUseValidationLayers = ConfigInterface->RenderingSetting().bEnableLayerValidation;
#else
	bUseValidationLayers = false;
#endif

	// variable setting
	WindowCache = Hwnd;

	bool bInitSuccess = CreateInstance()
		&& CreatePhysicalDevice()
		&& CreateWindowSurface()
		&& CreateQueueFamily()
		&& CreateLogicalDevice()
		&& CreateSwapChain();

	if (bInitSuccess)
	{
		// allocate shared GPU memory if initialization succeed
		GGPUImageMemory = std::make_unique<UHGPUMemory>();
		GGPUMeshBufferMemory = std::make_unique<UHGPUMemory>();

		GGPUImageMemory->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);
		GGPUMeshBufferMemory->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

		GGPUImageMemory->AllocateMemory(static_cast<uint64_t>(ConfigInterface->EngineSetting().ImageMemoryBudgetMB) * 1048576, 1);
		GGPUMeshBufferMemory->AllocateMemory(static_cast<uint64_t>(ConfigInterface->EngineSetting().MeshBufferMemoryBudgetMB) * 1048576, 2);
	}

	return bInitSuccess;
}

// release graphics
void UHGraphic::Release()
{
	// wait device to finish before release
	WaitGPU();

	if (bIsFullScreen)
	{
		LeaveFullScreenCallback(LogicalDevice, SwapChain);
		bIsFullScreen = false;
	}

	WindowCache = nullptr;
	GraphicsQueue = nullptr;
	ComputesQueue = nullptr;

	// release all shaders
	for (auto& Shader : ShaderPools)
	{
		Shader->Release();
		Shader.reset();
	}
	ShaderPools.clear();

	// release all states
	for (auto& State : StatePools)
	{
		State->Release();
		State.reset();
	}
	StatePools.clear();

	// release all RTs
	ClearSwapChain();
	for (auto& RT : RTPools)
	{
		RT->Release();
		RT.reset();
	}
	RTPools.clear();

	// release all samplers
	for (auto& Sampler : SamplerPools)
	{
		Sampler->Release();
		Sampler.reset();
	}
	SamplerPools.clear();

	// relase all texture 2D
	for (auto& Tex : Texture2DPools)
	{
		Tex->ReleaseCPUTextureData();
		Tex->Release();
		Tex.reset();
	}
	Texture2DPools.clear();

	for (auto& Cube : TextureCubePools)
	{
		Cube->Release();
		Cube.reset();
	}
	TextureCubePools.clear();

	// release all materials
	for (auto& Mat : MaterialPools)
	{
		Mat.reset();
	}
	MaterialPools.clear();

	// release all queries
	for (auto& Query : QueryPools)
	{
		Query->Release();
		Query.reset();
	}
	QueryPools.clear();

	// release GPU memory pool
	GGPUImageMemory->Release();
	GGPUImageMemory.reset();
	GGPUMeshBufferMemory->Release();
	GGPUMeshBufferMemory.reset();

	vkDestroyCommandPool(LogicalDevice, CreationCommandPool, nullptr);
	vkDestroySurfaceKHR(VulkanInstance, MainSurface, nullptr);
	vkDestroyDevice(LogicalDevice, nullptr);
	vkDestroyInstance(VulkanInstance, nullptr);
}

// debug only functions
#if WITH_DEBUG

// check validation layer support
bool UHGraphic::CheckValidationLayerSupport()
{
	uint32_t LayerCount;
	vkEnumerateInstanceLayerProperties(&LayerCount, nullptr);

	std::vector<VkLayerProperties> AvailableLayers(LayerCount);
	vkEnumerateInstanceLayerProperties(&LayerCount, AvailableLayers.data());

	bool LayerFound = false;
	for (const char* LayerName : ValidationLayers)
	{
		for (const auto& LayerProperties : AvailableLayers)
		{
			if (strcmp(LayerName, LayerProperties.layerName) == 0)
			{
				LayerFound = true;
				break;
			}
		}
	}

	if (!LayerFound) 
	{
		return false;
	}
	return true;
}

#endif

bool CheckInstanceExtension(const std::vector<const char*>& RequiredExtensions)
{
	uint32_t ExtensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, nullptr);
	std::vector<VkExtensionProperties> Extensions(ExtensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, Extensions.data());

	// count every extensions
	uint32_t InExtensionCount = static_cast<uint32_t>(RequiredExtensions.size());
	uint32_t Count = 0;

	for (uint32_t Idx = 0; Idx < InExtensionCount; Idx++)
	{
		bool bSupported = false;
		for (uint32_t Jdx = 0; Jdx < ExtensionCount; Jdx++)
		{
			if (strcmp(RequiredExtensions[Idx], Extensions[Jdx].extensionName) == 0)
			{
				Count++;
				bSupported = true;
				break;
			}
		}

		if (!bSupported)
		{
			UHE_LOG(L"Unsupport instance extension detected: " + UHUtilities::ToStringW(RequiredExtensions[Idx]) + L"\n");
		}
	}

	// only return true if required extension is supported
	if (InExtensionCount == Count)
	{
		return true;
	}

	UHE_LOG(L"Unsupport instance extension detected!\n");
	return false;
}

bool UHGraphic::CreateInstance()
{
	// prepare app info
	VkApplicationInfo AppInfo{};
	AppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	AppInfo.pApplicationName = ENGINE_NAME;
	AppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	AppInfo.pEngineName = ENGINE_NAME;
	AppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	AppInfo.apiVersion = VK_API_VERSION_1_3;

	// create vk instance
	VkInstanceCreateInfo CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	CreateInfo.pApplicationInfo = &AppInfo;

	// set up validation layer if it's debugging
#if WITH_DEBUG
	if (bUseValidationLayers && CheckValidationLayerSupport())
	{
		CreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
		CreateInfo.ppEnabledLayerNames = ValidationLayers.data();
	}
#endif

#if WITH_DEBUG
	InstanceExtensions.push_back("VK_EXT_debug_utils");
#endif

	if (CheckInstanceExtension(InstanceExtensions))
	{
		CreateInfo.enabledExtensionCount = static_cast<uint32_t>(InstanceExtensions.size());
		CreateInfo.ppEnabledExtensionNames = InstanceExtensions.data();
	}

	VkResult CreateResult = vkCreateInstance(&CreateInfo, nullptr, &VulkanInstance);

	// print the failure reason for instance creation
	if (CreateResult != VK_SUCCESS)
	{
		UHE_LOG(L"Vulkan instance creation failed!\n");
		return false;
	}

	// get necessary function callback after instance is created
	EnterFullScreenCallback = (PFN_vkAcquireFullScreenExclusiveModeEXT)vkGetInstanceProcAddr(VulkanInstance, "vkAcquireFullScreenExclusiveModeEXT");
	LeaveFullScreenCallback = (PFN_vkReleaseFullScreenExclusiveModeEXT)vkGetInstanceProcAddr(VulkanInstance, "vkReleaseFullScreenExclusiveModeEXT");
	GetSurfacePresentModes2Callback = (PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT)vkGetInstanceProcAddr(VulkanInstance, "vkGetPhysicalDeviceSurfacePresentModes2EXT");

#if WITH_DEBUG
	BeginCmdDebugLabelCallback = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(VulkanInstance, "vkCmdBeginDebugUtilsLabelEXT");
	EndCmdDebugLabelCallback = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(VulkanInstance, "vkCmdEndDebugUtilsLabelEXT");
#endif

	return true;
}

bool UHGraphic::CheckDeviceExtension(VkPhysicalDevice InDevice, const std::vector<const char*>& RequiredExtensions)
{
	uint32_t ExtensionCount;
	vkEnumerateDeviceExtensionProperties(InDevice, nullptr, &ExtensionCount, nullptr);

	std::vector<VkExtensionProperties> AvailableExtensions(ExtensionCount);
	vkEnumerateDeviceExtensionProperties(InDevice, nullptr, &ExtensionCount, AvailableExtensions.data());

	// count every extensions
	uint32_t InExtensionCount = static_cast<uint32_t>(RequiredExtensions.size());
	uint32_t Count = 0;

	for (uint32_t Idx = 0; Idx < InExtensionCount; Idx++)
	{
		bool bSupported = false;
		for (uint32_t Jdx = 0; Jdx < ExtensionCount; Jdx++)
		{
			if (strcmp(RequiredExtensions[Idx], AvailableExtensions[Jdx].extensionName) == 0)
			{
				Count++;
				bSupported = true;
				break;
			}
		}

		if (!bSupported)
		{
			UHE_LOG(L"Unsupport device extension detected: " + UHUtilities::ToStringW(RequiredExtensions[Idx]) + L"\n");
			if (UHUtilities::FindByElement(RayTracingExtensions, RequiredExtensions[Idx]))
			{
				UHE_LOG(L"Ray tracing not supported!\n");
				GEnableRayTracing = false;
			}
		}
	}

	// only return true if required extension is supported
	if (InExtensionCount == Count)
	{
		return true;
	}

	UHE_LOG(L"Unsupport device extension detected!\n");
	return false;
}

bool UHGraphic::CreatePhysicalDevice()
{
	// enum devices
	uint32_t DeviceCount = 0;
	vkEnumeratePhysicalDevices(VulkanInstance, &DeviceCount, nullptr);
	if (DeviceCount == 0)
	{
		UHE_LOG(L"Failed to find GPUs with Vulkan support!\n");
		return false;
	}

	// actually collect devices
	std::vector<VkPhysicalDevice> Devices(DeviceCount);
	vkEnumeratePhysicalDevices(VulkanInstance, &DeviceCount, Devices.data());

	// choose a suitable device
	for (uint32_t Idx = 0; Idx < DeviceCount; Idx++)
	{
		// use device properties 2
		VkPhysicalDeviceProperties2 DeviceProperties{};
		DeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		vkGetPhysicalDeviceProperties2(Devices[Idx], &DeviceProperties);
		UHE_LOG(L"Trying GPU device: " + UHUtilities::ToStringW(DeviceProperties.properties.deviceName) + L"\n");

		// choose 1st available GPU for use
		if ((DeviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
			|| DeviceProperties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
			&& CheckDeviceExtension(Devices[Idx], DeviceExtensions))
		{
			PhysicalDevice = Devices[Idx];
			std::wostringstream Msg;
			Msg << L"Selected device: " << DeviceProperties.properties.deviceName << std::endl;
			UHE_LOG(Msg.str());
			break;
		}
	}

	if (PhysicalDevice == nullptr)
	{
		UHE_LOG(L"Failed to find a suitable GPU!\n");
		return false;
	}

	// request memory props after creation
	vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &PhysicalDeviceMemoryProperties);

	return true;
}

bool UHGraphic::CreateQueueFamily()
{
	uint32_t QueueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyCount, QueueFamilies.data());

	// choose queue family, find both graphic queue and compute queue for now
	for (uint32_t Idx = 0; Idx < QueueFamilyCount; Idx++)
	{
		// consider present support
		VkBool32 PresentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, Idx, MainSurface, &PresentSupport);

		// consider swap chain support
		bool SwapChainAdequate = false;
		UHSwapChainDetails SwapChainSupport = QuerySwapChainSupport(PhysicalDevice);
		SwapChainAdequate = !SwapChainSupport.Formats2.empty() && !SwapChainSupport.PresentModes.empty();

		if (PresentSupport && SwapChainAdequate)
		{
			if (QueueFamilies[Idx].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				QueueFamily.GraphicsFamily = Idx;
			}
			else if (QueueFamilies[Idx].queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				QueueFamily.ComputesFamily = Idx;
			}
		}
	}

	if (!QueueFamily.GraphicsFamily.has_value())
	{
		UHE_LOG(L"Failed to create graphic queue!\n");
		return false;
	}

	if (!QueueFamily.ComputesFamily.has_value())
	{
		UHE_LOG(L"Failed to create compute queue!\n");
		return false;
	}

	return true;
}

bool UHGraphic::CreateLogicalDevice()
{
	// graphic queue
	float QueuePriority = 1.0f;
	VkDeviceQueueCreateInfo GraphicQueueCreateInfo{};
	GraphicQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	GraphicQueueCreateInfo.queueFamilyIndex = QueueFamily.GraphicsFamily.value();
	GraphicQueueCreateInfo.queueCount = 1;
	GraphicQueueCreateInfo.pQueuePriorities = &QueuePriority;

	// compute queue
	VkDeviceQueueCreateInfo ComputeQueueCreateInfo{};
	ComputeQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	ComputeQueueCreateInfo.queueFamilyIndex = QueueFamily.ComputesFamily.value();
	ComputeQueueCreateInfo.queueCount = 1;
	ComputeQueueCreateInfo.pQueuePriorities = &QueuePriority;

	VkDeviceQueueCreateInfo QueueCreateInfo[2] = { GraphicQueueCreateInfo, ComputeQueueCreateInfo };

	// define features, enable what I need in UH
	VkPhysicalDeviceFeatures DeviceFeatures{};
	DeviceFeatures.samplerAnisotropy = true;
	DeviceFeatures.fullDrawIndexUint32 = true;

	// check ray tracing & AS & ray query feature
	VkPhysicalDeviceAccelerationStructureFeaturesKHR ASFeatures{};
	ASFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

	VkPhysicalDeviceRayQueryFeaturesKHR RQFeatures{};
	RQFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
	RQFeatures.pNext = &ASFeatures;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR RTFeatures{};
	RTFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	RTFeatures.pNext = &RQFeatures;

	// 1_2 runtime features
	VkPhysicalDeviceVulkan12Features Vk12Features{};
	Vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	Vk12Features.pNext = &RTFeatures;

	VkPhysicalDeviceRobustness2FeaturesEXT RobustnessFeatures{};
	RobustnessFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
	RobustnessFeatures.pNext = &Vk12Features;

	// device feature needs to assign in fature 2
	VkPhysicalDeviceFeatures2 PhyFeatures{};
	PhyFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	PhyFeatures.features = DeviceFeatures;
	PhyFeatures.pNext = &RobustnessFeatures;

	vkGetPhysicalDeviceFeatures2(PhysicalDevice, &PhyFeatures);
	if (!RTFeatures.rayTracingPipeline)
	{
		UHE_LOG(L"Ray tracing pipeline not supported. System won't render ray tracing effects.\n");
		GEnableRayTracing = false;
	}

	// get RT feature props
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR RTPropsFeatures{};
	RTPropsFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

	VkPhysicalDeviceProperties2 Props2{};
	Props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	Props2.pNext = &RTPropsFeatures;
	vkGetPhysicalDeviceProperties2(PhysicalDevice, &Props2);
	ShaderRecordSize = RTPropsFeatures.shaderGroupHandleSize;
	GPUTimeStampPeriod = Props2.properties.limits.timestampPeriod;

	// device create info, pass raytracing feature to pNext of create info
	VkDeviceCreateInfo CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	CreateInfo.pQueueCreateInfos = &QueueCreateInfo[0];
	CreateInfo.queueCreateInfoCount = 2;
	CreateInfo.pEnabledFeatures = nullptr;
	CreateInfo.pNext = &PhyFeatures;

	uint32_t ExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
	if (ExtensionCount > 0)
	{
		CreateInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
		CreateInfo.ppEnabledExtensionNames = DeviceExtensions.data();
	}

	// set up validation layer if it's debugging
#if WITH_DEBUG
	if (bUseValidationLayers && CheckValidationLayerSupport())
	{
		CreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
		CreateInfo.ppEnabledLayerNames = ValidationLayers.data();
	}
#endif

	VkResult CreateResult = vkCreateDevice(PhysicalDevice, &CreateInfo, nullptr, &LogicalDevice);
	if (CreateResult != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create Vulkan device!\n");
		return false;
	}

	// finally, get both graphics and computes queue
	vkGetDeviceQueue(LogicalDevice, QueueFamily.GraphicsFamily.value(), 0, &GraphicsQueue);
	vkGetDeviceQueue(LogicalDevice, QueueFamily.ComputesFamily.value(), 0, &ComputesQueue);

	return true;
}

bool UHGraphic::CreateWindowSurface()
{
	// pass window handle and create surface
	VkWin32SurfaceCreateInfoKHR CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	CreateInfo.hwnd = WindowCache;
	CreateInfo.hinstance = GetModuleHandle(nullptr);

	if (vkCreateWin32SurfaceKHR(VulkanInstance, &CreateInfo, nullptr, &MainSurface) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create window surface!.\n");
		return false;
	}

	return true;
}

UHSwapChainDetails UHGraphic::QuerySwapChainSupport(VkPhysicalDevice InDevice) const
{
	// query swap chain details from chosen physical device, so we can know what format is supported
	UHSwapChainDetails Details;

	VkSurfaceFullScreenExclusiveInfoEXT FullScreenInfo{};
	FullScreenInfo.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;

	// prepare win32 full screen for capabilities
	VkSurfaceFullScreenExclusiveWin32InfoEXT Win32FullScreenInfo{};
	Win32FullScreenInfo.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT;
	Win32FullScreenInfo.hmonitor = MonitorFromWindow(WindowCache, MONITOR_DEFAULTTOPRIMARY);

	// try to get surface 2
	VkPhysicalDeviceSurfaceInfo2KHR Surface2Info{};
	Surface2Info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
	Surface2Info.surface = MainSurface;
	Surface2Info.pNext = &Win32FullScreenInfo;
	
	Details.Capabilities2.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
	vkGetPhysicalDeviceSurfaceCapabilities2KHR(InDevice, &Surface2Info, &Details.Capabilities2);

	// find format
	uint32_t FormatCount;
	vkGetPhysicalDeviceSurfaceFormats2KHR(InDevice, &Surface2Info, &FormatCount, nullptr);

	if (FormatCount != 0)
	{
		Details.Formats2.resize(FormatCount);
		for (uint32_t Idx = 0; Idx < FormatCount; Idx++)
		{
			Details.Formats2[Idx].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
		}

		vkGetPhysicalDeviceSurfaceFormats2KHR(InDevice, &Surface2Info, &FormatCount, Details.Formats2.data());
	}

	// find present mode
	uint32_t PresentModeCount;
	GetSurfacePresentModes2Callback(InDevice, &Surface2Info, &PresentModeCount, nullptr);

	if (PresentModeCount != 0)
	{
		Details.PresentModes.resize(PresentModeCount);
		GetSurfacePresentModes2Callback(InDevice, &Surface2Info, &PresentModeCount, Details.PresentModes.data());
	}

	return Details;
}

VkSurfaceFormatKHR ChooseSwapChainFormat(const UHSwapChainDetails& Details)
{
	// for now, choose non linear SRGB format
	// even use R10G10B10A2_UNORM, I need linear to gamma conversion, so just let it be converted by hardware
	for (const auto& AvailableFormat : Details.Formats2)
	{
		if (AvailableFormat.surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && AvailableFormat.surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return AvailableFormat.surfaceFormat;
		}
	}

	return Details.Formats2[0].surfaceFormat;
}

VkPresentModeKHR ChooseSwapChainMode(const UHSwapChainDetails& Details, bool bUseVsync)
{
	// VK_PRESENT_MODE_IMMEDIATE_KHR: Fastest but might have screen tearing
	// VK_PRESENT_MODE_FIFO_KHR: vertical blank
	// VK_PRESENT_MODE_FIFO_RELAXED_KHR: Instead of waiting for the next vertical blank
	// , the image is transferred right away when it finally arrives. This may result in visible tearing
	// VK_PRESENT_MODE_MAILBOX_KHR: Similar to triple buffering

	// select the present mode I want
	for (const auto& AvailablePresentMode : Details.PresentModes)
	{
		if (AvailablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR && !bUseVsync)
		{
			return AvailablePresentMode;
		}
		else if (AvailablePresentMode == VK_PRESENT_MODE_FIFO_KHR && bUseVsync)
		{
			return AvailablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseSwapChainExtent(const UHSwapChainDetails& Details, HWND WindowCache)
{
	// return size directly if it's already acquired by Vulkan
	if (Details.Capabilities2.surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return Details.Capabilities2.surfaceCapabilities.currentExtent;
	}

	RECT Rect;
	int32_t Width = 0;
	int32_t Height = 0;

	if (GetWindowRect(WindowCache, &Rect))
	{
		Width = Rect.right - Rect.left;
		Height = Rect.bottom - Rect.top;
	}

	// ensure to clamp the resolution size
	VkExtent2D ActualExtent = { static_cast<uint32_t>(Width), static_cast<uint32_t>(Height) };
	ActualExtent.width = std::clamp(ActualExtent.width, Details.Capabilities2.surfaceCapabilities.minImageExtent.width, Details.Capabilities2.surfaceCapabilities.maxImageExtent.width);
	ActualExtent.height = std::clamp(ActualExtent.height, Details.Capabilities2.surfaceCapabilities.minImageExtent.height, Details.Capabilities2.surfaceCapabilities.maxImageExtent.height);

	return ActualExtent;
}

void UHGraphic::ClearSwapChain()
{
	for (size_t Idx = 0; Idx < SwapChainFrameBuffer.size(); Idx++)
	{
		RequestReleaseRT(SwapChainRT[Idx]);
		vkDestroyFramebuffer(LogicalDevice, SwapChainFrameBuffer[Idx], nullptr);
	}

	vkDestroyRenderPass(LogicalDevice, SwapChainRenderPass, nullptr);
	vkDestroySwapchainKHR(LogicalDevice, SwapChain, nullptr);
	SwapChainRT.clear();
	SwapChainFrameBuffer.clear();
}

bool UHGraphic::ResizeSwapChain()
{
	// wait device before resize
	WaitGPU();
	ClearSwapChain();

	return CreateSwapChain();
}

void UHGraphic::ToggleFullScreen(bool InFullScreenState)
{
	if (bIsFullScreen == InFullScreenState)
	{
		return;
	}

	// wait before toggling
	WaitGPU();

	if (!bIsFullScreen)
	{
		if (EnterFullScreenCallback(LogicalDevice, SwapChain) != VK_SUCCESS)
		{
			UHE_LOG(L"Enter fullscreen failed!\n");
			return;
		}
	}
	else
	{
		if (LeaveFullScreenCallback(LogicalDevice, SwapChain) != VK_SUCCESS)
		{
			UHE_LOG(L"Leave fullscreen failed!\n");
			return;
		}
	}

	bIsFullScreen = !bIsFullScreen;
}

void UHGraphic::WaitGPU()
{
	vkDeviceWaitIdle(LogicalDevice);
}

// create render pass, imageless
VkRenderPass UHGraphic::CreateRenderPass(UHTransitionInfo InTransitionInfo) const
{
	std::vector<VkFormat> Format{};
	return CreateRenderPass(Format, InTransitionInfo);
}

// create render pass, single format
VkRenderPass UHGraphic::CreateRenderPass(VkFormat InFormat, UHTransitionInfo InTransitionInfo, VkFormat InDepthFormat) const
{
	std::vector<VkFormat> Format{ InFormat };
	return CreateRenderPass(Format, InTransitionInfo, InDepthFormat);
}

// create render pass, multiple formats are possible
VkRenderPass UHGraphic::CreateRenderPass(std::vector<VkFormat> InFormat, UHTransitionInfo InTransitionInfo, VkFormat InDepthFormat) const
{
	VkRenderPass NewRenderPass = VK_NULL_HANDLE;
	uint32_t RTCount = static_cast<uint32_t>(InFormat.size());
	bool bHasDepthAttachment = (InDepthFormat != VK_FORMAT_UNDEFINED);

	std::vector<VkAttachmentDescription> ColorAttachments;
	std::vector<VkAttachmentReference> ColorAttachmentRefs;

	ColorAttachments.resize(RTCount);
	ColorAttachmentRefs.resize(RTCount);

	for (size_t Idx = 0; Idx < RTCount; Idx++)
	{
		// create color attachment, this part desides how RT is going to be used
		VkAttachmentDescription ColorAttachment{};
		ColorAttachment.format = InFormat[Idx];
		ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		ColorAttachment.loadOp = InTransitionInfo.LoadOp;
		ColorAttachment.storeOp = InTransitionInfo.StoreOp;
		ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		ColorAttachment.initialLayout = InTransitionInfo.InitialLayout;
		ColorAttachment.finalLayout = InTransitionInfo.FinalLayout;
		ColorAttachments[Idx] = ColorAttachment;

		// define attachment ref cor color attachment
		VkAttachmentReference ColorAttachmentRef{};
		ColorAttachmentRef.attachment = static_cast<uint32_t>(Idx);
		ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		ColorAttachmentRefs[Idx] = ColorAttachmentRef;
	}

	// subpass desc
	VkSubpassDescription Subpass{};
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.colorAttachmentCount = RTCount;
	Subpass.pColorAttachments = ColorAttachmentRefs.data();

	// consider depth attachment
	VkAttachmentDescription DepthAttachment{};
	VkAttachmentReference DepthAttachmentRef{};
	if (bHasDepthAttachment)
	{
		DepthAttachment.format = InDepthFormat;
		DepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		DepthAttachment.loadOp = InTransitionInfo.LoadOp;
		DepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		DepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		DepthAttachment.initialLayout = (DepthAttachment.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
		DepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		
		DepthAttachmentRef.attachment = RTCount;
		DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		Subpass.pDepthStencilAttachment = &DepthAttachmentRef;
		ColorAttachments.push_back(DepthAttachment);
	}

	// setup subpass dependency, similar to resource transition
	VkSubpassDependency Dependency{};
	Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	Dependency.dstSubpass = 0;
	Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	Dependency.srcAccessMask = 0;
	Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	if (bHasDepthAttachment)
	{
		// adjust dependency
		Dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		Dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		Dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}

	// collect the desc above 
	VkRenderPassCreateInfo RenderPassInfo{};
	RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	RenderPassInfo.attachmentCount = (bHasDepthAttachment) ? RTCount + 1 : RTCount;
	RenderPassInfo.pAttachments = ColorAttachments.data();
	RenderPassInfo.subpassCount = 1;
	RenderPassInfo.pSubpasses = &Subpass;
	RenderPassInfo.dependencyCount = 1;
	RenderPassInfo.pDependencies = &Dependency;

	if (vkCreateRenderPass(LogicalDevice, &RenderPassInfo, nullptr, &NewRenderPass) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create render pass\n");
	}

	return NewRenderPass;
}

VkFramebuffer UHGraphic::CreateFrameBuffer(VkRenderPass InRenderPass, VkExtent2D InExtent) const
{
	std::vector<VkImageView> Views{};
	return CreateFrameBuffer(Views, InRenderPass, InExtent);
}

VkFramebuffer UHGraphic::CreateFrameBuffer(VkImageView InImageView, VkRenderPass InRenderPass, VkExtent2D InExtent) const
{
	std::vector<VkImageView> Views{ InImageView };
	return CreateFrameBuffer(Views, InRenderPass, InExtent);
}

VkFramebuffer UHGraphic::CreateFrameBuffer(std::vector<VkImageView> InImageView, VkRenderPass InRenderPass, VkExtent2D InExtent) const
{
	VkFramebuffer NewFrameBuffer = VK_NULL_HANDLE;

	// create frame buffer
	VkFramebufferCreateInfo FramebufferInfo{};
	FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	FramebufferInfo.renderPass = InRenderPass;
	FramebufferInfo.attachmentCount = static_cast<uint32_t>(InImageView.size());
	FramebufferInfo.pAttachments = InImageView.data();
	FramebufferInfo.width = InExtent.width;
	FramebufferInfo.height = InExtent.height;
	FramebufferInfo.layers = 1;

	if (vkCreateFramebuffer(LogicalDevice, &FramebufferInfo, nullptr, &NewFrameBuffer) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create framebuffer!\n");
	}

	return NewFrameBuffer;
}

UHGPUQuery* UHGraphic::RequestGPUQuery(uint32_t Count, VkQueryType QueueType)
{
	std::unique_ptr<UHGPUQuery> NewQuery = std::make_unique<UHGPUQuery>();
	NewQuery->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);
	NewQuery->SetGfxCache(this);
	NewQuery->CreateQueryPool(Count, QueueType);

	QueryPools.push_back(std::move(NewQuery));
	return QueryPools.back().get();
}

// request render texture, this also sets device info to it
UHRenderTexture* UHGraphic::RequestRenderTexture(std::string InName, VkExtent2D InExtent, VkFormat InFormat, bool bIsLinear, bool bIsReadWrite, bool bIsShadowRT)
{
	return RequestRenderTexture(InName, VK_NULL_HANDLE, InExtent, InFormat, bIsLinear, bIsReadWrite, bIsShadowRT);
}

// request render texture, this also sets device info to it
UHRenderTexture* UHGraphic::RequestRenderTexture(std::string InName, VkImage InImage, VkExtent2D InExtent, VkFormat InFormat, bool bIsLinear, bool bIsReadWrite
	, bool bIsShadowRT)
{
	// return cached if there is already one
	UHRenderTexture Temp(InName, InExtent, InFormat, bIsLinear, bIsShadowRT);
	Temp.SetImage(InImage);

	int32_t Idx = UHUtilities::FindIndex<UHRenderTexture>(RTPools, Temp);
	if (Idx != -1)
	{
		return RTPools[Idx].get();
	}

	std::unique_ptr<UHRenderTexture> NewRT = std::make_unique<UHRenderTexture>(InName, InExtent, InFormat, bIsLinear, bIsReadWrite, bIsShadowRT);
	NewRT->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);
	NewRT->SetImage(InImage);

	if (NewRT->CreateRT())
	{
		RTPools.push_back(std::move(NewRT));
		return RTPools.back().get();
	}

	return nullptr;
}

// request release RT, this could be used during resizing
void UHGraphic::RequestReleaseRT(UHRenderTexture* InRT)
{
	int32_t Idx = UHUtilities::FindIndex<UHRenderTexture>(RTPools, *InRT);
	if (Idx == -1)
	{
		return;
	}

	RTPools[Idx]->Release();
	RTPools[Idx].reset();
	UHUtilities::RemoveByIndex(RTPools, Idx);
}

UHTexture2D* UHGraphic::RequestTexture2D(std::string InName, std::string InSourcePath, uint32_t Width, uint32_t Height, std::vector<uint8_t> InData
	, bool bIsLinear)
{
	// return cached if there is already one
	VkExtent2D Extent;
	Extent.width = Width;
	Extent.height = Height;

	VkFormat TexFormat = (bIsLinear) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
	UHTexture2D Temp(InName, InSourcePath, Extent, TexFormat, bIsLinear);

	int32_t Idx = UHUtilities::FindIndex<UHTexture2D>(Texture2DPools, Temp);
	if (Idx != -1)
	{
		return Texture2DPools[Idx].get();
	}

	std::unique_ptr<UHTexture2D> NewTex = std::make_unique<UHTexture2D>(InName, InSourcePath, Extent, TexFormat, bIsLinear);
	NewTex->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

	if (NewTex->CreateTextureFromMemory(Width, Height, InData, bIsLinear))
	{
		Texture2DPools.push_back(std::move(NewTex));
		return Texture2DPools.back().get();
	}

	return nullptr;
}

bool AreTextureSliceConsistent(std::string InArrayName, std::vector<UHTexture2D*> InTextures)
{
	if (InTextures.size() == 0)
	{
		return false;
	}

	bool bIsConsistent = true;
	for (size_t Idx = 0; Idx < InTextures.size(); Idx++)
	{
		for (size_t Jdx = 0; Jdx < InTextures.size(); Jdx++)
		{
			if (Idx != Jdx)
			{
				bool bIsFormatMatched = (InTextures[Idx]->GetFormat() == InTextures[Jdx]->GetFormat());
				bool bIsExtentMatched = (InTextures[Idx]->GetExtent().width == InTextures[Jdx]->GetExtent().width
					&& InTextures[Idx]->GetExtent().height == InTextures[Jdx]->GetExtent().height);

				if (!bIsFormatMatched)
				{
					UHE_LOG(L"Inconsistent texture slice format detected in array " + UHUtilities::ToStringW(InArrayName) + L"\n");
				}

				if (!bIsExtentMatched)
				{
					UHE_LOG(L"Inconsistent texture slice extent detected in array " + UHUtilities::ToStringW(InArrayName) + L"\n");
				}

				bIsConsistent &= bIsFormatMatched;
				bIsConsistent &= bIsExtentMatched;
			}
		}
	}

	return bIsConsistent;
}

UHTextureCube* UHGraphic::RequestTextureCube(std::string InName, std::vector<UHTexture2D*> InTextures)
{
	if (InTextures.size() != 6)
	{
		// for now the cube is consisted by texture slices, can't do it without any slices
		UHE_LOG(L"Number of texture slices is not 6!\n");
		return nullptr;
	}

	// consistent check between slices
	bool bConsistent = AreTextureSliceConsistent(InName, InTextures);
	if (!bConsistent)
	{
		return nullptr;
	}

	UHTextureCube Temp(InName, InTextures[0]->GetExtent(), InTextures[0]->GetFormat());
	int32_t Idx = UHUtilities::FindIndex<UHTextureCube>(TextureCubePools, Temp);
	if (Idx != -1)
	{
		return TextureCubePools[Idx].get();
	}

	std::unique_ptr<UHTextureCube> NewCube = std::make_unique<UHTextureCube>(InName, InTextures[0]->GetExtent(), InTextures[0]->GetFormat());
	NewCube->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

	if (NewCube->CreateCube(InTextures))
	{
		TextureCubePools.push_back(std::move(NewCube));
		return TextureCubePools.back().get();
	}

	return nullptr;
}

// request material without any import
// mostly used for engine materials
UHMaterial* UHGraphic::RequestMaterial()
{
	std::unique_ptr<UHMaterial> NewMat = std::make_unique<UHMaterial>();
	MaterialPools.push_back(std::move(NewMat));
	return MaterialPools.back().get();
}

UHMaterial* UHGraphic::RequestMaterial(std::filesystem::path InPath)
{
	// for now this function just allocate a new material
	// might have other usage in the future
	std::unique_ptr<UHMaterial> NewMat = std::make_unique<UHMaterial>();
	if (NewMat->Import(InPath))
	{
		MaterialPools.push_back(std::move(NewMat));
		return MaterialPools.back().get();
	}

	return nullptr;
}

std::unique_ptr<UHAccelerationStructure> UHGraphic::RequestAccelerationStructure()
{
	std::unique_ptr<UHAccelerationStructure> NewAS = std::make_unique<UHAccelerationStructure>();
	NewAS->SetGfxCache(this);
	NewAS->SetVulkanInstance(VulkanInstance);
	NewAS->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

	return std::move(NewAS);
}

UHShader* UHGraphic::RequestShader(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName
	, std::vector<std::string> InMacro)
{
	std::unique_ptr<UHShader> NewShader = std::make_unique<UHShader>(InShaderName, InSource, EntryName, ProfileName, InMacro);
	NewShader->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

	// early return if it's exist in pool
	int32_t PoolIdx = UHUtilities::FindIndex<UHShader>(ShaderPools, *NewShader.get());
	if (PoolIdx != -1)
	{
		return ShaderPools[PoolIdx].get();
	}

	// ensure the shader is compiled (debug only)
#if WITH_DEBUG
	AssetManagerInterface->CompileShader(InShaderName, InSource, EntryName, ProfileName, InMacro);
#endif

	// get macro hash name
	size_t MacroHash = UHUtilities::ShaderDefinesToHash(InMacro);
	std::string MacroHashName = (MacroHash != 0) ? "_" + std::to_string(MacroHash) : "";

	// find origin path and try to preserve file structure
	std::string OriginSubpath = UHAssetPath::GetShaderOriginSubpath(InSource);

	// setup input shader path, read from compiled shader
	std::filesystem::path OutputShaderPath = GShaderAssetFolder + OriginSubpath + InShaderName + MacroHashName + GShaderAssetExtension;
	if (!std::filesystem::exists(OutputShaderPath))
	{
		UHE_LOG(L"Failed to load shader " + OutputShaderPath.wstring() + L"!\n");
		return nullptr;
	}

	// load shader code
	std::ifstream FileIn(OutputShaderPath.string(), std::ios::ate | std::ios::binary);

	// get file size
	size_t FileSize = static_cast<size_t>(FileIn.tellg());
	std::vector<char> ShaderCode(FileSize);

	// read data
	FileIn.seekg(0);
	FileIn.read(ShaderCode.data(), FileSize);

	FileIn.close();

	// start to create shader module
	VkShaderModuleCreateInfo CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	CreateInfo.codeSize = ShaderCode.size();
	CreateInfo.pCode = reinterpret_cast<const uint32_t*>(ShaderCode.data());

	if (!NewShader->Create(CreateInfo))
	{
		return nullptr;
	}

	ShaderPools.push_back(std::move(NewShader));
	return ShaderPools.back().get();
}

// request a Graphic State object and return
UHGraphicState* UHGraphic::RequestGraphicState(UHRenderPassInfo InInfo)
{
	UHGraphicState Temp(InInfo);

	// check cached state first
	int32_t Idx = UHUtilities::FindIndex(StatePools, Temp);
	if (Idx != -1)
	{
		return StatePools[Idx].get();
	}

	std::unique_ptr<UHGraphicState> NewState = std::make_unique<UHGraphicState>();
	NewState->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);
	
	if (!NewState->CreateState(InInfo))
	{
		return nullptr;
	}

	StatePools.push_back(std::move(NewState));
	return StatePools.back().get();
}

UHGraphicState* UHGraphic::RequestRTState(UHRayTracingInfo InInfo)
{
	UHGraphicState Temp(InInfo);

	// check cached state first
	int32_t Idx = UHUtilities::FindIndex(StatePools, Temp);
	if (Idx != -1)
	{
		return StatePools[Idx].get();
	}

	std::unique_ptr<UHGraphicState> NewState = std::make_unique<UHGraphicState>();
	NewState->SetVulkanInstance(VulkanInstance);
	NewState->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

	if (!NewState->CreateState(InInfo))
	{
		return nullptr;
	}

	StatePools.push_back(std::move(NewState));
	return StatePools.back().get();
}

UHSampler* UHGraphic::RequestTextureSampler(UHSamplerInfo InInfo)
{
	UHSampler Temp(InInfo);

	int32_t Idx = UHUtilities::FindIndex(SamplerPools, Temp);
	if (Idx != -1)
	{
		return SamplerPools[Idx].get();
	}

	// create new one if cache fails
	std::unique_ptr<UHSampler> NewSampler = std::make_unique<UHSampler>(InInfo);
	NewSampler->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

	if (!NewSampler->Create())
	{
		return nullptr;
	}

	SamplerPools.push_back(std::move(NewSampler));
	return SamplerPools.back().get();
}

VkInstance UHGraphic::GetInstance() const
{
	return VulkanInstance;
}

VkDevice UHGraphic::GetLogicalDevice() const
{
	return LogicalDevice;
}

UHQueueFamily UHGraphic::GetQueueFamily() const
{
	return QueueFamily;
}

VkSwapchainKHR UHGraphic::GetSwapChain() const
{
	return SwapChain;
}

UHRenderTexture* UHGraphic::GetSwapChainRT(int32_t ImageIdx) const
{
	return SwapChainRT[ImageIdx];
}

VkFramebuffer UHGraphic::GetSwapChainBuffer(int32_t ImageIdx) const
{
	return SwapChainFrameBuffer[ImageIdx];
}

VkExtent2D UHGraphic::GetSwapChainExtent() const
{
	return SwapChainRT[0]->GetExtent();
}

VkFormat UHGraphic::GetSwapChainFormat() const
{
	return SwapChainRT[0]->GetFormat();
}

VkRenderPass UHGraphic::GetSwapChainRenderPass() const
{
	return SwapChainRenderPass;
}

VkQueue UHGraphic::GetGraphicQueue() const
{
	return GraphicsQueue;
}

VkQueue UHGraphic::GetComputeQueue() const
{
	return ComputesQueue;
}

VkPhysicalDeviceMemoryProperties UHGraphic::GetDeviceMemProps() const
{
	return PhysicalDeviceMemoryProperties;
}

uint32_t UHGraphic::GetShaderRecordSize() const
{
	return ShaderRecordSize;
}

float UHGraphic::GetGPUTimeStampPeriod() const
{
	return GPUTimeStampPeriod;
}

std::vector<UHSampler*> UHGraphic::GetSamplers() const
{
	std::vector<UHSampler*> Samplers(SamplerPools.size());
	for (size_t Idx = 0; Idx < Samplers.size(); Idx++)
	{
		Samplers[Idx] = SamplerPools[Idx].get();
	}

	return Samplers;
}

void UHGraphic::BeginCmdDebug(VkCommandBuffer InBuffer, std::string InName)
{
#if WITH_DEBUG
	if (ConfigInterface->RenderingSetting().bEnableGPULabeling)
	{
		VkDebugUtilsLabelEXT LabelInfo{};
		LabelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		LabelInfo.pLabelName = InName.c_str();

		BeginCmdDebugLabelCallback(InBuffer, &LabelInfo);
	}
#endif
}

void UHGraphic::EndCmdDebug(VkCommandBuffer InBuffer)
{
#if WITH_DEBUG
	if (ConfigInterface->RenderingSetting().bEnableGPULabeling)
	{
		EndCmdDebugLabelCallback(InBuffer);
	}
#endif
}

VkCommandBuffer UHGraphic::BeginOneTimeCmd()
{
	// allocate command pool
	VkCommandPoolCreateInfo PoolInfo{};
	PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

	// I'd like to reset and record every frame
	PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	PoolInfo.queueFamilyIndex = QueueFamily.GraphicsFamily.value();

	vkCreateCommandPool(LogicalDevice, &PoolInfo, nullptr, &CreationCommandPool);

	// allocate cmd and kick off it after creation
	VkCommandBufferAllocateInfo AllocInfo{};
	AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	AllocInfo.commandPool = CreationCommandPool;
	AllocInfo.commandBufferCount = 1;

	VkCommandBuffer CommandBuffer;
	vkAllocateCommandBuffers(LogicalDevice, &AllocInfo, &CommandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(CommandBuffer, &beginInfo);

	return CommandBuffer;
}

void UHGraphic::EndOneTimeCmd(VkCommandBuffer InBuffer)
{
	// end the one time cmd and submit it to GPU
	vkEndCommandBuffer(InBuffer);

	VkSubmitInfo SubmitInfo{};
	SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &InBuffer;

	vkQueueSubmit(GraphicsQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(GraphicsQueue);

	vkFreeCommandBuffers(LogicalDevice, CreationCommandPool, 1, &InBuffer);
	vkDestroyCommandPool(LogicalDevice, CreationCommandPool, nullptr);
	CreationCommandPool = VK_NULL_HANDLE;
}

bool UHGraphic::CreateSwapChain()
{
	// create swap chain officially!
	UHSwapChainDetails SwapChainSupport = QuerySwapChainSupport(PhysicalDevice);

	VkSurfaceFormatKHR Format = ChooseSwapChainFormat(SwapChainSupport);
	VkPresentModeKHR PresentMode = ChooseSwapChainMode(SwapChainSupport, ConfigInterface->PresentationSetting().bVsync);
	VkExtent2D Extent = ChooseSwapChainExtent(SwapChainSupport, WindowCache);

	// give one extra count, so we have a chance to advance one more frame if driver is busy processing the others
	uint32_t ImageCount = SwapChainSupport.Capabilities2.surfaceCapabilities.minImageCount + 1;
	if (SwapChainSupport.Capabilities2.surfaceCapabilities.maxImageCount > 0 && ImageCount > SwapChainSupport.Capabilities2.surfaceCapabilities.maxImageCount)
	{
		ImageCount = SwapChainSupport.Capabilities2.surfaceCapabilities.maxImageCount;
	}

	// create info for swap chain
	VkSwapchainCreateInfoKHR CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	CreateInfo.surface = MainSurface;
	CreateInfo.minImageCount = ImageCount;
	CreateInfo.imageFormat = Format.format;
	CreateInfo.imageColorSpace = Format.colorSpace;
	CreateInfo.imageExtent = Extent;

	// for VR app, this can be above 1
	CreateInfo.imageArrayLayers = 1;

	// use VK_IMAGE_USAGE_TRANSFER_DST_BIT if I'm rendering on another image and transfer to swapchain
	// use VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT if I want to render to swapchain directly
	// combine both usages so I can create imageview for swapchain but can also transfer to dst bit
	CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	// in this engine, graphic family is used as present family
	CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	CreateInfo.queueFamilyIndexCount = 0; // Optional
	CreateInfo.pQueueFamilyIndices = nullptr; // Optional

	CreateInfo.preTransform = SwapChainSupport.Capabilities2.surfaceCapabilities.currentTransform;
	CreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	CreateInfo.presentMode = PresentMode;
	CreateInfo.clipped = VK_TRUE;
	CreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// prepare win32 fullscreen ext
	VkSurfaceFullScreenExclusiveWin32InfoEXT Win32FullScreenInfo{};
	Win32FullScreenInfo.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT;
	Win32FullScreenInfo.hmonitor = MonitorFromWindow(WindowCache, MONITOR_DEFAULTTOPRIMARY);

	// prepare fullscreen stuff, set to VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT, so I can toggle fullscreen myself
	VkSurfaceFullScreenExclusiveInfoEXT FullScreenInfo{};
	FullScreenInfo.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;
	FullScreenInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_APPLICATION_CONTROLLED_EXT;
	FullScreenInfo.pNext = &Win32FullScreenInfo;
	CreateInfo.pNext = &FullScreenInfo;

	if (vkCreateSwapchainKHR(LogicalDevice, &CreateInfo, nullptr, &SwapChain) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create swap chain!\n");
		return false;
	}

	// store swap chain image
	vkGetSwapchainImagesKHR(LogicalDevice, SwapChain, &ImageCount, nullptr);

	std::vector<VkImage> SwapChainImages;
	SwapChainImages.resize(ImageCount);
	vkGetSwapchainImagesKHR(LogicalDevice, SwapChain, &ImageCount, SwapChainImages.data());

	// create render pass for swap chain, it will be blit from other source, so transfer to drc_bit first
	UHTransitionInfo SwapChainBlitTransition(VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	SwapChainRenderPass = CreateRenderPass(Format.format, SwapChainBlitTransition);

	// create swap chain RTs
	SwapChainRT.resize(ImageCount);
	SwapChainFrameBuffer.resize(ImageCount);
	for (size_t Idx = 0; Idx < ImageCount; Idx++)
	{
		SwapChainRT[Idx] = RequestRenderTexture("SwapChain" + std::to_string(Idx), SwapChainImages[Idx], Extent, Format.format, false);
		SwapChainFrameBuffer[Idx] = CreateFrameBuffer(SwapChainRT[Idx]->GetImageView(), SwapChainRenderPass, Extent);
	}

	return true;
}