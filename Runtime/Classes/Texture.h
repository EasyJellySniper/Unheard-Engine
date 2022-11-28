#pragma once
#include "../Engine/RenderResource.h"
#include <string>
#include <unordered_map>
#include "../../UnheardEngine.h"
#include "GPUMemory.h"

struct UHTextureInfo
{
	UHTextureInfo()
		: UHTextureInfo(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_UNDEFINED, VkExtent2D(), VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, false, false)
	{
	}

	UHTextureInfo(VkImageType InType, VkImageViewType InViewType, VkFormat InFormat, VkExtent2D InExtent, VkImageUsageFlags InUsage
		, bool bInIsRT, bool bInUseMipMap)
		: Type(InType)
		, ViewType(InViewType)
		, Format(InFormat)
		, Extent(InExtent)
		, Usage(InUsage)
		, bIsRT(bInIsRT)
		, bUseMipMap(bInUseMipMap)
		, bIsShadowRT(false)
	{

	}

	VkImageType Type;
	VkImageViewType ViewType;
	VkFormat Format;
	VkExtent2D Extent;
	VkImageUsageFlags Usage;
	bool bIsRT;
	bool bUseMipMap;
	bool bIsShadowRT;
};

class UHGraphic;
class UHGraphicBuilder;

// base texture class for textures. Texture2D, RenderTexture can inherit this
class UHTexture : public UHRenderResource
{
public:
	UHTexture();
	UHTexture(std::string InName, VkExtent2D InExtent, VkFormat InFormat, bool bInIsLinear);

	virtual void UploadToGPU(UHGraphic* InGfx, VkCommandBuffer InCmd, UHGraphicBuilder& InGraphBuilder) {};
	virtual void GenerateMipMaps(UHGraphic* InGfx, VkCommandBuffer InCmd, UHGraphicBuilder& InGraphBuilder) {};

	// release
	void Release();

	// set image
	void SetImage(VkImage InImage);

	// get name
	std::string GetName() const;

	// get source path
	std::string GetSourcePath() const;

	// get format
	VkFormat GetFormat() const;

	// get extent
	VkExtent2D GetExtent() const;

	// get image
	VkImage GetImage() const;

	// get image view
	VkImageView GetImageView() const;

	// get image view info
	VkImageViewCreateInfo GetImageViewInfo() const;

	uint32_t GetMipMapCount() const;

	bool HasUploadedToGPU() const;

	bool IsLinear() const;

	bool operator==(const UHTexture& InTexture);

protected:
	bool Create(UHTextureInfo InInfo, UHGPUMemory* InSharedMemory);
	std::string Name;
	std::string SourcePath;

	VkFormat ImageFormat;
	VkExtent2D ImageExtent;
	VkDeviceMemory ImageMemory;

	bool bHasUploadedToGPU;
	bool bIsMipMapGenerated;
	bool bIsLinear;

private:
	// is depth format?
	bool IsDepthFormat() const;

	// create image view based on stored format and image
	bool CreateImageView(VkImageViewType InViewType);

	bool bIsSourceCreatedByThis;

	// device variables
	VkImage ImageSource;
	VkImageView ImageView;
	VkImageViewCreateInfo ImageViewInfo;
	uint32_t MipMapCount;
	UHTextureInfo TextureInfo;
};