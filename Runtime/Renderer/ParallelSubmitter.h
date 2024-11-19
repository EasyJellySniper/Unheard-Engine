#pragma once
#include "../Engine/Graphic.h"

struct UHParallelSubmitter
{
public:
	UHParallelSubmitter()
		: LogicalDevice(nullptr)
		, NumWT(0)
	{

	}

	void Initialize(UHGraphic* InGfx, UHQueueFamily QueueFamily, int32_t NumWorkerThreads, std::string InName)
	{
		LogicalDevice = InGfx->GetLogicalDevice();
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

#if WITH_EDITOR
			InGfx->SetDebugUtilsObjectName(VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)WorkerCommandPools[Idx]
				, InName + "_CommandPool" + std::to_string(Idx));
#endif

			// create worker command buffers
			AllocInfo.commandPool = WorkerCommandPools[Idx];
			AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
			AllocInfo.commandBufferCount = GMaxFrameInFlight;

			if (vkAllocateCommandBuffers(LogicalDevice, &AllocInfo, &WorkerCommandBuffers[Idx * (int32_t)GMaxFrameInFlight]) != VK_SUCCESS)
			{
				UHE_LOG(L"Failed to allocate worker command buffers!\n");
				return;
			}

#if WITH_EDITOR
			InGfx->SetDebugUtilsObjectName(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)WorkerCommandBuffers[Idx * (int32_t)GMaxFrameInFlight]
				, InName + "_CommandBuffer");
#endif
		}
	}

	void Release()
	{
		for (int32_t Idx = 0; Idx < NumWT; Idx++)
		{
			vkDestroyCommandPool(LogicalDevice, WorkerCommandPools[Idx], nullptr);
		}
	}

	void CollectCurrentFrameRTBundle(int32_t CurrentFrameRT)
	{
		for (int32_t Idx = 0; Idx < NumWT; Idx++)
		{
			WorkerBundles[Idx] = WorkerCommandBuffers[Idx * (int32_t)GMaxFrameInFlight + CurrentFrameRT];
		}
	}

	VkDevice LogicalDevice;
	int32_t NumWT;
	std::vector<VkCommandPool> WorkerCommandPools;
	std::vector<VkCommandBuffer> WorkerCommandBuffers;
	std::vector<VkCommandBuffer> WorkerBundles;
};