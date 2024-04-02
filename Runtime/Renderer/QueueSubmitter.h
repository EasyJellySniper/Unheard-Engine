#pragma once
#include "../Engine/Graphic.h"

// the queue submission stuffs
struct UHQueueSubmitter
{
public:
	UHQueueSubmitter()
		: CommandPool(nullptr)
		, LogicalDevice(nullptr)
		, Queue(nullptr)
	{
		for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			CommandBuffers[Idx] = nullptr;
			WaitingSemaphores[Idx] = nullptr;
			FinishedSemaphores[Idx] = nullptr;
			Fences[Idx] = nullptr;
		}
	}

	bool Initialize(VkDevice InDevice, const uint32_t QueueFamilyIndex, const uint32_t QueueIndex)
	{
		LogicalDevice = InDevice;
		VkDevice LogicalDevice = InDevice;

		VkCommandPoolCreateInfo PoolInfo{};
		PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

		// I'd like to reset and record every frame
		PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		PoolInfo.queueFamilyIndex = QueueFamilyIndex;

		if (vkCreateCommandPool(LogicalDevice, &PoolInfo, nullptr, &CommandPool) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create command pool!\n");
			return false;
		}

		// now create command buffer
		VkCommandBufferAllocateInfo AllocInfo{};
		AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		AllocInfo.commandPool = CommandPool;
		AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		AllocInfo.commandBufferCount = GMaxFrameInFlight;

		if (vkAllocateCommandBuffers(LogicalDevice, &AllocInfo, CommandBuffers.data()) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to allocate command buffers!\n");
			return false;
		}

		// init semphore
		VkSemaphoreCreateInfo SemaphoreInfo{};
		SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo FenceInfo{};
		FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

		// so fence won't be blocked on the first frame
		FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			if (vkCreateSemaphore(LogicalDevice, &SemaphoreInfo, nullptr, &WaitingSemaphores[Idx]) != VK_SUCCESS ||
				vkCreateSemaphore(LogicalDevice, &SemaphoreInfo, nullptr, &FinishedSemaphores[Idx]) != VK_SUCCESS ||
				vkCreateFence(LogicalDevice, &FenceInfo, nullptr, &Fences[Idx]) != VK_SUCCESS)
			{
				UHE_LOG(L"Failed to allocate fences!\n");
				return false;
			}
		}

		// at last, get the queue
		vkGetDeviceQueue(LogicalDevice, QueueFamilyIndex, QueueIndex, &Queue);

		return true;
	}

	void Release()
	{
		for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			vkDestroySemaphore(LogicalDevice, WaitingSemaphores[Idx], nullptr);
			vkDestroySemaphore(LogicalDevice, FinishedSemaphores[Idx], nullptr);
			vkDestroyFence(LogicalDevice, Fences[Idx], nullptr);
		}

		vkDestroyCommandPool(LogicalDevice, CommandPool, nullptr);
	}

	// similar to D3D12 command list allocator
	VkCommandPool CommandPool;

	// similar to D3D12 command list
	std::array<VkCommandBuffer, GMaxFrameInFlight> CommandBuffers;

	// similar to D3D12 Fence (GPU Fence)
	std::array<VkSemaphore, GMaxFrameInFlight> WaitingSemaphores;
	std::array<VkSemaphore, GMaxFrameInFlight> FinishedSemaphores;

	// similar to D3D12 Fence
	std::array<VkFence, GMaxFrameInFlight> Fences;

	VkDevice LogicalDevice;
	VkQueue Queue;
};
