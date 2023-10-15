#pragma once
#include "../Engine/RenderResource.h"
#include <string>
#include <unordered_map>
#include "../../UnheardEngine.h"
#include "GPUMemory.h"

enum UHTextureVersion
{
	InitialTexture = 0,
	TextureVersionMax
};

enum UHTextureType
{
	Texture2D,
	TextureCube
};

enum UHTextureCompressionSettings
{
	CompressionNone,
	BC1,
	BC3,
	BC4,
	BC5
};

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
		, ReboundOffset(~0)
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
	uint64_t ReboundOffset;
};

struct UHTextureSettings
{
	UHTextureSettings()
		: bIsLinear(false)
		, bIsNormal(false)
		, CompressionSetting(CompressionNone)
		, bIsCompressed(false)
	{
	}

	bool bIsLinear;
	bool bIsNormal;
	UHTextureCompressionSettings CompressionSetting;
	bool bIsCompressed;
};

class UHGraphic;
class UHRenderBuilder;

// base texture class for textures. Texture2D, RenderTexture can inherit this
class UHTexture : public UHRenderResource
{
public:
	UHTexture();
	UHTexture(std::string InName, VkExtent2D InExtent, VkFormat InFormat, UHTextureSettings InSettings);
	virtual ~UHTexture() {}

	virtual void UploadToGPU(UHGraphic* InGfx, VkCommandBuffer InCmd, UHRenderBuilder& InRenderBuilder) {}
	virtual void GenerateMipMaps(UHGraphic* InGfx, VkCommandBuffer InCmd, UHRenderBuilder& InRenderBuilder) {}

#if WITH_EDITOR
	virtual void Recreate() {}
	virtual std::vector<uint8_t> ReadbackTextureData() { return std::vector<uint8_t>(); }
	void SetTextureSettings(UHTextureSettings InSetting);
	void SetRawSourcePath(std::string InPath);
	void SetExtent(uint32_t Width, uint32_t Height);
#endif

	// release
	void Release();

	// set image
	void SetImage(VkImage InImage);

	// get name
	std::string GetName() const;

	// get source path
	std::string GetSourcePath() const;

	// get raw source path
	std::string GetRawSourcePath() const;

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

	UHTextureSettings GetTextureSettings() const;

	bool HasUploadedToGPU() const;

	// is depth format?
	bool IsDepthFormat() const;

	bool operator==(const UHTexture& InTexture);

protected:
	bool Create(UHTextureInfo InInfo, UHGPUMemory* InSharedMemory);
	std::string Name;
	std::string SourcePath;
	std::string RawSourcePath;

	VkFormat ImageFormat;
	VkExtent2D ImageExtent;
	VkDeviceMemory ImageMemory;
	VkImage ImageSource;

	bool bHasUploadedToGPU;
	bool bIsMipMapGenerated;
	UHTextureSettings TextureSettings;
	uint64_t MemoryOffset;
	UHTextureVersion TextureVersion;
	UHTextureType TextureType;

private:

	// create image view based on stored format and image
	bool CreateImageView(VkImageViewType InViewType);
	bool bIsSourceCreatedByThis;

	// device variables
	VkImageView ImageView;
	VkImageViewCreateInfo ImageViewInfo;
	uint32_t MipMapCount;
	UHTextureInfo TextureInfo;
};