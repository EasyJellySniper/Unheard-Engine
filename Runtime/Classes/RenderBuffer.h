#pragma once
#include "../Engine/RenderResource.h"
#include "../Classes/Utility.h"
#include "../../UnheardEngine.h"
#include "GPUMemory.h"

// class for managing render buffer (E.g. vertex buffer/index buffer)
// make this template so we can decide the size dynamically
template<class T>
class UHRenderBuffer : public UHRenderResource
{
public:
    UHRenderBuffer() 
        : BufferSource(nullptr)
        , BufferMemory(nullptr)
        , ElementCount(0)
        , bIsUploadBuffer(false)
        , bIsShaderDeviceAddress(false)
        , DstData(nullptr)
        , BufferSize(0)
        , BufferStride(0)
        , OffsetInSharedMemory(~0)
    {

    }

	bool CreateBuffer(uint64_t InElementCount, VkBufferUsageFlags InUsage)
	{
        // skip creaetion if it's empty
        if (InElementCount == 0)
        {
            return false;
        }

        ElementCount = InElementCount;

        // set is upload buffer, and don't treat VB/IB buffer as upload buffer!
        bIsUploadBuffer = (InUsage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) || (InUsage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        if ((InUsage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) || (InUsage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
        {
            bIsUploadBuffer = false;
        }

        // for vkCmdFillBuffer()
        if (InUsage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
        {
            InUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        bIsShaderDeviceAddress = (InUsage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        BufferStride = sizeof(T);
        
        // padding stride to 256 bytes if it's constant buffer
        if (InUsage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
        {
            // AND ~255 strip the bits below 256
            // +255 ensure the value below 256 is padding to 256
            BufferStride = (BufferStride + 255) & ~255;
        }

        BufferSize = InElementCount * BufferStride;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = BufferSize;
        bufferInfo.usage = InUsage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(LogicalDevice, &bufferInfo, nullptr, &BufferSource) != VK_SUCCESS)
        {
            UHE_LOG(L"Failed to create buffer!\n");
        }

        VkMemoryRequirements MemRequirements;
        vkGetBufferMemoryRequirements(LogicalDevice, BufferSource, &MemRequirements);

        VkMemoryAllocateInfo AllocInfo{};
        AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        AllocInfo.allocationSize = MemRequirements.size;
        AllocInfo.memoryTypeIndex = UHUtilities::FindMemoryTypes(&DeviceMemoryProperties, MemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkMemoryAllocateFlagsInfo MemFlagInfo{};
        MemFlagInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        MemFlagInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

        if (bIsShaderDeviceAddress)
        {
            // put the VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT flag if buffer usage has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
            AllocInfo.pNext = &MemFlagInfo;
        }

        if (vkAllocateMemory(LogicalDevice, &AllocInfo, nullptr, &BufferMemory) != VK_SUCCESS)
        {
            UHE_LOG(L"Failed to allocate buffer memory!\n");
        }

        // committed resource to GPU
        if (vkBindBufferMemory(LogicalDevice, BufferSource, BufferMemory, 0) != VK_SUCCESS)
        {
            UHE_LOG(L"Failed to bind buffer to GPU!\n");
        }

        // if it's uploading buffer, Map here immediately and unmap it until destruction
        // more efficient than Map/Unmap every frame, app needs to handle sync however 
        if (bIsUploadBuffer)
        {
            if (vkMapMemory(LogicalDevice, BufferMemory, 0, BufferSize, 0, reinterpret_cast<void**>(&DstData)) != VK_SUCCESS)
            {
                UHE_LOG(L"Failed to map upload buffer to GPU!\n");
            }
        }

        return true;
	}

    bool CreateBuffer(uint64_t InElementCount, VkBufferUsageFlags InUsage, UHGPUMemory* SharedMemory)
    {
        // skip creaetion if it's empty
        if (InElementCount == 0 || SharedMemory == nullptr)
        {
            return false;
        }

        ElementCount = InElementCount;
        BufferStride = sizeof(T);
        BufferSize = InElementCount * BufferStride;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = BufferSize;
        bufferInfo.usage = InUsage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(LogicalDevice, &bufferInfo, nullptr, &BufferSource) != VK_SUCCESS)
        {
            UHE_LOG(L"Failed to create buffer!\n");
            return false;
        }

        VkMemoryRequirements MemRequirements;
        vkGetBufferMemoryRequirements(LogicalDevice, BufferSource, &MemRequirements);

        // bind to shared memory and keep the offset
        bool bExceedSharedMemory = false;
        OffsetInSharedMemory = SharedMemory->BindMemory(MemRequirements.size, MemRequirements.alignment, BufferSource);
        if (OffsetInSharedMemory == ~0)
        {
            bExceedSharedMemory = true;
            static bool bIsExceedingMsgPrinted = false;
            if (!bIsExceedingMsgPrinted)
            {
                UHE_LOG(L"Exceed shared image memory budget, will allocate individually instead.\n");
                bIsExceedingMsgPrinted = true;
            }
        }

        if (bExceedSharedMemory)
        {
            VkMemoryAllocateInfo AllocInfo{};
            AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            AllocInfo.allocationSize = MemRequirements.size;
            AllocInfo.memoryTypeIndex = UHUtilities::FindMemoryTypes(&DeviceMemoryProperties, MemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            VkMemoryAllocateFlagsInfo MemFlagInfo{};
            MemFlagInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            MemFlagInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

            if (bIsShaderDeviceAddress)
            {
                // put the VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT flag if buffer usage has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                AllocInfo.pNext = &MemFlagInfo;
            }

            if (vkAllocateMemory(LogicalDevice, &AllocInfo, nullptr, &BufferMemory) != VK_SUCCESS)
            {
                UHE_LOG(L"Failed to allocate buffer memory!\n");
            }

            // committed resource to GPU
            if (vkBindBufferMemory(LogicalDevice, BufferSource, BufferMemory, 0) != VK_SUCCESS)
            {
                UHE_LOG(L"Failed to bind buffer to GPU!\n");
            }
        }

        return true;
    }

    void Release()
    {
        // an upload buffer will be mapped when initialization, unmap it before destroy
        if (bIsUploadBuffer && BufferMemory)
        {
            vkUnmapMemory(LogicalDevice, BufferMemory);
        }

        if (BufferSource)
        {
            vkDestroyBuffer(LogicalDevice, BufferSource, nullptr);
        }

        if (BufferMemory)
        {
            vkFreeMemory(LogicalDevice, BufferMemory, nullptr);
        }

        BufferSource = nullptr;
        BufferMemory = nullptr;
    }

	// upload all data, this will copy whole buffer
	void UploadAllData(void* SrcData, size_t InCopySize = 0)
	{
        // upload buffer is mapped when initialization, simply copy it
        const int64_t CopySize = (InCopySize == 0) ? BufferSize : static_cast<int64_t>(InCopySize);
        if (bIsUploadBuffer)
        {
            // if it's constant buffer, it will be mapped on creation until destruction
            // simply copy to it without unmap
            // app must have cpu-gpu sync work flow and prevents accessing it at the same time
            memcpy_s(&DstData[0], BufferSize, SrcData, CopySize);
            return;
        }

        // for non-constant buffer, copy once and unmap it
		vkMapMemory(LogicalDevice, BufferMemory, 0, CopySize, 0, reinterpret_cast<void**>(&DstData));
		memcpy_s(&DstData[0], BufferSize, SrcData, CopySize);
		vkUnmapMemory(LogicalDevice, BufferMemory);
	}

    // upload all data, but it's copying to shared memory
    void UploadAllDataShared(void* SrcData, UHGPUMemory* InMemory)
    {
        if (OffsetInSharedMemory == ~0)
        {
            return;
        }

        vkMapMemory(LogicalDevice, InMemory->GetMemory(), OffsetInSharedMemory, BufferSize, 0, reinterpret_cast<void**>(&DstData));
        memcpy_s(&DstData[0], BufferSize, SrcData, BufferSize);
        vkUnmapMemory(LogicalDevice, InMemory->GetMemory());
    }

    // upload data with individual offset, similar to upload all but copy a buffer stride instead
    void UploadData(void* SrcData, int64_t Offset, size_t InCopySize = 0)
    {
        // upload buffer is mapped when initialization, simply copy it
        const int64_t CopySize = (InCopySize == 0) ? BufferStride : static_cast<int64_t>(InCopySize);
        if (bIsUploadBuffer)
        {
            memcpy_s(&DstData[Offset * BufferStride], BufferSize, SrcData, CopySize);
            return;
        }

        // for non-upload buffer, map from start offset and copy 
        vkMapMemory(LogicalDevice, BufferMemory, Offset * BufferStride, CopySize, 0, reinterpret_cast<void**>(&DstData));
        memcpy_s(&DstData[0], BufferSize, SrcData, CopySize);
        vkUnmapMemory(LogicalDevice, BufferMemory);
    }

    VkBuffer GetBuffer() const
    {
        return BufferSource;
    }

    int64_t GetBufferSize() const
    {
        return BufferSize;
    }

    int64_t GetBufferStride() const
    {
        return BufferStride;
    }

    uint64_t GetElementCount() const
    {
        return ElementCount;
    }

    std::vector<T> ReadbackData()
    {
        // map & unmap memory operation
        std::vector<T> OutputData(BufferSize / BufferStride);
        if (bIsUploadBuffer)
        {
            // already mapped, copy directly
            memcpy_s(OutputData.data(), BufferSize, &DstData[0], BufferSize);
            return OutputData;
        }

        vkMapMemory(LogicalDevice, BufferMemory, 0, BufferSize, 0, reinterpret_cast<void**>(&DstData));
        memcpy_s(OutputData.data(), BufferSize, &DstData[0], BufferSize);
        vkUnmapMemory(LogicalDevice, BufferMemory);

        return OutputData;
    }

private:
	VkBuffer BufferSource;
	VkDeviceMemory BufferMemory;
	uint64_t ElementCount;
    int64_t BufferSize;
    int64_t BufferStride;

    bool bIsUploadBuffer;
    bool bIsShaderDeviceAddress;
    BYTE* DstData;

    // for shared memory
    uint64_t OffsetInSharedMemory;
};