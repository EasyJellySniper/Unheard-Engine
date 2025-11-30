#pragma once
#include "../ShaderClass.h"

class UHRTSmoothSceneNormalShader : public UHShaderClass
{
public:
	UHRTSmoothSceneNormalShader(UHGraphic* InGfx, std::string Name, bool bIsVertical);

	virtual void OnCompile() override;

	void BindParameters();

private:
	bool bIsVertical;
};