#pragma once
#include "../../UnheardEngine.h"

// a header that store graphic API function callback
#include "Runtime/Platform/PlatformVulkan.h"

// Vulkan callback functions, mainly for functions that are not a part of core Vulkan

#if WITH_EDITOR
inline PFN_vkCmdBeginDebugUtilsLabelEXT GBeginCmdDebugLabelCallback;
inline PFN_vkCmdEndDebugUtilsLabelEXT GEndCmdDebugLabelCallback;
inline PFN_vkSetDebugUtilsObjectNameEXT GSetDebugUtilsObjectNameEXT;
#endif

inline PFN_vkGetAccelerationStructureDeviceAddressKHR GVkGetAccelerationStructureDeviceAddressKHR;
inline PFN_vkGetAccelerationStructureBuildSizesKHR GVkGetAccelerationStructureBuildSizesKHR;
inline PFN_vkCreateAccelerationStructureKHR GVkCreateAccelerationStructureKHR;
inline PFN_vkCmdBuildAccelerationStructuresKHR GVkCmdBuildAccelerationStructuresKHR;
inline PFN_vkDestroyAccelerationStructureKHR GVkDestroyAccelerationStructureKHR;
inline PFN_vkCreateRayTracingPipelinesKHR GVkCreateRayTracingPipelinesKHR;
inline PFN_vkCmdTraceRaysKHR GVkCmdTraceRaysKHR;
inline PFN_vkGetRayTracingShaderGroupHandlesKHR GVkGetRayTracingShaderGroupHandlesKHR;
inline PFN_vkCmdPushDescriptorSetKHR GVkCmdPushDescriptorSetKHR;
inline PFN_vkCmdBeginConditionalRenderingEXT GVkCmdBeginConditionalRenderingEXT;
inline PFN_vkCmdEndConditionalRenderingEXT GVkCmdEndConditionalRenderingEXT;
inline PFN_vkCmdDrawMeshTasksEXT GVkCmdDrawMeshTasksEXT;

extern void SafeDeviceWaitIdle(VkDevice Device);
extern void SafeDestroyCommandPool(VkDevice Device, VkCommandPool& Pool);
extern void SafeDestroyPipeline(VkDevice Device, VkPipeline& Pipeline);
extern void SafeDestroyDescriptorPool(VkDevice Device, VkDescriptorPool& Pool);
extern void SafeDestroySurfaceKHR(VkInstance Instance, VkSurfaceKHR& Surface);
extern void SafeDestroyDevice(VkDevice& Device);
extern void SafeDestroyInstance(VkInstance& Instance);
extern void SafeDestroyFrameBuffer(VkDevice Device, VkFramebuffer& Framebuffer);
extern void SafeDestroyRenderPass(VkDevice Device, VkRenderPass& RenderPass);
extern void SafeDestroySwapchain(VkDevice Device, VkSwapchainKHR& Swapchain);
extern void SafeDestroyQueryPool(VkDevice Device, VkQueryPool& Pool);
extern void SafeDestroyBuffer(VkDevice Device, VkBuffer& Buffer);
extern void SafeDestroySampler(VkDevice Device, VkSampler& Sampler);
extern void SafeDestroyShaderModule(VkDevice Device, VkShaderModule& ShaderModule);
extern void SafeDestroyImage(VkDevice Device, VkImage& Image);
extern void SafeDestroyImageView(VkDevice Device, VkImageView& ImageView);
extern void SafeDestroyFence(VkDevice Device, VkFence& Fence);
extern void SafeDestroySemaphore(VkDevice Device, VkSemaphore& Semaphore);
extern void SafeDestroyDescriptorSetLayout(VkDevice Device, VkDescriptorSetLayout& Layout);
extern void SafeDestroyPipelineLayout(VkDevice Device, VkPipelineLayout& Layout);
extern void SafeDestroyAccelerationStructure(VkDevice Device, VkAccelerationStructureKHR& AS);
extern void SafeFreeMemory(VkDevice Device, VkDeviceMemory& Memory);