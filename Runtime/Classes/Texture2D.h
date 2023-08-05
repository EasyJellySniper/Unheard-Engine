#pragma once
#include "../../UnheardEngine.h"
#include "Texture.h"
#include <vector>
#include <filesystem>
#include "RenderBuffer.h"

class UHGraphic;
class UHGraphicBuilder;

class UHTexture2D : public UHTexture
{
public:
	UHTexture2D();
	UHTexture2D(std::string InName, std::string InSourcePath, VkExtent2D InExtent, VkFormat InFormat, UHTextureSettings InSettings);

	void ReleaseCPUTextureData();
	bool Import(std::filesystem::path InTexturePath);
	void SetTextureData(std::vector<uint8_t> InData);

#if WITH_DEBUG
	virtual void Recreate() override;
	virtual std::vector<uint8_t> ReadbackTextureData() override;
	void Export(std::filesystem::path InTexturePath);
#endif
	std::vector<uint8_t>& GetTextureData();

	// upload texture data to GPU
	virtual void UploadToGPU(UHGraphic* InGfx, VkCommandBuffer InCmd, UHGraphicBuilder& InGraphBuilder) override;

	// generate mip maps
	virtual void GenerateMipMaps(UHGraphic* InGfx, VkCommandBuffer InCmd, UHGraphicBuilder& InGraphBuilder) override;

private:
	bool CreateTextureFromMemory();

	std::vector<uint8_t> TextureData;
	std::vector<UHRenderBuffer<uint8_t>> RawStageBuffers;

	friend UHGraphic;
};