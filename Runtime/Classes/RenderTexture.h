#pragma once
#include "Texture.h"

class UHGraphic;

class UHRenderTexture : public UHTexture
{
public:
	UHRenderTexture(std::string InName, VkExtent2D InExtent, UHTextureFormat InFormat, bool bIsLinear, bool bReadWrite = false, bool bIsShadowRT = false);

private:
	// create RT
	bool CreateRT();

	bool bIsReadWrite;
	bool bIsShadowRT;
	friend UHGraphic;
};