#pragma once
#include "../ShaderClass.h"

struct UHMipmapConstants
{
	uint32_t MipWidth;
	uint32_t MipHeight;
	uint32_t bUseAlphaWeight;
};

class UHRenderBuilder;
class UHRTReflectionMipmap : public UHShaderClass
{
public:
	UHRTReflectionMipmap(UHGraphic* InGfx, std::string Name);

	virtual void OnCompile() override;

	void BindTextures(UHRenderBuilder& RenderBuilder, UHTexture* Input, UHTexture* Output, const uint32_t OutputMipIdx, const int32_t FrameIdx);
};