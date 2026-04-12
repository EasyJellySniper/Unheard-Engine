#include "GraphicFunction.h"

// graphic functions, which contains several safe-destroy functions as vk functions will complain nullptr input
void SafeDeviceWaitIdle(VkDevice Device)
{
	if (Device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(Device);
	}
}

void SafeDestroyCommandPool(VkDevice Device, VkCommandPool& Pool)
{
	if (Device != VK_NULL_HANDLE && Pool != VK_NULL_HANDLE)
	{
		vkDestroyCommandPool(Device, Pool, nullptr);
		Pool = VK_NULL_HANDLE;
	}
}

void SafeDestroyPipeline(VkDevice Device, VkPipeline& Pipeline)
{
	if (Device != VK_NULL_HANDLE && Pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(Device, Pipeline, nullptr);
		Pipeline = VK_NULL_HANDLE;
	}
}

void SafeDestroyDescriptorPool(VkDevice Device, VkDescriptorPool& Pool)
{
	if (Device != VK_NULL_HANDLE && Pool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(Device, Pool, nullptr);
		Pool = VK_NULL_HANDLE;
	}
}

void SafeDestroySurfaceKHR(VkInstance Instance, VkSurfaceKHR& Surface)
{
	if (Instance != VK_NULL_HANDLE && Surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(Instance, Surface, nullptr);
		Surface = VK_NULL_HANDLE;
	}
}

void SafeDestroyDevice(VkDevice& Device)
{
	if (Device != VK_NULL_HANDLE)
	{
		vkDestroyDevice(Device, nullptr);
		Device = VK_NULL_HANDLE;
	}
}

void SafeDestroyInstance(VkInstance& Instance)
{
	if (Instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(Instance, nullptr);
		Instance = VK_NULL_HANDLE;
	}
}

void SafeDestroyFrameBuffer(VkDevice Device, VkFramebuffer& Framebuffer)
{
	if (Device != VK_NULL_HANDLE && Framebuffer != VK_NULL_HANDLE)
	{
		vkDestroyFramebuffer(Device, Framebuffer, nullptr);
		Framebuffer = VK_NULL_HANDLE;
	}
}

void SafeDestroyRenderPass(VkDevice Device, VkRenderPass& RenderPass)
{
	if (Device != VK_NULL_HANDLE && RenderPass != VK_NULL_HANDLE)
	{
		vkDestroyRenderPass(Device, RenderPass, nullptr);
		RenderPass = VK_NULL_HANDLE;
	}
}

void SafeDestroySwapchain(VkDevice Device, VkSwapchainKHR& Swapchain)
{
	if (Device != VK_NULL_HANDLE && Swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(Device, Swapchain, nullptr);
		Swapchain = VK_NULL_HANDLE;
	}
}

void SafeDestroyQueryPool(VkDevice Device, VkQueryPool& Pool)
{
	if (Device != VK_NULL_HANDLE && Pool != VK_NULL_HANDLE)
	{
		vkDestroyQueryPool(Device, Pool, nullptr);
		Pool = VK_NULL_HANDLE;
	}
}

void SafeDestroyBuffer(VkDevice Device, VkBuffer& Buffer)
{
	if (Device != VK_NULL_HANDLE && Buffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(Device, Buffer, nullptr);
		Buffer = VK_NULL_HANDLE;
	}
}

void SafeDestroySampler(VkDevice Device, VkSampler& Sampler)
{
	if (Device != VK_NULL_HANDLE && Sampler != VK_NULL_HANDLE)
	{
		vkDestroySampler(Device, Sampler, nullptr);
		Sampler = VK_NULL_HANDLE;
	}
}

void SafeDestroyShaderModule(VkDevice Device, VkShaderModule& ShaderModule)
{
	if (Device != VK_NULL_HANDLE && ShaderModule != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(Device, ShaderModule, nullptr);
		ShaderModule = VK_NULL_HANDLE;
	}
}

void SafeDestroyImage(VkDevice Device, VkImage& Image)
{
	if (Device != VK_NULL_HANDLE && Image != VK_NULL_HANDLE)
	{
		vkDestroyImage(Device, Image, nullptr);
		Image = VK_NULL_HANDLE;
	}
}

void SafeDestroyImageView(VkDevice Device, VkImageView& ImageView)
{
	if (Device != VK_NULL_HANDLE && ImageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(Device, ImageView, nullptr);
		ImageView = VK_NULL_HANDLE;
	}
}

void SafeDestroyFence(VkDevice Device, VkFence& Fence)
{
	if (Device != VK_NULL_HANDLE && Fence != VK_NULL_HANDLE)
	{
		vkDestroyFence(Device, Fence, nullptr);
		Fence = VK_NULL_HANDLE;
	}
}

void SafeDestroySemaphore(VkDevice Device, VkSemaphore& Semaphore)
{
	if (Device != VK_NULL_HANDLE && Semaphore != VK_NULL_HANDLE)
	{
		vkDestroySemaphore(Device, Semaphore, nullptr);
		Semaphore = VK_NULL_HANDLE;
	}
}

void SafeDestroyDescriptorSetLayout(VkDevice Device, VkDescriptorSetLayout& Layout)
{
	if (Device != VK_NULL_HANDLE && Layout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(Device, Layout, nullptr);
		Layout = VK_NULL_HANDLE;
	}
}

void SafeDestroyPipelineLayout(VkDevice Device, VkPipelineLayout& Layout)
{
	if (Device != VK_NULL_HANDLE && Layout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(Device, Layout, nullptr);
		Layout = VK_NULL_HANDLE;
	}
}

void SafeDestroyAccelerationStructure(VkDevice Device, VkAccelerationStructureKHR& AS)
{
	if (Device != VK_NULL_HANDLE && AS != VK_NULL_HANDLE)
	{
		GVkDestroyAccelerationStructureKHR(Device, AS, nullptr);
		AS = VK_NULL_HANDLE;
	}
}

void SafeFreeMemory(VkDevice Device, VkDeviceMemory& Memory)
{
	if (Device != VK_NULL_HANDLE && Memory != VK_NULL_HANDLE)
	{
		vkFreeMemory(Device, Memory, nullptr);
		Memory = VK_NULL_HANDLE;
	}
}