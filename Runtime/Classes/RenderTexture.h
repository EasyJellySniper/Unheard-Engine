#pragma once
#include "Texture.h"
#include "RenderBuffer.h"

class UHGraphic;

class UHRenderTexture : public UHTexture
{
public:
	UHRenderTexture(std::string InName, VkExtent2D InExtent, UHTextureFormat InFormat, bool bReadWrite = false, bool bUseMipmap = false);

	// generate mip maps
	virtual void GenerateMipMaps(UHGraphic* InGfx, UHRenderBuilder& InRenderBuilder) override;

#if WITH_EDITOR
	virtual std::vector<uint8_t> ReadbackTextureData() override;
#endif

private:
	// create RT
	bool CreateRT();

	bool bIsReadWrite;
	bool bUseMipmap;
	friend UHGraphic;
};