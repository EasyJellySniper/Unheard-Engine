#pragma once
#include "../ShaderClass.h"

class UHToneMappingShader : public UHShaderClass
{
public:
	UHToneMappingShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	virtual void Release(bool bDescriptorOnly = false) override;
	virtual void OnCompile() override;

	void BindParameters();
	UHRenderBuffer<uint32_t>* GetToneMapData(int32_t FrameIdx) const;

private:
	UniquePtr<UHRenderBuffer<uint32_t>> ToneMapData[GMaxFrameInFlight];
};