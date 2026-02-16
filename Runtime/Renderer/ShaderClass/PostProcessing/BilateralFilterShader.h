#pragma once
#include "../ShaderClass.h"

const int32_t GMaxBilateralFilterRadius = 5;
enum class UHBilaterialFilterType
{
	FilterHorizontal = 0,
	FilterVertical
};

struct UHBilateralFilterConstants
{
	void Release(UHGraphic* InGfx)
	{
		if (FilterTempRT0 != nullptr)
		{
			InGfx->RequestReleaseRT(FilterTempRT0);
		}

		if (FilterTempRT1 != nullptr)
		{
			InGfx->RequestReleaseRT(FilterTempRT1);
		}

		FilterTempRT0 = nullptr;
		FilterTempRT1 = nullptr;
	}

	uint32_t FilterResolution[2];
	int32_t KernalRadius;
	int32_t SceneBufferUpsample;
	float SigmaS;
	float SigmaD;
	float SigmaN;
	float SigmaC;
	int32_t IterationCount;

	// hold the temp RT
	UHRenderTexture* FilterTempRT0;
	UHRenderTexture* FilterTempRT1;
};

class UHBilateralFilterShader : public UHShaderClass
{
public:
	UHBilateralFilterShader(UHGraphic* InGfx, std::string Name, const UHBilaterialFilterType InType);
	virtual void OnCompile() override;

	void BindParameters(UHRenderBuilder& RenderBuilder, UHTexture* Input, UHTexture* Output, const int32_t FrameIdx);

private:
	UHBilaterialFilterType FilterType;
};