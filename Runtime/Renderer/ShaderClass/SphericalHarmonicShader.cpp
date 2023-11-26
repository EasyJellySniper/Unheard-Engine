#include "SphericalHarmonicShader.h"
#include "../RendererShared.h"

UHSphericalHarmonicShader::UHSphericalHarmonicShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHSphericalHarmonicShader))
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateDescriptor();
	OnCompile();

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		SH9Constants[Idx] = Gfx->RequestRenderBuffer<UHSphericalHarmonicConstants>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	}
}

void UHSphericalHarmonicShader::Release(bool bDescriptorOnly)
{
	UHShaderClass::Release(bDescriptorOnly);

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(SH9Constants[Idx]);
	}
}

void UHSphericalHarmonicShader::OnCompile()
{
	ShaderCS = Gfx->RequestShader("SphericalHarmonicShader", "Shaders/SphericalHarmonicComputeShader.hlsl", "GenerateSHParameterCS", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

UHRenderBuffer<UHSphericalHarmonicConstants>* UHSphericalHarmonicShader::GetSH9Constants(const int32_t FrameIdx) const
{
	return SH9Constants[FrameIdx].get();
}

void UHSphericalHarmonicShader::BindParameters()
{
	BindStorage(GSH9Data.get(), 0);
	BindConstant(SH9Constants, 1);
	BindImage(GSkyLightCube, 2);
	BindSampler(GSkyCubeSampler, 3);
}