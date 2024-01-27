#pragma once
#include "../ShaderClass.h"

enum UHGaussianBlurType
{
	BlurHorizontal = 0,
	BlurVertical
};

class UHGaussianBlurShader : public UHShaderClass
{
public:
	UHGaussianBlurShader(UHGraphic* InGfx, std::string Name, const UHGaussianBlurType InType);
	virtual void Release(bool bDescriptorOnly = false) override;
	virtual void OnCompile() override;

	void BindParameters();

private:
	UHGaussianBlurType GaussianBlurType;
};