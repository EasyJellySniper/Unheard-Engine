#pragma once
#include "../ShaderClass.h"

enum class UHUpsampleMethod
{
	Nearest2x2 = 0,
	NearestHorizontal
};

struct UHUpsampleConstants
{
	uint32_t OutputResolutionX;
	uint32_t OutputResolutionY;
};

class UHUpsampleShader : public UHShaderClass
{
public:
	UHUpsampleShader(UHGraphic* InGfx, std::string Name, const UHUpsampleMethod InMethod);
	virtual void OnCompile() override;
	void BindParameters(UHRenderBuilder& RenderBuilder, const int32_t CurrentFrame, UHTexture* Target, UHUpsampleConstants Consts);

private:
	UHUpsampleMethod UpsampleMethod;
};