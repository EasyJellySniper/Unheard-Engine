#pragma once
#include "../ShaderClass.h"

#if WITH_EDITOR
struct UHDebugViewConstant
{
	uint32_t ViewMipLevel;
	uint32_t ViewAlpha;
};

class UHDebugViewShader : public UHShaderClass
{
public:
	UHDebugViewShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass);
	virtual void Release() override;
	virtual void OnCompile() override;

	void BindParameters();
	UHRenderBuffer<UHDebugViewConstant>* GetDebugViewData() const;

private:
	UniquePtr<UHRenderBuffer<UHDebugViewConstant>> DebugViewData;
};
#endif