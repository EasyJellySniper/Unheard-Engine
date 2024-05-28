#include "BlockCompressionShader.h"

#if WITH_EDITOR
UHBlockCompressionShader::UHBlockCompressionShader(UHGraphic* InGfx, std::string Name, std::string InEntry)
	: UHShaderClass(InGfx, Name, typeid(UHBlockCompressionShader))
	, EntryFunction(InEntry)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHBlockCompressionShader::OnCompile()
{
	// entry function can vary
	std::vector<std::string> Macro = { "USE_" + EntryFunction };
	ShaderCS = Gfx->RequestShader("BlockCompressionOldShader", "Shaders/BlockCompressionOldShader.hlsl", EntryFunction, "cs_6_0", Macro);

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

UHBlockCompressionNewShader::UHBlockCompressionNewShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHBlockCompressionNewShader))
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHBlockCompressionNewShader::OnCompile()
{
	ShaderCS = Gfx->RequestShader("BlockCompressionNewShader", "Shaders/BlockCompressionNewShader.hlsl", "BlockCompressHDR", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}
#endif