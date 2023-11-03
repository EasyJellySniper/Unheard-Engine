#include "DescriptorHelper.h"

UHDescriptorHelper::UHDescriptorHelper(VkDevice InDevice, VkDescriptorSet InSet)
	: LogicalDevice(InDevice)
	, DescriptorSetToWrite(InSet)
{

}

UHDescriptorHelper::~UHDescriptorHelper()
{
	LogicalDevice = nullptr;
	DescriptorSetToWrite = nullptr;
}

void UHDescriptorHelper::WriteImage(const UHTexture* InTexture, uint32_t InDstBinding, bool bIsReadWrite)
{
	VkDescriptorImageInfo NewInfo{};
	NewInfo.imageLayout = (bIsReadWrite) ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	NewInfo.imageView = InTexture->GetImageView();

	VkWriteDescriptorSet DescriptorWrite{};
	DescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	DescriptorWrite.dstSet = DescriptorSetToWrite;
	DescriptorWrite.descriptorType = (bIsReadWrite) ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	DescriptorWrite.dstBinding = InDstBinding;
	DescriptorWrite.descriptorCount = 1;
	DescriptorWrite.pImageInfo = &NewInfo;

	vkUpdateDescriptorSets(LogicalDevice, 1, &DescriptorWrite, 0, nullptr);
}

// write multiple image at once for descriptor array
void UHDescriptorHelper::WriteImage(const std::vector<UHTexture*>& InTextures, uint32_t InDstBinding)
{
	std::vector<VkDescriptorImageInfo> NewInfos(InTextures.size());
	for (size_t Idx = 0; Idx < InTextures.size(); Idx++)
	{
		// allow null image view
		VkDescriptorImageInfo NewInfo{};
		NewInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		NewInfo.imageView = (InTextures[Idx] != nullptr) ? InTextures[Idx]->GetImageView() : nullptr;
		NewInfos[Idx] = NewInfo;
	}

	VkWriteDescriptorSet DescriptorWrite{};
	DescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	DescriptorWrite.dstSet = DescriptorSetToWrite;
	DescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	DescriptorWrite.dstBinding = InDstBinding;
	DescriptorWrite.descriptorCount = static_cast<uint32_t>(NewInfos.size());
	DescriptorWrite.pImageInfo = NewInfos.data();

	vkUpdateDescriptorSets(LogicalDevice, 1, &DescriptorWrite, 0, nullptr);
}

void UHDescriptorHelper::WriteSampler(const UHSampler* InSampler, uint32_t InDstBinding)
{
	VkDescriptorImageInfo NewInfo{};
	NewInfo.sampler = InSampler->GetSampler();

	VkWriteDescriptorSet DescriptorWrite{};
	DescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	DescriptorWrite.dstSet = DescriptorSetToWrite;
	DescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	DescriptorWrite.dstBinding = InDstBinding;
	DescriptorWrite.descriptorCount = 1;
	DescriptorWrite.pImageInfo = &NewInfo;

	vkUpdateDescriptorSets(LogicalDevice, 1, &DescriptorWrite, 0, nullptr);
}

// multiple sampler write
void UHDescriptorHelper::WriteSampler(const std::vector<UHSampler*>& InSamplers, uint32_t InDstBinding)
{
	std::vector<VkDescriptorImageInfo> NewInfos(InSamplers.size());

	for (size_t Idx = 0; Idx < NewInfos.size(); Idx++)
	{
		VkDescriptorImageInfo NewInfo{};
		NewInfo.sampler = InSamplers[Idx] ? InSamplers[Idx]->GetSampler() : nullptr;
		NewInfos[Idx] = NewInfo;
	}

	VkWriteDescriptorSet DescriptorWrite{};
	DescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	DescriptorWrite.dstSet = DescriptorSetToWrite;
	DescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	DescriptorWrite.dstBinding = InDstBinding;
	DescriptorWrite.descriptorCount = static_cast<uint32_t>(NewInfos.size());
	DescriptorWrite.pImageInfo = NewInfos.data();

	vkUpdateDescriptorSets(LogicalDevice, 1, &DescriptorWrite, 0, nullptr);
}

void UHDescriptorHelper::WriteTLAS(const UHAccelerationStructure* InAS, uint32_t InDstBinding)
{
	VkWriteDescriptorSet DescriptorWrite{};
	DescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	DescriptorWrite.dstSet = DescriptorSetToWrite;
	DescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	DescriptorWrite.dstBinding = InDstBinding;
	DescriptorWrite.descriptorCount = 1;

	// setup AS info
	VkWriteDescriptorSetAccelerationStructureKHR DesciptorWriterAS{};
	DesciptorWriterAS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	DesciptorWriterAS.accelerationStructureCount = 1;

	VkAccelerationStructureKHR AS[] = { InAS->GetAS() };
	DesciptorWriterAS.pAccelerationStructures = AS;
	DescriptorWrite.pNext = &DesciptorWriterAS;

	vkUpdateDescriptorSets(LogicalDevice, 1, &DescriptorWrite, 0, nullptr);
}