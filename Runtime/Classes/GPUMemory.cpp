#include "GPUMemory.h"
#include "Utility.h"
#include "../../UnheardEngine.h"
#include "Runtime/Engine/Graphic.h"

UHGPUMemory::UHGPUMemory()
	: MemoryBudgetByte(0)
	, BufferMemory(nullptr)
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
    const VkPhysicalDeviceMemoryProperties& DeviceMemoryProperties = GfxCache->GetDeviceMemProps();
    if (MemoryBudgetByte > DeviceMemoryProperties.memoryHeaps[MemTypeIndex].size && DeviceMemoryProperties.memoryHeaps[MemTypeIndex].size != 0)
    {
        MemoryBudgetByte = DeviceMemoryProperties.memoryHeaps[MemTypeIndex].size;
    }

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

void UHGPUMemory::Reset()
{
    CurrentOffset = 0;
}

uint64_t UHGPUMemory::BindMemory(uint64_t InSize, uint64_t InAlignment, VkBuffer InBuffer)
{
    // make the size is always the multiple of alignment
    if ((InSize % InAlignment) != 0)
    {
        const uint64_t Stride = InSize / InAlignment;
        InSize = InAlignment * (Stride + 1);
    }

    if (CurrentOffset + InSize > MemoryBudgetByte)
    {
        return ~0;
    }

    if (vkBindBufferMemory(LogicalDevice, InBuffer, BufferMemory, CurrentOffset) != VK_SUCCESS)
    {
        return ~0;
    }

    uint64_t StartOffset = CurrentOffset;
    CurrentOffset += InSize;

    // return start offset so the object knows where to manipulate
    return StartOffset;
}

uint64_t UHGPUMemory::BindMemory(uint64_t InSize, uint64_t InAlignment, VkImage InImage, uint64_t ReboundOffset)
{
    uint64_t StartOffset = (ReboundOffset != UINT64_MAX) ? ReboundOffset : CurrentOffset;

    // make the size is always the multiple of alignment
    if ((InSize % InAlignment) != 0)
    {
        const uint64_t Stride = InSize / InAlignment;
        InSize = InAlignment * (Stride + 1);
    }

    if (StartOffset + InSize > MemoryBudgetByte)
    {
        return ~0;
    }

    if (vkBindImageMemory(LogicalDevice, InImage, BufferMemory, StartOffset) != VK_SUCCESS)
    {
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