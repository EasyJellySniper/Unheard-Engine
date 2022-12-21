#pragma once
#include "../ShaderClass.h"

class UHTemporalAAShader : public UHShaderClass
{
public:
	UHTemporalAAShader() {}
	UHTemporalAAShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	void BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
		, const UHRenderTexture* PreviousSceneResult
		, const UHRenderTexture* MotionVectorRT
		, const UHRenderTexture* PrevMotionVectorRT
		, const UHRenderTexture* SceneDepth
		, const UHSampler* LinearClampedSampler);
};