#include "Texture.h"
#include "../../UnheardEngine.h"
#include "../Classes/Utility.h"
#include "../CoreGlobals.h"
#include "../../Runtime/Engine/Graphic.h"

UHTexture::UHTexture()
	: UHTexture("", VkExtent2D(), UHTextureFormat::UH_FORMAT_RGBA8_SRGB, UHTextureSettings())
{

}

UHTexture::UHTexture(std::string InName, VkExtent2D InExtent, UHTextureFormat InFormat, UHTextureSettings InSettings)
	: ImageFormat(InFormat)
	, ImageExtent(InExtent)
	, ImageSource(nullptr)
	, ImageView(nullptr)
	, ImageMemory(nullptr)
	, ImageViewInfo(VkImageViewCreateInfo{})
	, bIsSourceCreatedByThis(false)
	, bHasUploadedToGPU(false)
	, bIsMipMapGenerated(false)
	, TextureSettings(InSettings)
	, MemoryOffset(~0)
	, MipMapCount(1)
	, TextureType(UHTextureType::Texture2D)
	, bCreatePerMipImageView(false)
{
#if WITH_EDITOR
	for (int32_t Idx = 0; Idx < CubemapImageViewCount; Idx++)
	{
		CubemapImageView[Idx] = nullptr;
	}
#endif
	Name = InName;
}

#if WITH_EDITOR
void UHTexture::SetTextureSettings(UHTextureSettings InSetting)
{
	TextureSettings = InSetting;
}

void UHTexture::SetRawSourcePath(std::string InPath)
{
	RawSourcePath = InPath;
}

void UHTexture::SetExtent(uint32_t Width, uint32_t Height)
{
	ImageExtent.width = Width;
	ImageExtent.height = Height;
}
#endif

void UHTexture::Release()
{
	vkFreeMemory(LogicalDevice, ImageMemory, nullptr);
	ImageMemory = nullptr;
	vkDestroyImageView(LogicalDevice, ImageView, nullptr);
	ImageView = nullptr;

	for (size_t Idx = 0; Idx < ImageViewPerMip.size(); Idx++)
	{
		vkDestroyImageView(LogicalDevice, ImageViewPerMip[Idx], nullptr);
	}
	ImageViewPerMip.clear();

#if WITH_EDITOR
	for (int32_t Idx = 0; Idx < CubemapImageViewCount; Idx++)
	{
		if (CubemapImageView[Idx] != nullptr)
		{
			vkDestroyImageView(LogicalDevice, CubemapImageView[Idx], nullptr);
			CubemapImageView[Idx] = nullptr;
		}
	}
#endif

	// only destroy source if it's created by this
	// image like swap chain can't be destroyed here
	if (bIsSourceCreatedByThis)
	{
		vkDestroyImage(LogicalDevice, ImageSource, nullptr);
		ImageSource = nullptr;
	}

	bHasUploadedToGPU = false;
	bIsMipMapGenerated = false;
}

void UHTexture::SetImage(VkImage InImage)
{
	ImageSource = InImage;
}

void UHTexture::SetImageLayout(VkImageLayout InLayout, uint32_t InMipIndex)
{
	ImageLayouts[InMipIndex] = InLayout;
}

bool UHTexture::Create(UHTextureInfo InInfo, UHGPUMemory* InSharedMemory)
{
	ImageFormat = InInfo.Format;
	ImageExtent = InInfo.Extent;
	MipMapCount = TextureSettings.bUseMipmap ? static_cast<uint32_t>(std::floor(std::log2((std::min)(ImageExtent.width, ImageExtent.height)))) + 1 : 1;
	TextureInfo = InInfo;
	ImageLayouts.resize(MipMapCount);

	// only create if the source is null, otherwise create image view only
	if (ImageSource == nullptr)
	{
		VkImageCreateInfo CreateInfo{};
		CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		CreateInfo.imageType = InInfo.Type;
		CreateInfo.format = GetVulkanFormat(ImageFormat);
		CreateInfo.extent = VkExtent3D{ ImageExtent.width, ImageExtent.height, static_cast<uint32_t>(1) };
		CreateInfo.mipLevels = MipMapCount;
		CreateInfo.arrayLayers = 1;
		CreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		CreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

		if (InInfo.ViewType == VK_IMAGE_VIEW_TYPE_CUBE)
		{
			CreateInfo.arrayLayers = 6;
			CreateInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		}

		// setup necessary bits
		CreateInfo.usage = InInfo.Usage;
		CreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		CreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		// adjust create info if this is a depth texture
		if (IsDepthFormat())
		{
			CreateInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		}
		else if (InInfo.bIsRT)
		{
			CreateInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		}

		if (vkCreateImage(LogicalDevice, &CreateInfo, nullptr, &ImageSource) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create image!\n");
			return false;
		}

#if WITH_EDITOR
		GfxCache->SetDebugUtilsObjectName(VK_OBJECT_TYPE_IMAGE, (uint64_t)ImageSource, Name + "_Image");
#endif

		// try to commit image to GPU memory, if it's in the editor mode, always request uncompressed memory size, so toggling between compression won't be an issue
		VkMemoryRequirements MemRequirements{};
		vkGetImageMemoryRequirements(LogicalDevice, ImageSource, &MemRequirements);

		if (!InInfo.bIsRT && GIsEditor)
		{
			VkDeviceImageMemoryRequirements ImageMemoryReqs{};
			ImageMemoryReqs.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;

			// always use uncompressed format in the editor when getting memory requirements
			// so switching between compressed/uncompressed won't be an issue
			CreateInfo.format = (TextureSettings.bIsLinear) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
			CreateInfo.format = (TextureSettings.bIsHDR) ? VK_FORMAT_R16G16B16A16_SFLOAT : CreateInfo.format;
			CreateInfo.mipLevels = MipMapCount;
			ImageMemoryReqs.pCreateInfo = &CreateInfo;

			VkMemoryRequirements2 MemoryReqs2{};
			MemoryReqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

			vkGetDeviceImageMemoryRequirements(LogicalDevice, &ImageMemoryReqs, &MemoryReqs2);
			MemRequirements = MemoryReqs2.memoryRequirements;
		}

		// bind to shared memory if available, otherwise, creating memory individually
		bool bExceedSharedMemory = false;
		if (InSharedMemory)
		{
			// cache the memory offset in shared memory, when it's re-creating, bind to the same offset
			MemoryOffset = InSharedMemory->BindMemory(MemRequirements.size, MemRequirements.alignment, ImageSource, InInfo.ReboundOffset);
			if (MemoryOffset == ~0)
			{
				bExceedSharedMemory = true;
				//UHE_LOG(L"Exceed shared image memory budget, will allocate individually instead.\n");
			}
		}
		
		if (!InSharedMemory || bExceedSharedMemory)
		{
			VkMemoryAllocateInfo AllocInfo{};
			AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			AllocInfo.allocationSize = MemRequirements.size;
			const std::vector<uint32_t>& MemTypeIndices = GfxCache->GetDeviceMemoryTypeIndices();
			const VkPhysicalDeviceMemoryProperties& DeviceMemoryProperties = GfxCache->GetDeviceMemProps();

			for (size_t Idx = 0; Idx < MemTypeIndices.size(); Idx++)
			{
				AllocInfo.memoryTypeIndex = MemTypeIndices[Idx];
				if (AllocInfo.allocationSize > DeviceMemoryProperties.memoryHeaps[AllocInfo.memoryTypeIndex].size)
				{
					// use the host memory if a single allocation is already bigger than whole heap
					AllocInfo.memoryTypeIndex = GetHostMemoryTypeIndex();
				}

				if (vkAllocateMemory(LogicalDevice, &AllocInfo, nullptr, &ImageMemory) == VK_SUCCESS)
				{
					break;
				}
			}

			// if failed, the device memory isn't available and need to use host memory
			if (ImageMemory == nullptr)
			{
				AllocInfo.memoryTypeIndex = GfxCache->GetHostMemoryTypeIndex();
				if (vkAllocateMemory(LogicalDevice, &AllocInfo, nullptr, &ImageMemory) != VK_SUCCESS)
				{
					// return if host memory failed too
					UHE_LOG(L"Failed to allocate image memory!\n");
					return false;
				}
			}

			if (vkBindImageMemory(LogicalDevice, ImageSource, ImageMemory, 0) != VK_SUCCESS)
			{
				UHE_LOG(L"Failed to bind image to GPU!\n");
				return false;
			}
		}

		bIsSourceCreatedByThis = true;
	}

	return CreateImageView(InInfo.ViewType);
}

bool UHTexture::IsDepthFormat() const
{
	return ImageFormat == UHTextureFormat::UH_FORMAT_D16
		|| ImageFormat == UHTextureFormat::UH_FORMAT_D24_S8
		|| ImageFormat == UHTextureFormat::UH_FORMAT_D32F
		|| ImageFormat == UHTextureFormat::UH_FORMAT_D32F_S8
		|| ImageFormat == UHTextureFormat::UH_FORMAT_X8_D24;
}

bool UHTexture::CreateImageView(VkImageViewType InViewType)
{
	// create image view
	VkImageViewCreateInfo CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	CreateInfo.image = ImageSource;
	CreateInfo.viewType = InViewType;
	CreateInfo.format = GetVulkanFormat(ImageFormat);
	CreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	CreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	CreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	CreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	// again, layer settings is for array sources
	CreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	CreateInfo.subresourceRange.baseMipLevel = 0;
	CreateInfo.subresourceRange.levelCount = MipMapCount;
	CreateInfo.subresourceRange.baseArrayLayer = 0;
	CreateInfo.subresourceRange.layerCount = 1;

	if (InViewType == VK_IMAGE_VIEW_TYPE_CUBE)
	{
		CreateInfo.subresourceRange.layerCount = 6;
	}

	if (IsDepthFormat())
	{
		CreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	if (vkCreateImageView(LogicalDevice, &CreateInfo, nullptr, &ImageView) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create image views!\n");
		return false;
	}
	ImageViewInfo = CreateInfo;

#if WITH_EDITOR
	GfxCache->SetDebugUtilsObjectName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)ImageView, Name + "_ImageView");
#endif

	// create individual mipmap view if requested
	if (bCreatePerMipImageView)
	{
		for (uint32_t Idx = 0; Idx < MipMapCount; Idx++)
		{
			VkImageView NewView;
			CreateInfo.subresourceRange.baseMipLevel = Idx;
			CreateInfo.subresourceRange.levelCount = 1;
			if (vkCreateImageView(LogicalDevice, &CreateInfo, nullptr, &NewView) != VK_SUCCESS)
			{
				UHE_LOG(L"Failed to create image views!\n");
				return false;
			}
			ImageViewPerMip.push_back(NewView);

#if WITH_EDITOR
			GfxCache->SetDebugUtilsObjectName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)NewView
				, Name + "_ImageViewMip" + std::to_string(Idx));
#endif
		}
	}

#if WITH_EDITOR
	if (InViewType == VK_IMAGE_VIEW_TYPE_CUBE)
	{
		CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		CreateInfo.subresourceRange.layerCount = 1;
		for (int32_t Idx = 0; Idx < 6; Idx++)
		{
			CreateInfo.subresourceRange.baseArrayLayer = Idx;
			for (uint32_t MipIdx = 0; MipIdx < MipMapCount; MipIdx++)
			{
				CreateInfo.subresourceRange.baseMipLevel = MipIdx;
				CreateInfo.subresourceRange.levelCount = 1;
				if (vkCreateImageView(LogicalDevice, &CreateInfo, nullptr, &CubemapImageView[Idx * 15 + MipIdx]) != VK_SUCCESS)
				{
					UHE_LOG(L"Failed to create cubemap image views!\n");
				}

#if WITH_EDITOR
				GfxCache->SetDebugUtilsObjectName(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)CubemapImageView[Idx * 15 + MipIdx]
					, Name + "_ImageViewCubeFace" + std::to_string(Idx) + "_Mip" + std::to_string(MipIdx));
#endif
			}
		}
	}
#endif

	return true;
}

std::string UHTexture::GetName() const
{
	return Name;
}

std::string UHTexture::GetSourcePath() const
{
	return SourcePath;
}

std::string UHTexture::GetRawSourcePath() const
{
	return RawSourcePath;
}

UHTextureFormat UHTexture::GetFormat() const
{
	return ImageFormat;
}

VkExtent2D UHTexture::GetExtent() const
{
	return ImageExtent;
}

VkImage UHTexture::GetImage() const
{
	return ImageSource;
}

VkImageLayout UHTexture::GetImageLayout(const uint32_t InMipIndex) const
{
	return ImageLayouts[InMipIndex];
}

VkImageView UHTexture::GetImageView() const
{
	return ImageView;
}

VkImageView UHTexture::GetImageView(int32_t MipIndex) const
{
	if (MipIndex == UHINDEXNONE)
	{
		return ImageView;
	}
	return ImageViewPerMip[MipIndex];
}

#if WITH_EDITOR
VkImageView UHTexture::GetCubemapImageView(const int32_t SliceIndex, const int32_t MipIndex) const
{
	return CubemapImageView[SliceIndex * 15 + MipIndex];
}
#endif

VkImageViewCreateInfo UHTexture::GetImageViewInfo() const
{
	return ImageViewInfo;
}

uint32_t UHTexture::GetMipMapCount() const
{
	return MipMapCount;
}

UHTextureSettings UHTexture::GetTextureSettings() const
{
	return TextureSettings;
}

void UHTexture::SetHasUploadedToGPU(bool bFlag)
{
	bHasUploadedToGPU = bFlag;
}

bool UHTexture::HasUploadedToGPU() const
{
	return bHasUploadedToGPU;
}

void UHTexture::SetMipmapGenerated(bool bFlag)
{
	bIsMipMapGenerated = bFlag;
}

bool UHTexture::operator==(const UHTexture& InTexture)
{
	return InTexture.Name == Name
		&& InTexture.ImageFormat == ImageFormat
		&& InTexture.ImageExtent.width == ImageExtent.width
		&& InTexture.ImageExtent.height == ImageExtent.height
		&& TextureSettings.bIsLinear == InTexture.TextureSettings.bIsLinear
		&& TextureSettings.bIsNormal == InTexture.TextureSettings.bIsNormal;
}