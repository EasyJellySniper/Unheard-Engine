#include "RTReflectionMipmap.h"
#include "../../RenderBuilder.h"

UHRTReflectionMipmap::UHRTReflectionMipmap(UHGraphic* InGfx, std::string Name)
	: UHShaderClass(InGfx, Name, typeid(UHRTReflectionMipmap))
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

	PushConstantRange.offset = 0;
	PushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	PushConstantRange.size = sizeof(UHMipmapConstants);
	bPushDescriptor = true;

	CreateDescriptor();
	OnCompile();
}

void UHRTReflectionMipmap::OnCompile()
{
	ShaderCS = Gfx->RequestShader("RTReflectionMipmap", "Shaders/RayTracing/RTReflectionMipmap.hlsl", "MaxFilterMipmapCS", "cs_6_0");

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHRTReflectionMipmap::BindTextures(UHRenderBuilder& RenderBuilder, UHTexture* Input, UHTexture* Output, const uint32_t OutputMipIdx, const int32_t FrameIdx)
{
	// textures are keep changing, need to push it instead
	PushImage(Input, 0, true, OutputMipIdx - 1);
	PushImage(Output, 1, true, OutputMipIdx);
	FlushPushDescriptor(RenderBuilder.GetCmdList());
}