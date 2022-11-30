#pragma once
#include "ShaderClass.h"

class UHBasePassShader : public UHShaderClass
{
public:
	UHBasePassShader() {}
	UHBasePassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bEnableDepthPrePass)
		: UHShaderClass(InGfx, Name, typeid(UHBasePassShader))
	{
		// DeferredPass: Bind all constants, visiable in VS/PS only
		// use storage buffer on materials
		for (uint32_t Idx = 0; Idx < UHConstantTypes::ConstantTypeMax; Idx++)
		{
			AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, (Idx == UHConstantTypes::Material) ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		}

		// Bind texture/sampler inputs as well, following constants
		for (int32_t Idx = 0; Idx < UHMaterialTextureType::TextureTypeMax; Idx++)
		{
			AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
			AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
		}

		// bind UV0/Normal/Tangent buffer
		AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

		CreateDescriptor();
		ShaderVS = InGfx->RequestShader("BaseVertexShader", "Shaders/BaseVertexShader.hlsl", "BaseVS", "vs_6_0", InMat->GetMaterialDefines(VS));
		ShaderPS = InGfx->RequestShader("BasePixelShader", "Shaders/BasePixelShader.hlsl", "BasePS", "ps_6_0", InMat->GetMaterialDefines(PS));

		// states
		UHRenderPassInfo Info = UHRenderPassInfo(InRenderPass
			// adjust depth info based on depth pre pass setting
			, UHDepthInfo(true, !bEnableDepthPrePass, (bEnableDepthPrePass) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER)
			, InMat->GetCullMode()
			, InMat->GetBlendMode()
			, ShaderVS
			, ShaderPS
			, GNumOfGBuffers
			, PipelineLayout);

		GraphicState = InGfx->RequestGraphicState(Info);
	}
};