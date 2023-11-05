#pragma once
#include "Texture.h"

class UHGraphic;

class UHRenderTexture : public UHTexture
{
public:
	UHRenderTexture(std::string InName, VkExtent2D InExtent, UHTextureFormat InFormat, bool bReadWrite = false, bool bUseMipmap = false);

private:
	// create RT
	bool CreateRT();

	bool bIsReadWrite;
	bool bUseMipmap;
	friend UHGraphic;
};