#pragma once
#include "Texture2D.h"

class UHGraphic;
class UHRenderBuilder;

// UH texture cube class
// for now this should be built up with several Texture2D slices
// consider saving it as asset in the future
class UHTextureCube : public UHTexture
{
public:
	UHTextureCube();
	UHTextureCube(std::string InName, VkExtent2D InExtent, UHTextureFormat InFormat, UHTextureSettings InSettings);

	virtual void Release() override;

	void ReleaseCPUData();
	void SetCubeData(std::vector<uint8_t> InData, int32_t Slice);
	std::vector<uint8_t> GetCubeData(int32_t Slice) const;
	bool IsBuilt() const;

	bool Import(std::filesystem::path InCubePath);
#if WITH_EDITOR
	void SetSlices(std::vector<UHTexture2D*> InSlices);
	void Recreate(UHTextureFormat NewFormat);
	void Export(std::filesystem::path InCubePath);
	void SetSourcePath(std::filesystem::path InPath);
	size_t GetDataSize() const;
#endif

	void Build(UHGraphic* InGfx, UHRenderBuilder& InRenderBuilder);
	void UploadSlice(UHGraphic* InGfx, UHRenderBuilder& InRenderBuilder, const int32_t SliceIndex, const uint32_t MipMapCount);

private:
	bool CreateCube(std::vector<UHTexture2D*> InSlices);
	bool CreateCube();

	std::vector<uint8_t> SliceData[6];
	std::vector<UHTexture2D*> Slices;
	std::vector<UHRenderBuffer<uint8_t>> RawStageBuffers[6];
	bool bIsCubeBuilt;

	friend UHGraphic;
};