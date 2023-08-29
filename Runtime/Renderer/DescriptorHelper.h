#pragma once
#include <vulkan/vulkan_core.h>
#include <vector>
#include "../Classes/RenderBuffer.h"
#include "../Classes/Texture.h"
#include "../Classes/Sampler.h"
#include "../Classes/AccelerationStructure.h"

class UHDescriptorHelper
{
public:
	UHDescriptorHelper(VkDevice InDevice, VkDescriptorSet InSet);
	~UHDescriptorHelper();

	// write constant buffer once
	template<typename T>
	void WriteConstantBuffer(const UHRenderBuffer<T>* InBuffer, uint32_t InDstBinding, uint64_t InOffset = 0)
	{
		VkDescriptorBufferInfo NewInfo{};
		NewInfo.buffer = InBuffer->GetBuffer();
		NewInfo.range = InBuffer->GetBufferStride();
		NewInfo.offset = InOffset * InBuffer->GetBufferStride();
		
		VkWriteDescriptorSet DescriptorWrite{};
		DescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrite.dstSet = DescriptorSetToWrite;
		DescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		DescriptorWrite.dstBinding = InDstBinding;
		DescriptorWrite.descriptorCount = 1;
		DescriptorWrite.pBufferInfo = &NewInfo;

		vkUpdateDescriptorSets(LogicalDevice, 1, &DescriptorWrite, 0, nullptr);
	}

	// write storage buffer once
	template<typename T>
	void WriteStorageBuffer(const UHRenderBuffer<T>* InBuffer, uint32_t InDstBinding, uint64_t InOffset = 0, bool bFullRange = false)
	{
		VkDescriptorBufferInfo NewInfo{};
		NewInfo.buffer = InBuffer->GetBuffer();

		// Full range binding if I want to index into structuredbuffer instead of using first element only
		NewInfo.range = (bFullRange) ? InBuffer->GetBufferSize() : InBuffer->GetBufferStride();
		NewInfo.offset = InOffset * InBuffer->GetBufferStride();

		if (NewInfo.buffer == VK_NULL_HANDLE)
		{
			NewInfo.offset = 0;
			NewInfo.range = VK_WHOLE_SIZE;
		}

		VkWriteDescriptorSet DescriptorWrite{};
		DescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrite.dstSet = DescriptorSetToWrite;
		DescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		DescriptorWrite.dstBinding = InDstBinding;
		DescriptorWrite.descriptorCount = 1;
		DescriptorWrite.pBufferInfo = &NewInfo;

		vkUpdateDescriptorSets(LogicalDevice, 1, &DescriptorWrite, 0, nullptr);
	}

	// write multiple storage buffer, this will always bind full range and 0 offset for individual buffer
	template<typename T>
	void WriteStorageBuffer(const std::vector<UHRenderBuffer<T>*>& InBuffers, uint32_t InDstBinding)
	{
		std::vector<VkDescriptorBufferInfo> NewInfos(InBuffers.size());

		for (size_t Idx = 0; Idx < NewInfos.size(); Idx++)
		{
			VkDescriptorBufferInfo NewInfo{};

			// allow null buffer
			if (InBuffers[Idx] != nullptr)
			{
				NewInfo.buffer = InBuffers[Idx]->GetBuffer();
				NewInfo.range = InBuffers[Idx]->GetBufferSize();
			}
			else
			{
				NewInfo.range = VK_WHOLE_SIZE;
			}

			NewInfo.offset = 0;
			NewInfos[Idx] = NewInfo;
		}

		VkWriteDescriptorSet DescriptorWrite{};
		DescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrite.dstSet = DescriptorSetToWrite;
		DescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		DescriptorWrite.dstBinding = InDstBinding;
		DescriptorWrite.descriptorCount = static_cast<uint32_t>(NewInfos.size());
		DescriptorWrite.pBufferInfo = NewInfos.data();

		vkUpdateDescriptorSets(LogicalDevice, 1, &DescriptorWrite, 0, nullptr);
	}

	// write multiple storage buffer, but this uses VkDescriptorBufferInfo as input directly
	void WriteStorageBuffer(const std::vector<VkDescriptorBufferInfo>& InBufferInfos, uint32_t InDstBinding)
	{
		VkWriteDescriptorSet DescriptorWrite{};
		DescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		DescriptorWrite.dstSet = DescriptorSetToWrite;
		DescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		DescriptorWrite.dstBinding = InDstBinding;
		DescriptorWrite.descriptorCount = static_cast<uint32_t>(InBufferInfos.size());
		DescriptorWrite.pBufferInfo = InBufferInfos.data();

		vkUpdateDescriptorSets(LogicalDevice, 1, &DescriptorWrite, 0, nullptr);
	}

	// write image/sampler once
	void WriteImage(const UHTexture* InTexture, uint32_t InDstBinding, bool bIsReadWrite = false);
	void WriteImage(const std::vector<UHTexture*>& InTextures, uint32_t InDstBinding);
	void WriteSampler(const UHSampler* InSampler, uint32_t InDstBinding);
	void WriteSampler(const std::vector<UHSampler*>& InSamplers, uint32_t InDstBinding);
	void WriteTLAS(const UHAccelerationStructure* InAS, uint32_t InDstBinding);

private:
	VkDevice LogicalDevice;
	VkDescriptorSet DescriptorSetToWrite;
};