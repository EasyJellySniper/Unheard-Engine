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
	UHTextureCube(std::string InName, VkExtent2D InExtent, VkFormat InFormat);
	void Build(UHGraphic* InGfx, VkCommandBuffer InCmd, UHRenderBuilder& InRenderBuilder);

private:
	bool CreateCube(std::vector<UHTexture2D*> InSlices);
	std::vector<UHTexture2D*> Slices;
	bool bIsCubeBuilt;

	friend UHGraphic;
};