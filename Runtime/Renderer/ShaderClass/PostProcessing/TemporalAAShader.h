#pragma once
#include "../ShaderClass.h"

class UHTemporalAAShader : public UHShaderClass
{
public:
	UHTemporalAAShader(UHGraphic* InGfx, std::string Name);
	virtual void OnCompile() override;

	void BindParameters();
};