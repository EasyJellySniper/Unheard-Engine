#pragma once
#include "../Engine/RenderResource.h"
#include "../Classes/Utility.h"
#include "../../UnheardEngine.h"

// class for managing render buffer (E.g. vertex buffer/index buffer)
// make this template so we can decide the size dynamically
template<class T>
class UHRenderBuffer : public UHRenderResource
{
public:
    UHRenderBuffer() 
        : BufferSource(VK_NULL_HANDLE)
        , BufferMemory(VK_NULL_HANDLE)
        , ElementCount(0)
        , bIsUploadBuffer(false)
        , bIsShaderDeviceAddress(false)
        , DstData(nullptr)
        , BufferSize(0)
        , BufferStride(0)
    {

    }

	void CreaetBuffer(uint64_t InElementCount, VkBufferUsageFlags InUsage)
	{
        // skip creaetion if it's empty
        if (InElementCount == 0)
        {
            return;
        }

        ElementCount = InElementCount;

        // set is upload buffer, and don't treat VB/IB buffer as upload buffer!
        bIsUploadBuffer = (InUsage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) || (InUsage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

        if ((InUsage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) || (InUsage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
        {
            bIsUploadBuffer = false;
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
	}

    void Release()
    {
        // an upload buffer will be mapped when initialization, unmap it before destroy
        if (bIsUploadBuffer)
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

        BufferSource = VK_NULL_HANDLE;
        BufferMemory = VK_NULL_HANDLE;
    }

	// upload all data, this will copy whole buffer
	void UploadAllData(void* SrcData)
	{
        // upload buffer is mapped when initialization, simply copy it
        if (bIsUploadBuffer)
        {
            // if it's constant buffer, it will be mapped on creation until destruction
            // simply copy to it without unmap
            // app must have cpu-gpu sync work flow and prevents accessing it at the same time
            memcpy(&DstData[0], SrcData, BufferSize);
            return;
        }

        // for non-constant buffer, copy once and unmap it
		vkMapMemory(LogicalDevice, BufferMemory, 0, BufferSize, 0, reinterpret_cast<void**>(&DstData));
		memcpy(&DstData[0], SrcData, BufferSize);
		vkUnmapMemory(LogicalDevice, BufferMemory);
	}

    // upload data with individual offset, similar to upload all but copy a buffer stride instead
    void UploadData(void* SrcData, int64_t Offset)
    {
        // upload buffer is mapped when initialization, simply copy it
        if (bIsUploadBuffer)
        {
            memcpy(&DstData[Offset * BufferStride], SrcData, BufferStride);
            return;
        }

        // for upload buffer, map from start offset and copy 
        vkMapMemory(LogicalDevice, BufferMemory, Offset * BufferStride, BufferStride, 0, reinterpret_cast<void**>(&DstData));
        memcpy(&DstData[0], SrcData, BufferStride);
        vkUnmapMemory(LogicalDevice, BufferMemory);
    }

    VkBuffer GetBuffer() const
    {
        return BufferSource;
    }

    VkDeviceMemory GetMemory() const
    {
        return BufferMemory;
    }

    int64_t GetBufferSize() const
    {
        return BufferSize;
    }

    int64_t GetBufferStride() const
    {
        return BufferStride;
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
};