#pragma once
#include "../UnheardEngine.h"

#if WITH_DEBUG

#include "../Runtime/Classes/Utility.h"
#include <string>
#include <vector>
#include <filesystem>
#include "../Runtime/Classes/Texture2D.h"
using namespace Microsoft::WRL;

#define RETURN_IF_FAILED(x) if(x != S_OK) return std::vector<uint8_t>();

class UHGraphic;

class UHTextureImporter
{
public:
	UHTextureImporter();
	UHTextureImporter(UHGraphic* InGfx);

	// load texture from file, reference: https://github.com/microsoft/Xbox-ATG-Samples/blob/main/PCSamples/IntroGraphics/SimpleTexturePC12/SimpleTexturePC12.cpp
	std::vector<uint8_t> LoadTexture(std::wstring Filename, uint32_t& Width, uint32_t& Height);
	UHTexture* ImportRawTexture(std::filesystem::path SourcePath, std::filesystem::path OutputFolder, UHTextureSettings InSettings);

private:
	UHGraphic* Gfx;
};
#endif