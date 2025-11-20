#pragma once
#include "ShaderClass.h"

struct UHClearUAVConstants
{
	uint32_t ResolutionX;
	uint32_t ResolutionY;
};

class UHClearUAVShader : public UHShaderClass
{
public:
	UHClearUAVShader(UHGraphic* InGfx, std::string Name);

	virtual void OnCompile() override;
};