#pragma once
#include "../Engine/RenderResource.h"
#include <string>
#include <unordered_map>
#include "../../UnheardEngine.h"
#include "GPUMemory.h"
#include "TextureFormat.h"

enum class UHTextureVersion
{
	InitialTexture = 0,
	TextureVersionMax
};

enum class UHTextureType
{
	Texture2D,
	TextureCube
};

enum class UHTextureCompressionSettings
{
	CompressionNone,
	BC1,
	BC3,
	BC4,
	BC5,
	BC6H
};

struct UHTextureInfo
{
	UHTextureInfo()
		: UHTextureInfo(VK_IMAGE_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D, UHTextureFormat::UH_FORMAT_NONE, VkExtent2D(), VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, false)
	{
	}

	UHTextureInfo(VkImageType InType, VkImageViewType InViewType, UHTextureFormat InFormat, VkExtent2D InExtent, VkImageUsageFlags InUsage
		, bool bInIsRT)
		: Type(InType)
		, ViewType(InViewType)
		, Format(InFormat)
		, Extent(InExtent)
		, Usage(InUsage)
		, bIsRT(bInIsRT)
		, bIsShadowRT(false)
		, ReboundOffset(~0)
	{

	}

	VkImageType Type;
	VkImageViewType ViewType;
	UHTextureFormat Format;
	VkExtent2D Extent;
	VkImageUsageFlags Usage;
	bool bIsRT;
	bool bIsShadowRT;
	uint64_t ReboundOffset;
};

struct UHTextureSettings
{
	UHTextureSettings()
		: bIsLinear(false)
		, bIsNormal(false)
		, CompressionSetting(UHTextureCompressionSettings::CompressionNone)
		, bIsCompressed(false)
		, bIsHDR(false)
		, bUseMipmap(true)
	{
	}

	bool bIsLinear;
	bool bIsNormal;
	UHTextureCompressionSettings CompressionSetting;
	bool bIsCompressed;
	bool bIsHDR;
	bool bUseMipmap;
};

class UHGraphic;
class UHRenderBuilder;

// base texture class for textures. Texture2D, RenderTexture can inherit this
class UHTexture : public UHRenderResource
{
public:
	UHTexture();
	UHTexture(std::string InName, VkExtent2D InExtent, UHTextureFormat InFormat, UHTextureSettings InSettings);
	virtual ~UHTexture() {}
	// release
	virtual void Release();

	virtual void UploadToGPU(UHGraphic* InGfx, UHRenderBuilder& InRenderBuilder) {}
	virtual void GenerateMipMaps(UHGraphic* InGfx, UHRenderBuilder& InRenderBuilder) {}

#if WITH_EDITOR
	virtual std::vector<uint8_t> ReadbackTextureData() { return std::vector<uint8_t>(); }
	void SetTextureSettings(UHTextureSettings InSetting);
	void SetRawSourcePath(std::string InPath);
	void SetExtent(uint32_t Width, uint32_t Height);
#endif

	// set image
	void SetImage(VkImage InImage);
	void SetImageLayout(VkImageLayout InLayout, const uint32_t InMipIndex = 0);

	// get name
	std::string GetName() const;

	// get source path
	std::string GetSourcePath() const;

	// get raw source path
	std::string GetRawSourcePath() const;

	// get format
	UHTextureFormat GetFormat() const;

	// get extent
	VkExtent2D GetExtent() const;

	// get image
	VkImage GetImage() const;
	VkImageLayout GetImageLayout(const uint32_t InMipIndex = 0) const;

	// get image view
	VkImageView GetImageView() const;
	VkImageView GetImageView(int32_t MipIndex) const;
#if WITH_EDITOR
	VkImageView GetCubemapImageView(const int32_t SliceIndex, const int32_t MipIndex) const;
#endif

	// get image view info
	VkImageViewCreateInfo GetImageViewInfo() const;

	uint32_t GetMipMapCount() const;

	UHTextureSettings GetTextureSettings() const;

	void SetHasUploadedToGPU(bool bFlag);
	bool HasUploadedToGPU() const;
	void SetMipmapGenerated(bool bFlag);

	// is depth format?
	bool IsDepthFormat() const;

	bool operator==(const UHTexture& InTexture);

protected:
	bool Create(UHTextureInfo InInfo, UHGPUMemory* InSharedMemory);
	std::string SourcePath;
	std::string RawSourcePath;

	UHTextureFormat ImageFormat;
	VkExtent2D ImageExtent;
	VkDeviceMemory ImageMemory;
	VkImage ImageSource;

	bool bHasUploadedToGPU;
	bool bIsMipMapGenerated;
	bool bCreatePerMipImageView;
	UHTextureSettings TextureSettings;
	uint64_t MemoryOffset;
	UHTextureType TextureType;
	uint32_t MipMapCount;

private:

	// create image view based on stored format and image
	bool CreateImageView(VkImageViewType InViewType);
	bool bIsSourceCreatedByThis;

	// device variables, the image view per mip can be useful for render texture
	VkImageView ImageView;
	std::vector<VkImageView> ImageViewPerMip;
	VkImageViewCreateInfo ImageViewInfo;
	UHTextureInfo TextureInfo;
	std::vector<VkImageLayout> ImageLayouts;

#if WITH_EDITOR
	// individual face image view for editor use, assume it's 15 mipmap max
	static const int32_t CubemapImageViewCount = 90;
	VkImageView CubemapImageView[CubemapImageViewCount];
#endif
};