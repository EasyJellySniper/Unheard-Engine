#include "LightCullingShader.h"
#include "../RendererShared.h"

UHLightCullingShader::UHLightCullingShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHLightCullingShader), nullptr)
{
	// bind system constants, point light buffers, output tile lists, and depth textures
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHLightCullingShader::OnCompile()
{
	ShaderCS = Gfx->RequestShader("LightCullingComputeShader", "Shaders/LightCullingComputeShader.hlsl", "LightCullingCS", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHLightCullingShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0);
	BindStorage(GPointLightBuffer, 1, 0, true);
	BindStorage(GSpotLightBuffer, 2, 0, true);
	BindStorage(GPointLightListBuffer.get(), 3, 0, true);
	BindStorage(GPointLightListTransBuffer.get(), 4, 0, true);
	BindStorage(GSpotLightListBuffer.get(), 5, 0, true);
	BindStorage(GSpotLightListTransBuffer.get(), 6, 0, true);
	BindImage(GSceneDepth, 7);
	BindImage(GSceneTranslucentDepth, 8);
}