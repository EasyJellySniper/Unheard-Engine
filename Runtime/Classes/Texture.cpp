#include "Texture.h"
#include "../../UnheardEngine.h"
#include "../Classes/Utility.h"
#include "../CoreGlobals.h"

UHTexture::UHTexture()
	: UHTexture("", VkExtent2D(), VK_FORMAT_R8G8B8A8_SRGB, false)
{

}

UHTexture::UHTexture(std::string InName, VkExtent2D InExtent, VkFormat InFormat, bool bInIsLinear)
	: Name(InName)
	, ImageFormat(InFormat)
	, ImageExtent(InExtent)
	, ImageSource(VK_NULL_HANDLE)
	, ImageView(VK_NULL_HANDLE)
	, ImageMemory(VK_NULL_HANDLE)
	, ImageViewInfo(VkImageViewCreateInfo{})
	, bIsSourceCreatedByThis(false)
	, bHasUploadedToGPU(false)
	, bIsMipMapGenerated(false)
	, bIsLinear(bInIsLinear)
{

}

void UHTexture::Release()
{
	vkFreeMemory(LogicalDevice, ImageMemory, nullptr);
	vkDestroyImageView(LogicalDevice, ImageView, nullptr);

	// only destroy source if it's created by this
	// image like swap chain can't be destroyed here
	if (bIsSourceCreatedByThis)
	{
		vkDestroyImage(LogicalDevice, ImageSource, nullptr);
	}
}

void UHTexture::SetImage(VkImage InImage)
{
	ImageSource = InImage;
}

bool UHTexture::Create(UHTextureInfo InInfo, UHGPUMemory* InSharedMemory)
{
	ImageFormat = InInfo.Format;
	ImageExtent = InInfo.Extent;
	MipMapCount = InInfo.bUseMipMap ? static_cast<uint32_t>(std::floor(std::log2((std::max)(ImageExtent.width, ImageExtent.height)))) + 1
		: 1;
	TextureInfo = InInfo;

	// only create if the source
	if (ImageSource == VK_NULL_HANDLE)
	{
		VkImageCreateInfo CreateInfo{};
		CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		CreateInfo.imageType = InInfo.Type;
		CreateInfo.format = ImageFormat;
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

		// try to commit image to GPU memory
		VkMemoryRequirements MemRequirements;
		vkGetImageMemoryRequirements(LogicalDevice, ImageSource, &MemRequirements);

		// bind to shared memory if available, otherwise, creating memory individually
		if (InSharedMemory)
		{
			uint64_t Offset = InSharedMemory->BindMemory(MemRequirements.size, ImageSource);
			if (Offset == ~0)
			{
				UHE_LOG(L"Failed to bind image to GPU. Exceed image memory budget!\n");
				return false;
			}
		}
		else
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
	return ImageFormat == VK_FORMAT_D16_UNORM
		|| ImageFormat == VK_FORMAT_D24_UNORM_S8_UINT
		|| ImageFormat == VK_FORMAT_D32_SFLOAT
		|| ImageFormat == VK_FORMAT_D32_SFLOAT_S8_UINT
		|| ImageFormat == VK_FORMAT_X8_D24_UNORM_PACK32;
}

bool UHTexture::CreateImageView(VkImageViewType InViewType)
{
	// create image view
	VkImageViewCreateInfo CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	CreateInfo.image = ImageSource;
	CreateInfo.viewType = InViewType;
	CreateInfo.format = ImageFormat;
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

VkFormat UHTexture::GetFormat() const
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

VkImageViewCreateInfo UHTexture::GetImageViewInfo() const
{
	return ImageViewInfo;
}

uint32_t UHTexture::GetMipMapCount() const
{
	return MipMapCount;
}

bool UHTexture::HasUploadedToGPU() const
{
	return bHasUploadedToGPU;
}

bool UHTexture::IsLinear() const
{
	return bIsLinear;
}

bool UHTexture::operator==(const UHTexture& InTexture)
{
	return InTexture.Name == Name
		&& InTexture.ImageFormat == ImageFormat
		&& InTexture.ImageExtent.width == ImageExtent.width
		&& InTexture.ImageExtent.height == ImageExtent.height;
}