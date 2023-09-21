#include "../../framework.h"
#include "Graphic.h"
#include <sstream>	// file stream
#include <limits>	// std::numeric_limits
#include <algorithm> // for clamp
#include "../Classes/Utility.h"
#include "../Classes/AssetPath.h"

UHGraphic::UHGraphic(UHAssetManager* InAssetManager, UHConfigManager* InConfig)
	: GraphicsQueue(VK_NULL_HANDLE)
	, CreationCommandPool(VK_NULL_HANDLE)
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
	, bEnableDepthPrePass(InConfig->RenderingSetting().bEnableDepthPrePass)
	, bEnableRayTracing(InConfig->RenderingSetting().bEnableRayTracing)
	, bSupportHDR(false)
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
		, "VK_EXT_robustness2"
		, "VK_KHR_present_id"
		, "VK_KHR_present_wait" };

	RayTracingExtensions = { "VK_KHR_deferred_host_operations"
		, "VK_KHR_acceleration_structure"
		, "VK_KHR_ray_tracing_pipeline"
		, "VK_KHR_ray_query"
		, "VK_KHR_pipeline_library" };

	// push ray tracing extension
	if (bEnableRayTracing)
	{
		DeviceExtensions.insert(DeviceExtensions.end(), RayTracingExtensions.begin(), RayTracingExtensions.end());
	}
}

uint32_t GetMemoryTypeIndex(VkPhysicalDeviceMemoryProperties InProps, VkMemoryPropertyFlags InFlags)
{
	// note that this doesn't consider the resource bits
	for (uint32_t Idx = 0; Idx < InProps.memoryTypeCount; Idx++)
	{
		if ((InProps.memoryTypes[Idx].propertyFlags & InFlags) == InFlags)
		{
			return Idx;
		}
	}

	return ~0;
}

// init graphics
bool UHGraphic::InitGraphics(HWND Hwnd)
{
#if WITH_EDITOR
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
		ImageSharedMemory = MakeUnique<UHGPUMemory>();
		MeshBufferSharedMemory = MakeUnique<UHGPUMemory>();

		ImageSharedMemory->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);
		MeshBufferSharedMemory->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

		uint32_t ImageMemoryType = GetMemoryTypeIndex(PhysicalDeviceMemoryProperties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		uint32_t BufferMemoryType = GetMemoryTypeIndex(PhysicalDeviceMemoryProperties, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		ImageSharedMemory->AllocateMemory(static_cast<uint64_t>(ConfigInterface->EngineSetting().ImageMemoryBudgetMB) * 1048576, ImageMemoryType);
		MeshBufferSharedMemory->AllocateMemory(static_cast<uint64_t>(ConfigInterface->EngineSetting().MeshBufferMemoryBudgetMB) * 1048576, BufferMemoryType);
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
		GLeaveFullScreenCallback(LogicalDevice, SwapChain);
		bIsFullScreen = false;
	}

	WindowCache = nullptr;
	GraphicsQueue = VK_NULL_HANDLE;

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
	ImageSharedMemory->Release();
	ImageSharedMemory.reset();
	MeshBufferSharedMemory->Release();
	MeshBufferSharedMemory.reset();

	vkDestroyCommandPool(LogicalDevice, CreationCommandPool, nullptr);
	vkDestroySurfaceKHR(VulkanInstance, MainSurface, nullptr);
	vkDestroyDevice(LogicalDevice, nullptr);
	vkDestroyInstance(VulkanInstance, nullptr);
}

// debug only functions
#if WITH_EDITOR

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
#if WITH_EDITOR
	if (bUseValidationLayers && CheckValidationLayerSupport())
	{
		CreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
		CreateInfo.ppEnabledLayerNames = ValidationLayers.data();
	}
#endif

#if WITH_EDITOR
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
	GEnterFullScreenCallback = (PFN_vkAcquireFullScreenExclusiveModeEXT)vkGetInstanceProcAddr(VulkanInstance, "vkAcquireFullScreenExclusiveModeEXT");
	GLeaveFullScreenCallback = (PFN_vkReleaseFullScreenExclusiveModeEXT)vkGetInstanceProcAddr(VulkanInstance, "vkReleaseFullScreenExclusiveModeEXT");
	GGetSurfacePresentModes2Callback = (PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT)vkGetInstanceProcAddr(VulkanInstance, "vkGetPhysicalDeviceSurfacePresentModes2EXT");

#if WITH_EDITOR
	GBeginCmdDebugLabelCallback = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(VulkanInstance, "vkCmdBeginDebugUtilsLabelEXT");
	GEndCmdDebugLabelCallback = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(VulkanInstance, "vkCmdEndDebugUtilsLabelEXT");
#endif

	GVkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetInstanceProcAddr(VulkanInstance, "vkGetAccelerationStructureDeviceAddressKHR");
	GVkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetInstanceProcAddr(VulkanInstance, "vkGetAccelerationStructureBuildSizesKHR");
	GVkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetInstanceProcAddr(VulkanInstance, "vkCreateAccelerationStructureKHR");
	GVkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetInstanceProcAddr(VulkanInstance, "vkCmdBuildAccelerationStructuresKHR");
	GVkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetInstanceProcAddr(VulkanInstance, "vkDestroyAccelerationStructureKHR");
	GVkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetInstanceProcAddr(VulkanInstance, "vkCreateRayTracingPipelinesKHR");
	GVkWaitForPresentKHR = (PFN_vkWaitForPresentKHR)vkGetInstanceProcAddr(VulkanInstance, "vkWaitForPresentKHR");
	GVkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetInstanceProcAddr(VulkanInstance, "vkCmdTraceRaysKHR");
	GVkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetInstanceProcAddr(VulkanInstance, "vkGetRayTracingShaderGroupHandlesKHR");

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
				bEnableRayTracing = false;
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

	std::vector<VkDeviceQueueCreateInfo> QueueCreateInfo = { GraphicQueueCreateInfo, ComputeQueueCreateInfo };

	// define features, enable what I need in UH
	VkPhysicalDeviceFeatures DeviceFeatures{};
	DeviceFeatures.samplerAnisotropy = true;
	DeviceFeatures.fullDrawIndexUint32 = true;
	DeviceFeatures.textureCompressionBC = true;

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

	VkPhysicalDevicePresentIdFeaturesKHR PresentIdFeatureKHR{};
	PresentIdFeatureKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;
	PresentIdFeatureKHR.pNext = &RobustnessFeatures;

	VkPhysicalDevicePresentWaitFeaturesKHR PresentWaitFeatureKHR{};
	PresentWaitFeatureKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;
	PresentWaitFeatureKHR.pNext = &PresentIdFeatureKHR;

	// device feature needs to assign in fature 2
	VkPhysicalDeviceFeatures2 PhyFeatures{};
	PhyFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	PhyFeatures.features = DeviceFeatures;
	PhyFeatures.pNext = &PresentWaitFeatureKHR;

	vkGetPhysicalDeviceFeatures2(PhysicalDevice, &PhyFeatures);
	if (!RTFeatures.rayTracingPipeline)
	{
		UHE_LOG(L"Ray tracing pipeline not supported. System won't render ray tracing effects.\n");
		bEnableRayTracing = false;
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
	CreateInfo.pQueueCreateInfos = QueueCreateInfo.data();
	CreateInfo.queueCreateInfoCount = static_cast<uint32_t>(QueueCreateInfo.size());
	CreateInfo.pEnabledFeatures = nullptr;
	CreateInfo.pNext = &PhyFeatures;

	uint32_t ExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
	if (ExtensionCount > 0)
	{
		CreateInfo.enabledExtensionCount = static_cast<uint32_t>(DeviceExtensions.size());
		CreateInfo.ppEnabledExtensionNames = DeviceExtensions.data();
	}

	// set up validation layer if it's debugging
#if WITH_EDITOR
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
	GGetSurfacePresentModes2Callback(InDevice, &Surface2Info, &PresentModeCount, nullptr);

	if (PresentModeCount != 0)
	{
		Details.PresentModes.resize(PresentModeCount);
		GGetSurfacePresentModes2Callback(InDevice, &Surface2Info, &PresentModeCount, Details.PresentModes.data());
	}

	return Details;
}

VkSurfaceFormatKHR ChooseSwapChainFormat(const UHSwapChainDetails& Details, bool bEnableHDR, bool& bSupportHDR)
{
	std::optional<VkSurfaceFormatKHR> HDR10Format;
	VkSurfaceFormatKHR DesiredFormat{};

	// for now, choose non linear SRGB format
	// even use R10G10B10A2_UNORM, I need linear to gamma conversion, so just let it be converted by hardware
	for (const auto& AvailableFormat : Details.Formats2)
	{
		if (AvailableFormat.surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && AvailableFormat.surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			DesiredFormat = AvailableFormat.surfaceFormat;
		}
		else if (AvailableFormat.surfaceFormat.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && AvailableFormat.surfaceFormat.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
		{
			HDR10Format = AvailableFormat.surfaceFormat;
			bSupportHDR = true;
		}
	}

	// return hdr format if it's supported and enabled
	if (HDR10Format.has_value() && bEnableHDR)
	{
		return HDR10Format.value();
	}

	return DesiredFormat;
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
		if (AvailablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR && !bUseVsync)
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

// create render pass, depth only
VkRenderPass UHGraphic::CreateRenderPass(UHTransitionInfo InTransitionInfo, VkFormat InDepthFormat) const
{
	return CreateRenderPass(std::vector<VkFormat>(), InTransitionInfo, InDepthFormat);
}

// create render pass, multiple formats are possible
VkRenderPass UHGraphic::CreateRenderPass(std::vector<VkFormat> InFormat, UHTransitionInfo InTransitionInfo, VkFormat InDepthFormat) const
{
	VkRenderPass NewRenderPass = VK_NULL_HANDLE;
	uint32_t RTCount = static_cast<uint32_t>(InFormat.size());
	bool bHasDepthAttachment = (InDepthFormat != VK_FORMAT_UNDEFINED);

	std::vector<VkAttachmentDescription> ColorAttachments;
	std::vector<VkAttachmentReference> ColorAttachmentRefs;

	if (RTCount > 0)
	{
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
	}

	// subpass desc
	VkSubpassDescription Subpass{};
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.colorAttachmentCount = RTCount;
	Subpass.pColorAttachments = (RTCount > 0) ? ColorAttachmentRefs.data() : nullptr;

	// consider depth attachment
	VkAttachmentDescription DepthAttachment{};
	VkAttachmentReference DepthAttachmentRef{};
	if (bHasDepthAttachment)
	{
		DepthAttachment.format = InDepthFormat;
		DepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		DepthAttachment.loadOp = InTransitionInfo.DepthLoadOp;
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
	UniquePtr<UHGPUQuery> NewQuery = MakeUnique<UHGPUQuery>();
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
	UniquePtr<UHRenderTexture> NewRT = MakeUnique<UHRenderTexture>(InName, InExtent, InFormat, bIsLinear, bIsReadWrite, bIsShadowRT);
	NewRT->SetImage(InImage);

	int32_t Idx = UHUtilities::FindIndex<UHRenderTexture>(RTPools, *NewRT.get());
	if (Idx != UHINDEXNONE)
	{
		return RTPools[Idx].get();
	}

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

UHTexture2D* UHGraphic::RequestTexture2D(UHTexture2D& LoadedTex)
{
	// return cached if there is already one
	int32_t Idx = UHUtilities::FindIndex<UHTexture2D>(Texture2DPools, LoadedTex);
	if (Idx != UHINDEXNONE)
	{
		return Texture2DPools[Idx].get();
	}

	UniquePtr<UHTexture2D> NewTex = MakeUnique<UHTexture2D>(LoadedTex.GetName()
		, LoadedTex.GetSourcePath(), LoadedTex.GetExtent(), LoadedTex.GetFormat(), LoadedTex.GetTextureSettings());
	NewTex->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);
	NewTex->SetGfxCache(this);
	NewTex->SetTextureData(LoadedTex.GetTextureData());

	if (NewTex->CreateTextureFromMemory())
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

	UniquePtr<UHTextureCube> NewCube = MakeUnique<UHTextureCube>(InName, InTextures[0]->GetExtent(), InTextures[0]->GetFormat());
	int32_t Idx = UHUtilities::FindIndex<UHTextureCube>(TextureCubePools, *NewCube.get());
	if (Idx != UHINDEXNONE)
	{
		return TextureCubePools[Idx].get();
	}

	NewCube->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);
	NewCube->SetGfxCache(this);

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
	UniquePtr<UHMaterial> NewMat = MakeUnique<UHMaterial>();
	MaterialPools.push_back(std::move(NewMat));
	return MaterialPools.back().get();
}

UHMaterial* UHGraphic::RequestMaterial(std::filesystem::path InPath)
{
	// for now this function just allocate a new material
	// might have other usage in the future
	UniquePtr<UHMaterial> NewMat = MakeUnique<UHMaterial>();
	if (NewMat->Import(InPath))
	{
		NewMat->SetGfxCache(this);
		NewMat->PostImport();
		MaterialPools.push_back(std::move(NewMat));
		return MaterialPools.back().get();
	}

	return nullptr;
}

UniquePtr<UHAccelerationStructure> UHGraphic::RequestAccelerationStructure()
{
	UniquePtr<UHAccelerationStructure> NewAS = MakeUnique<UHAccelerationStructure>();
	NewAS->SetGfxCache(this);
	NewAS->SetVulkanInstance(VulkanInstance);
	NewAS->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

	return std::move(NewAS);
}

bool UHGraphic::CreateShaderModule(UniquePtr<UHShader>& NewShader, std::filesystem::path OutputShaderPath)
{
	// setup input shader path, read from compiled shader
	if (!std::filesystem::exists(OutputShaderPath))
	{
		UHE_LOG(L"Failed to load shader " + OutputShaderPath.wstring() + L"!\n");
		return false;
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
		return false;
	}

	return true;
}

uint32_t UHGraphic::RequestShader(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName
	, std::vector<std::string> InMacro)
{
	UniquePtr<UHShader> NewShader = MakeUnique<UHShader>(InShaderName, InSource, EntryName, ProfileName, InMacro);
	NewShader->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

	// early return if it's exist in pool
	int32_t PoolIdx = UHUtilities::FindIndex<UHShader>(ShaderPools, *NewShader.get());
	if (PoolIdx != UHINDEXNONE)
	{
		return ShaderPools[PoolIdx]->GetId();
	}

	// ensure the shader is compiled (debug only)
#if WITH_EDITOR
	AssetManagerInterface->CompileShader(InShaderName, InSource, EntryName, ProfileName, InMacro);
#endif

	// get macro hash name
	size_t MacroHash = UHUtilities::ShaderDefinesToHash(InMacro);
	std::string MacroHashName = (MacroHash != 0) ? "_" + std::to_string(MacroHash) : "";

	// find origin path and try to preserve file structure
	std::string OriginSubpath = UHAssetPath::GetShaderOriginSubpath(InSource);

	// setup input shader path, read from compiled shader
	std::filesystem::path OutputShaderPath = GShaderAssetFolder + OriginSubpath + InShaderName + MacroHashName + GShaderAssetExtension;
	if (!CreateShaderModule(NewShader, OutputShaderPath))
	{
		return -1;
	}

	ShaderPools.push_back(std::move(NewShader));
	return ShaderPools.back()->GetId();
}

// request shader for material
uint32_t UHGraphic::RequestMaterialShader(std::string InShaderName, std::filesystem::path InSource, std::string EntryName, std::string ProfileName
	, UHMaterialCompileData InData, std::vector<std::string> InMacro)
{
	// macro hash
	size_t MacroHash = UHUtilities::ShaderDefinesToHash(InMacro);
	std::string MacroHashName = (MacroHash != 0) ? "_" + std::to_string(MacroHash) : "";

	UHMaterial* InMat = InData.MaterialCache;
	std::string OutName = UHAssetPath::FormatMaterialShaderOutputPath("", InData.MaterialCache->GetName(), InShaderName, MacroHashName);
	std::filesystem::path OutputShaderPath = GShaderAssetFolder + OutName + GShaderAssetExtension;

	// if it's a release build, and there is no material shader for it, use a fallback one
#if WITH_RELEASE
	if (!std::filesystem::exists(OutputShaderPath))
	{
		InShaderName = "FallbackPixelShader";
		EntryName = "FallbackPS";
		InSource = GRawShaderPath + InShaderName;
	}
#endif

	UniquePtr<UHShader> NewShader = MakeUnique<UHShader>(OutName, InSource, EntryName, ProfileName, true, InMacro);
	NewShader->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

	// early return if it's exist in pool and does not need recompile
	int32_t PoolIdx = UHUtilities::FindIndex<UHShader>(ShaderPools, *NewShader.get());
	if (PoolIdx != UHINDEXNONE)
	{
		return ShaderPools[PoolIdx]->GetId();
	}

	// almost the same as common shader flow, but this will go through HLSL translator instead
	// only compile it when the compile flag or version is matched
#if WITH_EDITOR
	AssetManagerInterface->TranslateHLSL(InShaderName, InSource, EntryName, ProfileName, InData, InMacro, OutputShaderPath);
#endif

	if (!CreateShaderModule(NewShader, OutputShaderPath))
	{
		return -1;
	}

	ShaderPools.push_back(std::move(NewShader));
	return ShaderPools.back()->GetId();
}

void UHGraphic::RequestReleaseShader(uint32_t InShaderID)
{
	// check if the object still exists before release
	if (const UHShader* InShader = SafeGetObjectFromTable<const UHShader>(InShaderID))
	{
		int32_t Idx = UHUtilities::FindIndex(ShaderPools, *InShader);
		if (Idx != UHINDEXNONE)
		{
			ShaderPools[Idx]->Release();
			UHUtilities::RemoveByIndex(ShaderPools, Idx);
		}
	}
}

// request a Graphic State object and return
UHGraphicState* UHGraphic::RequestGraphicState(UHRenderPassInfo InInfo)
{
	UniquePtr<UHGraphicState> NewState = MakeUnique<UHGraphicState>(InInfo);

	// check cached state first
	int32_t Idx = UHUtilities::FindIndex(StatePools, *NewState.get());
	if (Idx != UHINDEXNONE)
	{
		return StatePools[Idx].get();
	}

	NewState->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);
	
	if (!NewState->CreateState(InInfo))
	{
		return nullptr;
	}

	StatePools.push_back(std::move(NewState));
	return StatePools.back().get();
}

void UHGraphic::RequestReleaseGraphicState(UHGraphicState* InState)
{
	if (InState == nullptr)
	{
		return;
	}

	int32_t Idx = UHUtilities::FindIndex(StatePools, *InState);
	if (Idx != UHINDEXNONE)
	{
		StatePools[Idx]->Release();
		UHUtilities::RemoveByIndex(StatePools, Idx);
	}
}

UHGraphicState* UHGraphic::RequestRTState(UHRayTracingInfo InInfo)
{
	UniquePtr<UHGraphicState> NewState = MakeUnique<UHGraphicState>(InInfo);

	// check cached state first
	int32_t Idx = UHUtilities::FindIndex(StatePools, *NewState.get());
	if (Idx != UHINDEXNONE)
	{
		return StatePools[Idx].get();
	}

	NewState->SetVulkanInstance(VulkanInstance);
	NewState->SetDeviceInfo(LogicalDevice, PhysicalDeviceMemoryProperties);

	if (!NewState->CreateState(InInfo))
	{
		return nullptr;
	}

	StatePools.push_back(std::move(NewState));
	return StatePools.back().get();
}

UHComputeState* UHGraphic::RequestComputeState(UHComputePassInfo InInfo)
{
	UniquePtr<UHComputeState> NewState = MakeUnique<UHComputeState>(InInfo);

	// check cached state first
	int32_t Idx = UHUtilities::FindIndex(StatePools, *NewState.get());
	if (Idx != UHINDEXNONE)
	{
		return StatePools[Idx].get();
	}

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
	UniquePtr<UHSampler> NewSampler = MakeUnique<UHSampler>(InInfo);

	int32_t Idx = UHUtilities::FindIndex(SamplerPools, *NewSampler.get());
	if (Idx != UHINDEXNONE)
	{
		return SamplerPools[Idx].get();
	}

	// create new one if cache fails
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

uint32_t UHGraphic::GetSwapChainCount() const
{
	return static_cast<uint32_t>(SwapChainRT.size());
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

bool UHGraphic::IsDepthPrePassEnabled() const
{
	return bEnableDepthPrePass;
}

bool UHGraphic::IsRayTracingEnabled() const
{
	return bEnableRayTracing;
}

bool UHGraphic::IsDebugLayerEnabled() const
{
	return bUseValidationLayers;
}

bool UHGraphic::IsHDRSupported() const
{
	return bSupportHDR;
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

UHGPUMemory* UHGraphic::GetMeshSharedMemory() const
{
	return MeshBufferSharedMemory.get();
}

UHGPUMemory* UHGraphic::GetImageSharedMemory() const
{
	return ImageSharedMemory.get();
}

void UHGraphic::BeginCmdDebug(VkCommandBuffer InBuffer, std::string InName)
{
#if WITH_EDITOR
	if (ConfigInterface->RenderingSetting().bEnableGPULabeling)
	{
		VkDebugUtilsLabelEXT LabelInfo{};
		LabelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		LabelInfo.pLabelName = InName.c_str();

		GBeginCmdDebugLabelCallback(InBuffer, &LabelInfo);
	}
#endif
}

void UHGraphic::EndCmdDebug(VkCommandBuffer InBuffer)
{
#if WITH_EDITOR
	if (ConfigInterface->RenderingSetting().bEnableGPULabeling)
	{
		GEndCmdDebugLabelCallback(InBuffer);
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

VkQueue UHGraphic::GetGraphicsQueue() const
{
	return GraphicsQueue;
}

bool UHGraphic::CreateSwapChain()
{
	// create swap chain officially!
	UHSwapChainDetails SwapChainSupport = QuerySwapChainSupport(PhysicalDevice);

	VkSurfaceFormatKHR Format = ChooseSwapChainFormat(SwapChainSupport, ConfigInterface->RenderingSetting().bEnableHDR, bSupportHDR);
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

	// prepare fullscreen stuff, set to VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT and let the driver do the work
	// at the beginning it was controlled by app, but it could cause initialization failed for 4070 Ti 
	VkSurfaceFullScreenExclusiveInfoEXT FullScreenInfo{};
	FullScreenInfo.sType = VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT;
	FullScreenInfo.fullScreenExclusive = VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT;
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
	UHTransitionInfo SwapChainBlitTransition(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	SwapChainRenderPass = CreateRenderPass(Format.format, SwapChainBlitTransition);

	// create swap chain RTs
	SwapChainRT.resize(ImageCount);
	SwapChainFrameBuffer.resize(ImageCount);
	for (size_t Idx = 0; Idx < ImageCount; Idx++)
	{
		SwapChainRT[Idx] = RequestRenderTexture("SwapChain" + std::to_string(Idx), SwapChainImages[Idx], Extent, Format.format, false);
		SwapChainFrameBuffer[Idx] = CreateFrameBuffer(SwapChainRT[Idx]->GetImageView(), SwapChainRenderPass, Extent);
	}

	// HDR metadata setting
	if (bSupportHDR)
	{
		VkHdrMetadataEXT HDRMetadata{};
		HDRMetadata.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;

		// follow the HDR10 metadata, Table 49. Color Spaces and Attributes from Vulkan specs
		HDRMetadata.displayPrimaryRed.x = 0.708f;
		HDRMetadata.displayPrimaryRed.y = 0.292f;
		HDRMetadata.displayPrimaryGreen.x = 0.170f;
		HDRMetadata.displayPrimaryGreen.y = 0.797f;
		HDRMetadata.displayPrimaryBlue.x = 0.131f;
		HDRMetadata.displayPrimaryBlue.y = 0.046f;
		HDRMetadata.whitePoint.x = 0.3127f;
		HDRMetadata.whitePoint.y = 0.3290f;

		// @TODO: expose MaxOutputNits, MinOutputNits, MaxCLL, MaxFALL for user input
		HDRMetadata.maxLuminance = 1000.0f;
		HDRMetadata.minLuminance = 0.001f;
		HDRMetadata.maxContentLightLevel = 2000.0f;
		HDRMetadata.maxFrameAverageLightLevel = 500.0f;

		GVkSetHdrMetadataEXT(LogicalDevice, ImageCount, &SwapChain, &HDRMetadata);
	}

	return true;
}