#pragma once
#include "../../UnheardEngine.h"
#include "Texture.h"
#include <vector>
#include <filesystem>
#include "RenderBuffer.h"

class UHGraphic;
class UHRenderBuilder;

class UHTexture2D : public UHTexture
{
public:
	UHTexture2D();
	UHTexture2D(std::string InName, std::string InSourcePath, VkExtent2D InExtent, UHTextureFormat InFormat, UHTextureSettings InSettings);
	virtual void Release() override;

	void ReleaseCPUTextureData();
	bool Import(std::filesystem::path InTexturePath);
	void SetTextureData(std::vector<uint8_t> InData);

#if WITH_EDITOR
	void Recreate(bool bNeedGeneratMipmap, const std::vector<uint8_t>& RawData);
	virtual std::vector<uint8_t> ReadbackTextureData() override;
	void Export(std::filesystem::path InTexturePath, bool bOverwrite = true);
#endif
	const std::vector<uint8_t>& GetTextureData() const;

	// upload texture data to GPU
	virtual void UploadToGPU(UHGraphic* InGfx, UHRenderBuilder& InRenderBuilder) override;

	// generate mip maps
	virtual void GenerateMipMaps(UHGraphic* InGfx, UHRenderBuilder& InRenderBuilder) override;

private:
	bool CreateTexture(bool bFromSharedMemory);

	std::vector<uint8_t> TextureData;
	std::vector<UHRenderBuffer<uint8_t>> RawStageBuffers;
	bool bSharedMemory;

	friend UHGraphic;
};