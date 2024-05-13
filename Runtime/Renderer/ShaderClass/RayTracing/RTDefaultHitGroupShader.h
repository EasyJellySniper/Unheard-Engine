#pragma once
#include "../ShaderClass.h"
#include "Runtime/Classes/Material.h"

class UHRTDefaultHitGroupShader : public UHShaderClass
{
public:
	UHRTDefaultHitGroupShader(UHGraphic* InGfx, std::string Name, const std::vector<UHMaterial*>& Materials);
	virtual void OnCompile() override {}

	void UpdateHitShader(UHGraphic* InGfx, UHMaterial* InMat);

private:
	std::vector<UHMaterial*> AllMaterials;
};