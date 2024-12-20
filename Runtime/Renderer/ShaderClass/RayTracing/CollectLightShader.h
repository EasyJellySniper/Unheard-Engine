#pragma once
#include "../ShaderClass.h"

class UHCollectLightShader : public UHShaderClass
{
public:
	UHCollectLightShader(UHGraphic* InGfx, std::string Name, bool bCollectPointLight);
	virtual void OnCompile() override;

	void BindParameters();

private:
	bool bCollectPointLight;
};