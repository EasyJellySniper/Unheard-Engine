#pragma once
#include "../ShaderClass.h"

class RTSmoothReflectShader : public UHShaderClass
{
public:
	RTSmoothReflectShader(UHGraphic* InGfx, std::string Name, bool bIsVertical);

	virtual void OnCompile() override;

	void BindParameters();

private:
	bool bIsVertical;
};