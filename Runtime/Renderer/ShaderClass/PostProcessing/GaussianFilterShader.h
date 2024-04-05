#pragma once
#include "../ShaderClass.h"

const int32_t GMaxGaussianFilterRadius = 5;
enum class UHGaussianFilterType
{
	FilterHorizontal = 0,
	FilterVertical
};

struct UHGaussianFilterConstants
{
	uint32_t GBlurResolution[2];
	int32_t GBlurRadius;
	// Support up to 11 (Radius 5) weights
	float Weights[11];

	// following 3 won't be used by shader
	int32_t IterationCount;
	int32_t InputMip = 0;
	UHTextureFormat TempRTFormat = UHTextureFormat::UH_FORMAT_RGBA16F;
};

class UHGaussianFilterShader : public UHShaderClass
{
public:
	UHGaussianFilterShader(UHGraphic* InGfx, std::string Name, const UHGaussianFilterType InType);
	virtual void OnCompile() override;

	void BindParameters(UHRenderBuilder& RenderBuilder, const int32_t CurrentFrame, UHTexture* Input, UHTexture* Output);

private:
	UHGaussianFilterType GaussianFilterType;
};