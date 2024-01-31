#pragma once
#include "../ShaderClass.h"

const int32_t GMaxGaussianFilterRadius = 5;
enum UHGaussianFilterType
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
	int32_t IterationCount;
	int32_t DownsizeFactor = 2;
};

class UHGaussianFilterShader : public UHShaderClass
{
public:
	UHGaussianFilterShader(UHGraphic* InGfx, std::string Name, const UHGaussianFilterType InType);
	virtual void Release(bool bDescriptorOnly = false) override;
	virtual void OnCompile() override;

	void BindParameters();
	void SetGaussianConstants(UHGaussianFilterConstants InConstants, const int32_t FrameIdx);

private:
	UHGaussianFilterType GaussianFilterType;
	UniquePtr<UHRenderBuffer<UHGaussianFilterConstants>> GaussianConstantsGPU[GMaxFrameInFlight];
};