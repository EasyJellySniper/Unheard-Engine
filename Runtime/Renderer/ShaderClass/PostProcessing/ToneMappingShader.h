#pragma once
#include "../ShaderClass.h"

struct UHToneMapData
{
	UHToneMapData(float InGamma, float InPaperWhite, float InContrast)
		: GammaCorrection(InGamma)
		, HDRPaperWhiteNits(InPaperWhite)
		, HDRContrast(InContrast)
	{

	}

	float GammaCorrection;
	float HDRPaperWhiteNits;
	float HDRContrast;
};

class UHToneMappingShader : public UHShaderClass
{
public:
	UHToneMappingShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	virtual void Release() override;
	virtual void OnCompile() override;

	void BindParameters();
	void UploadToneMapData(UHToneMapData InData, const int32_t CurrentFrame);
	void BindInputImage(UHTexture* InImage, const int32_t CurrentFrameRT);

private:
	UniquePtr<UHRenderBuffer<UHToneMapData>> ToneMapData[GMaxFrameInFlight];
};