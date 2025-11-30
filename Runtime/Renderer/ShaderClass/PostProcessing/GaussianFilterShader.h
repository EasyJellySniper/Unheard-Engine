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
	void Release(UHGraphic* InGfx)
	{
		for (size_t Idx = 0; Idx < GaussianFilterTempRT0.size(); Idx++)
		{
			if (GaussianFilterTempRT0[Idx] != nullptr)
			{
				InGfx->RequestReleaseRT(GaussianFilterTempRT0[Idx]);
				InGfx->RequestReleaseRT(GaussianFilterTempRT1[Idx]);
			}
		}

		GaussianFilterTempRT0.clear();
		GaussianFilterTempRT1.clear();
	}

	uint32_t GBlurResolution[2];
	int32_t GBlurRadius;
	// Support up to 11 (Radius 5) weights
	float Weights[11];

	// following 3 won't be used by shader
	int32_t IterationCount;
	int32_t InputMip = 0;

	// hold the temp RT
	std::vector<UHRenderTexture*> GaussianFilterTempRT0;
	std::vector<UHRenderTexture*> GaussianFilterTempRT1;
};

class UHGaussianFilterShader : public UHShaderClass
{
public:
	UHGaussianFilterShader(UHGraphic* InGfx, std::string Name, const UHGaussianFilterType InType);
	virtual void OnCompile() override;

	void BindParameters(UHRenderBuilder& RenderBuilder, UHTexture* Input, UHTexture* Output);

private:
	UHGaussianFilterType GaussianFilterType;
};