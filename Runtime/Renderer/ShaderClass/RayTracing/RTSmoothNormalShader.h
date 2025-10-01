#pragma once
#include "../ShaderClass.h"

class RTSmoothSceneNormalShader : public UHShaderClass
{
public:
	RTSmoothSceneNormalShader(UHGraphic* InGfx, std::string Name, bool bIsVertical);

	virtual void OnCompile() override;

	void BindParameters();

private:
	bool bIsVertical;
};