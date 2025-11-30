#include "SoftRTShadowShader.h"
#include "../../RendererShared.h"

UHSoftRTShadowShader::UHSoftRTShadowShader(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHSoftRTShadowShader), nullptr)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateLayoutAndDescriptor();
	OnCompile();

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		SoftRTShadowConsts[Idx] =
			Gfx->RequestRenderBuffer<UHSoftRTShadowConstants>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, "ILConstants");
	}
}

void UHSoftRTShadowShader::Release()
{
	UHShaderClass::Release();

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UH_SAFE_RELEASE(SoftRTShadowConsts[Idx]);
	}
}

void UHSoftRTShadowShader::OnCompile()
{
	ShaderCS = Gfx->RequestShader("SoftRTShadowComputeShader", "Shaders/RayTracing/SoftRTShadowComputeShader.hlsl", "SoftRTShadowCS", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHSoftRTShadowShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindRWImage(GRTShadowResult, 1);
	BindConstant(SoftRTShadowConsts, 2, 0);
	BindImage(GRTSharedTextureRG, 3);

	// translucent depth contains opaque as well
	BindImage(GSceneMixedDepth, 4);

	BindImage(GSceneMip, 5);
	BindSampler(GPointClampedSampler, 6);
	BindSampler(GLinearClampedSampler, 7);
}

UHRenderBuffer<UHSoftRTShadowConstants>* UHSoftRTShadowShader::GetConstants(const int32_t FrameIdx) const
{
	return SoftRTShadowConsts[FrameIdx].get();
}