#pragma once
#include "ShaderClass.h"

class UHDepthPassShader : public UHShaderClass
{
public:
	UHDepthPassShader() {}
	UHDepthPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat)
		: UHShaderClass(InGfx, Name, typeid(UHDepthPassShader))
	{
		// DeferredPass: Bind all constants, visiable in VS/PS only
		// use storage buffer on materials
		for (uint32_t Idx = 0; Idx < UHConstantTypes::ConstantTypeMax; Idx++)
		{
			AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, (Idx == UHConstantTypes::Material) ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		}

		// bind opacity 
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

		// bind UV0 Buffer
		AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		CreateDescriptor();

		// check opacity texture
		std::vector<std::string> OpacityDefine = {};
		if (UHTexture* OpacityTex = InMat->GetTex(UHMaterialTextureType::Opacity))
		{
			OpacityDefine.push_back("WITH_OPACITY");
		}

		ShaderVS = InGfx->RequestShader("DepthPassVS", "Shaders/DepthPassShader.hlsl", "DepthVS", "vs_6_0", OpacityDefine);
		if (InMat->GetBlendMode() == UHBlendMode::Masked)
		{
			ShaderPS = InGfx->RequestShader("DepthPassPS", "Shaders/DepthPassShader.hlsl", "DepthPS", "ps_6_0", OpacityDefine);
		}

		// states
		UHRenderPassInfo Info = UHRenderPassInfo(InRenderPass, UHDepthInfo(true, true, VK_COMPARE_OP_GREATER)
			, InMat->GetCullMode()
			, InMat->GetBlendMode()
			, ShaderVS
			, ShaderPS
			, 1
			, PipelineLayout);

		GraphicState = InGfx->RequestGraphicState(Info);
	}
};