#include "LightCullingShader.h"

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
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateDescriptor();

	ShaderCS = InGfx->RequestShader("LightCullingComputeShader", "Shaders/LightCullingComputeShader.hlsl", "LightCullingCS", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHLightCullingShader::BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const std::array<UniquePtr<UHRenderBuffer<UHPointLightConstants>>, GMaxFrameInFlight>& PointLightConst
	, const std::array<UniquePtr<UHRenderBuffer<UHSpotLightConstants>>, GMaxFrameInFlight>& SpotLightConst
	, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightList
	, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightListTrans
	, const UniquePtr<UHRenderBuffer<uint32_t>>& SpotLightList
	, const UniquePtr<UHRenderBuffer<uint32_t>>& SpotLightListTrans
	, const UHRenderTexture* DepthTexture
	, const UHRenderTexture* TransDepthTexture
	, const UHSampler* LinearClampped)
{
	BindConstant(SysConst, 0);
	BindStorage(PointLightConst, 1, 0, true);
	BindStorage(SpotLightConst, 2, 0, true);
	BindStorage(PointLightList.get(), 3, 0, true);
	BindStorage(PointLightListTrans.get(), 4, 0, true);
	BindStorage(SpotLightList.get(), 5, 0, true);
	BindStorage(SpotLightListTrans.get(), 6, 0, true);
	BindImage(DepthTexture, 7);
	BindImage(TransDepthTexture, 8);
	BindSampler(LinearClampped, 9);
}