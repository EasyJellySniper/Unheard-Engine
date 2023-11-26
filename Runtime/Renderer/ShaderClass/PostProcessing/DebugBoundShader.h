#pragma once
#include "../ShaderClass.h"

#if WITH_EDITOR
class UHDebugBoundShader : public UHShaderClass
{
public:
	UHDebugBoundShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	virtual void Release(bool bDescriptorOnly = false) override;
	virtual void OnCompile() override;

	void BindParameters();
	UHRenderBuffer<UHDebugBoundConstant>* GetDebugBoundData(const int32_t FrameIdx) const;

private:
	UniquePtr<UHRenderBuffer<UHDebugBoundConstant>> DebugBoundData[GMaxFrameInFlight];
};
#endif