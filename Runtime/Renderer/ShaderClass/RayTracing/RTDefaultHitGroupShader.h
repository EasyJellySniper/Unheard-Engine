#pragma once
#include "../ShaderClass.h"
#include "../../../Classes/Material.h"

class UHRTDefaultHitGroupShader : public UHShaderClass
{
public:
	UHRTDefaultHitGroupShader() {}
	UHRTDefaultHitGroupShader(UHGraphic* InGfx, std::string Name, const std::vector<UHMaterial*>& Materials);
	void UpdateHitShader(UHGraphic* InGfx, UHMaterial* InMat);

private:
	std::vector<UHMaterial*> AllMaterials;
};