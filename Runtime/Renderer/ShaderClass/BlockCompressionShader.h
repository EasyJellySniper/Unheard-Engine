#pragma once
#include "ShaderClass.h"

#if WITH_EDITOR
class UHBlockCompressionShader : public UHShaderClass
{
public:
	UHBlockCompressionShader(UHGraphic* InGfx, std::string Name, std::string InEntry);
	virtual void OnCompile() override;

private:
	std::string EntryFunction;
};

class UHBlockCompressionNewShader : public UHShaderClass
{
public:
	UHBlockCompressionNewShader(UHGraphic* InGfx, std::string Name);
	virtual void OnCompile() override;
};
#endif