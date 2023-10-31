#include "Texture.h"
#include "../../UnheardEngine.h"
#include "../Classes/Utility.h"
#include "../CoreGlobals.h"

UHTexture::UHTexture()
	: UHTexture("", VkExtent2D(), UH_FORMAT_RGBA8_SRGB, UHTextureSettings())
{

}

UHTexture::UHTexture(std::string InName, VkExtent2D InExtent, UHTextureFormat InFormat, UHTextureSettings InSettings)
	: Name(InName)
	, ImageFormat(InFormat)
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
	, TextureVersion(InitialTexture)
	, MipMapCount(1)
	, TextureType(Texture2D)
{
#if WITH_EDITOR
	for (int32_t Idx = 0; Idx < 6; Idx++)
	{
		CubemapImageView[Idx] = nullptr;
	}
#endif
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

#if WITH_EDITOR
	for (int32_t Idx = 0; Idx < 6; Idx++)
	{
		vkDestroyImageView(LogicalDevice, CubemapImageView[Idx], nullptr);
		CubemapImageView[Idx] = nullptr;
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

bool UHTexture::Create(UHTextureInfo InInfo, UHGPUMemory* InSharedMemory)
{
	ImageFormat = InInfo.Format;
	ImageExtent = InInfo.Extent;
	MipMapCount = InInfo.bUseMipMap ? static_cast<uint32_t>(std::floor(std::log2((std::min)(ImageExtent.width, ImageExtent.height)))) + 1
		: 1;
	TextureInfo = InInfo;

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

		// try to commit image to GPU memory, if it's in the editor mode, always request uncompressed memory size, so toggling between compression won't be an issue
		VkMemoryRequirements MemRequirements{};
		vkGetImageMemoryRequirements(LogicalDevice, ImageSource, &MemRequirements);

#if WITH_EDITOR
		if (!InInfo.bIsRT)
		{
			VkDeviceImageMemoryRequirements ImageMemoryReqs{};
			ImageMemoryReqs.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;

			// always use uncompressed format in the editor when getting memory requirements
			// so switching between compressed/uncompressed won't be an issue
			CreateInfo.format = (TextureSettings.bIsLinear) ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
			CreateInfo.format = (TextureSettings.bIsHDR) ? VK_FORMAT_R16G16B16A16_SFLOAT : CreateInfo.format;
			ImageMemoryReqs.pCreateInfo = &CreateInfo;

			VkMemoryRequirements2 MemoryReqs2{};
			MemoryReqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;

			vkGetDeviceImageMemoryRequirements(LogicalDevice, &ImageMemoryReqs, &MemoryReqs2);
			MemRequirements = MemoryReqs2.memoryRequirements;
		}
#endif
		// bind to shared memory if available, otherwise, creating memory individually
		bool bExceedSharedMemory = false;
		if (InSharedMemory)
		{
			// cache the memory offset in shared memory, when it's re-creating, bind to the same offset
			MemoryOffset = InSharedMemory->BindMemory(MemRequirements.size, MemRequirements.alignment, ImageSource, InInfo.ReboundOffset);
			if (MemoryOffset == ~0)
			{
				bExceedSharedMemory = true;
				static bool bIsExceedingMsgPrinted = false;
				if (!bIsExceedingMsgPrinted)
				{
					UHE_LOG(L"Exceed shared image memory budget, will allocate individually instead.\n");
					bIsExceedingMsgPrinted = true;
				}
			}
		}
		
		if (!InSharedMemory || bExceedSharedMemory)
		{
			VkMemoryAllocateInfo AllocInfo{};
			AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			AllocInfo.allocationSize = MemRequirements.size;
			AllocInfo.memoryTypeIndex = UHUtilities::FindMemoryTypes(&DeviceMemoryProperties, MemRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			if (vkAllocateMemory(LogicalDevice, &AllocInfo, nullptr, &ImageMemory) != VK_SUCCESS)
			{
				UHE_LOG(L"Failed to commit image to GPU!\n");
				return false;
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
	return ImageFormat == UH_FORMAT_D16
		|| ImageFormat == UH_FORMAT_D24_S8
		|| ImageFormat == UH_FORMAT_D32F
		|| ImageFormat == UH_FORMAT_D32F_S8
		|| ImageFormat == UH_FORMAT_X8_D24;
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

	// again, layer settings is for VR
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

#if WITH_EDITOR
	if (InViewType == VK_IMAGE_VIEW_TYPE_CUBE)
	{
		CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		CreateInfo.subresourceRange.layerCount = 1;
		for (int32_t Idx = 0; Idx < 6; Idx++)
		{
			CreateInfo.subresourceRange.baseArrayLayer = Idx;
			if (vkCreateImageView(LogicalDevice, &CreateInfo, nullptr, &CubemapImageView[Idx]) != VK_SUCCESS)
			{
				UHE_LOG(L"Failed to create cubemap image views!\n");
			}
		}
	}
#endif

	ImageViewInfo = CreateInfo;
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

VkImageView UHTexture::GetImageView() const
{
	return ImageView;
}

#if WITH_EDITOR
VkImageView UHTexture::GetCubemapImageView(const int32_t SliceIndex) const
{
	return CubemapImageView[SliceIndex];
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

bool UHTexture::HasUploadedToGPU() const
{
	return bHasUploadedToGPU;
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