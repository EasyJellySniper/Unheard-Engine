#pragma once
#include "../Engine/Graphic.h"

struct UHParallelSubmitter
{
public:
	void Initialize(VkDevice InDevice, UHQueueFamily QueueFamily, int32_t NumWorkerThreads)
	{
		LogicalDevice = InDevice;
		NumWT = NumWorkerThreads;

		WorkerCommandPools.resize(NumWorkerThreads);
		WorkerCommandBuffers.resize(NumWorkerThreads * GMaxFrameInFlight);
		WorkerBundles.resize(NumWorkerThreads);

		VkCommandPoolCreateInfo PoolInfo{};
		PoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		PoolInfo.queueFamilyIndex = QueueFamily.GraphicsFamily.value();

		VkCommandBufferAllocateInfo AllocInfo{};
		AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

		for (int32_t Idx = 0; Idx < NumWorkerThreads; Idx++)
		{
			if (vkCreateCommandPool(LogicalDevice, &PoolInfo, nullptr, &WorkerCommandPools[Idx]) != VK_SUCCESS)
			{
				UHE_LOG(L"Failed to create worker command pool!\n");
				return;
			}

			// create worker command buffers
			AllocInfo.commandPool = WorkerCommandPools[Idx];
			AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
			AllocInfo.commandBufferCount = GMaxFrameInFlight;

			if (vkAllocateCommandBuffers(LogicalDevice, &AllocInfo, &WorkerCommandBuffers[Idx * (int32_t)GMaxFrameInFlight]) != VK_SUCCESS)
			{
				UHE_LOG(L"Failed to allocate worker command buffers!\n");
				return;
			}
		}
	}

	void Release()
	{
		for (int32_t Idx = 0; Idx < NumWT; Idx++)
		{
			vkDestroyCommandPool(LogicalDevice, WorkerCommandPools[Idx], nullptr);
		}
	}

	void CollectCurrentFrameBundle(int32_t CurrentFrame)
	{
		for (int32_t Idx = 0; Idx < NumWT; Idx++)
		{
			WorkerBundles[Idx] = WorkerCommandBuffers[Idx * (int32_t)GMaxFrameInFlight + CurrentFrame];
		}
	}

	VkDevice LogicalDevice;
	int32_t NumWT;
	std::vector<VkCommandPool> WorkerCommandPools;
	std::vector<VkCommandBuffer> WorkerCommandBuffers;
	std::vector<VkCommandBuffer> WorkerBundles;
};