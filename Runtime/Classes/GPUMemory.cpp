#include "GPUMemory.h"
#include "Utility.h"
#include "../../UnheardEngine.h"

UHGPUMemory::UHGPUMemory()
	: MemoryBudgetByte(0)
	, BufferMemory(VK_NULL_HANDLE)
    , CurrentOffset(0)
{

}

void UHGPUMemory::Release()
{
	vkFreeMemory(LogicalDevice, BufferMemory, nullptr);
}

void UHGPUMemory::AllocateMemory(uint64_t InBudget, uint32_t MemTypeIndex)
{
    MemoryBudgetByte = InBudget;

    VkMemoryAllocateInfo AllocInfo{};
    AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    AllocInfo.allocationSize = MemoryBudgetByte;
    AllocInfo.memoryTypeIndex = MemTypeIndex;

    VkMemoryAllocateFlagsInfo MemFlagInfo{};
    MemFlagInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    MemFlagInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    // allocate with device address anyway, this memory is going to be used with different source
    AllocInfo.pNext = &MemFlagInfo;

    if (vkAllocateMemory(LogicalDevice, &AllocInfo, nullptr, &BufferMemory) != VK_SUCCESS)
    {
        UHE_LOG(L"Failed to allocate buffer memory!\n");
    }
}

uint64_t UHGPUMemory::BindMemory(uint64_t InSize, VkBuffer InBuffer)
{
    if (CurrentOffset + InSize > MemoryBudgetByte)
    {
        UHE_LOG(L"Exceed max budget memory for buffers!\n");
        return ~0;
    }

    if (vkBindBufferMemory(LogicalDevice, InBuffer, BufferMemory, CurrentOffset) != VK_SUCCESS)
    {
        UHE_LOG(L"Failed to bind image to GPU!\n");
        return ~0;
    }

    uint64_t StartOffset = CurrentOffset;
    CurrentOffset += InSize;

    // return start offset so the object knows where to manipulate
    return StartOffset;
}

uint64_t UHGPUMemory::BindMemory(uint64_t InSize, VkImage InImage, uint64_t ReboundOffset)
{
    uint64_t StartOffset = (ReboundOffset != UINT64_MAX) ? ReboundOffset : CurrentOffset;

    if (StartOffset + InSize > MemoryBudgetByte)
    {
        UHE_LOG(L"Exceed max budget memory for images!\n");
        return ~0;
    }

    if (vkBindImageMemory(LogicalDevice, InImage, BufferMemory, StartOffset) != VK_SUCCESS)
    {
        UHE_LOG(L"Failed to bind image to GPU!\n");
        return ~0;
    }

    if (ReboundOffset == UINT64_MAX)
    {
        CurrentOffset += InSize;
    }

    // return start offset so the object knows where to manipulate
    return StartOffset;
}

VkDeviceMemory UHGPUMemory::GetMemory() const
{
    return BufferMemory;
}