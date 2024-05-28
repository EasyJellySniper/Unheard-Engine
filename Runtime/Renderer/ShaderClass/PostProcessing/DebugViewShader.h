#pragma once
#include "../ShaderClass.h"

#if WITH_EDITOR
class UHDebugViewShader : public UHShaderClass
{
public:
	UHDebugViewShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	virtual void Release() override;
	virtual void OnCompile() override;

	void BindParameters();
	UHRenderBuffer<uint32_t>* GetDebugViewData() const;

private:
	UniquePtr<UHRenderBuffer<uint32_t>> DebugViewData;
};
#endif