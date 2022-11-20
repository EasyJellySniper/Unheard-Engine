#pragma once
#include "../ShaderClass.h"

class UHRTDefaultHitGroupShader : public UHShaderClass
{
public:
	UHRTDefaultHitGroupShader() {}
	UHRTDefaultHitGroupShader(UHGraphic* InGfx, std::string Name)
		: UHShaderClass(InGfx, Name, typeid(UHRTDefaultHitGroupShader))
	{
		std::vector<std::string> Define = {"WITH_CLOSEST"};
		ClosestHitShader = InGfx->RequestShader("RTDefaultHitGroup", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTDefaultClosestHit", "lib_6_3", Define);

		Define = {"WITH_ANYHIT"};
		AnyHitShader = InGfx->RequestShader("RTDefaultHitGroup", "Shaders/RayTracing/RayTracingHitGroup.hlsl", "RTDefaultAnyHit", "lib_6_3", Define);
	}
};