#pragma once
#include "Texture.h"
#include "RenderBuffer.h"

// render texture settings with various constructors for use
struct UHRenderTextureSettings
{
	UHRenderTextureSettings()
		: bIsReadWrite(false)
		, bUseMipmap(false)
		, NumSlices(1)
		, bIsVolume(false)
		, OverrideTexture(nullptr)
	{
	}

	bool bIsReadWrite;
	bool bUseMipmap;
	uint32_t NumSlices;
	bool bIsVolume;
	VkImage OverrideTexture;
};

class UHGraphic;
class UHRenderTexture : public UHTexture
{
public:
	UHRenderTexture(std::string InName, VkExtent2D InExtent, UHTextureFormat InFormat, UHRenderTextureSettings InRTSettings);

	// generate mip maps
	virtual void GenerateMipMaps(UHGraphic* InGfx, UHRenderBuilder& InRenderBuilder) override;

#if WITH_EDITOR
	virtual std::vector<uint8_t> ReadbackTextureData() override;
#endif

private:
	// create RT
	bool CreateRT();

	UHRenderTextureSettings RenderTextureSettings;

	friend UHGraphic;
};