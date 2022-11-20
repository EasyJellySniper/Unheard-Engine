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
	UHTexture2D(std::string InName, std::string InSourcePath, VkExtent2D InExtent, VkFormat InFormat, bool bInIsLinear);
	void ReleaseCPUTextureData();

	bool Import(std::filesystem::path InTexturePath);

#if WITH_DEBUG
	void Export(std::filesystem::path InTexturePath);
#endif

	void SetTextureData(std::vector<uint8_t> InData);
	std::vector<uint8_t>& GetTextureData();

	// upload texture data to GPU
	virtual void UploadToGPU(UHGraphic* InGfx, VkCommandBuffer InCmd, UHGraphicBuilder& InGraphBuilder) override;

	// generate mip maps
	virtual void GenerateMipMaps(UHGraphic* InGfx, VkCommandBuffer InCmd, UHGraphicBuilder& InGraphBuilder) override;

private:
	bool CreateTextureFromMemory(uint32_t Width, uint32_t Height, std::vector<uint8_t> InTextureData, bool bIsLinear = false);

	std::vector<uint8_t> TextureData;
	UHRenderBuffer<uint8_t> StageBuffer;

	friend UHGraphic;
};