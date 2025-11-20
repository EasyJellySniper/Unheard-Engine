#include "ClearUAVShader.h"

UHClearUAVShader::UHClearUAVShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHClearUAVShader), nullptr)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	PushConstantRange.offset = 0;
	PushConstantRange.size = sizeof(UHClearUAVConstants);
	PushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bPushDescriptor = true;

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHClearUAVShader::OnCompile()
{
	ShaderCS = Gfx->RequestShader("ClearUAVComputeShader", "Shaders/ClearUAVComputeShader.hlsl", "ClearUavCS", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}