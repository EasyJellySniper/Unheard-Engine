#pragma once
#include "ShaderClass.h"

// shader implementation for camera motion
class UHMotionCameraPassShader : public UHShaderClass
{
public:
	UHMotionCameraPassShader() {}
	UHMotionCameraPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
		: UHShaderClass(InGfx, Name, typeid(UHMotionCameraPassShader))
	{
		// system const + depth texture + sampler
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

		CreateDescriptor();
		ShaderVS = InGfx->RequestShader("PostProcessVS", "Shaders/PostProcessVS.hlsl", "PostProcessVS", "vs_6_0");
		ShaderPS = InGfx->RequestShader("MotionCameraShader", "Shaders/MotionVectorShader.hlsl", "CameraMotionPS", "ps_6_0");

		// states
		UHRenderPassInfo Info = UHRenderPassInfo(InRenderPass, UHDepthInfo(false, false, VK_COMPARE_OP_ALWAYS)
			, VK_CULL_MODE_NONE
			, UHBlendMode::Opaque
			, ShaderVS
			, ShaderPS
			, 1
			, PipelineLayout);

		GraphicState = InGfx->RequestGraphicState(Info);
	}
};

// shader implementation for object motion
class UHMotionObjectPassShader : public UHShaderClass
{
public:
	UHMotionObjectPassShader() {}
	UHMotionObjectPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat)
		: UHShaderClass(InGfx, Name, typeid(UHMotionObjectPassShader))
	{
		VkDescriptorSetLayoutBinding LayoutBinding{};

		// Motion pass: constants + opacity image for cutoff (if there is any)
		for (uint32_t Idx = 0; Idx < UHConstantTypes::ConstantTypeMax; Idx++)
		{
			AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, (Idx == UHConstantTypes::Material) ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		}
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
		CreateDescriptor();

		ShaderVS = InGfx->RequestShader("MotionObjectVS", "Shaders/MotionVectorShader.hlsl", "MotionObjectVS", "vs_6_0");

		// find opacity define
		std::vector<std::string> Defines = InMat->GetMaterialDefines(PS);
		std::vector<std::string> ActualDefines;
		std::string OpacityDefine = InMat->GetTexDefineName(UHMaterialTextureType::Opacity);
		if (UHUtilities::FindByElement(Defines, std::string(OpacityDefine)))
		{
			ActualDefines.push_back(OpacityDefine);
		}
		ShaderPS = InGfx->RequestShader("MotionObjectPS", "Shaders/MotionVectorShader.hlsl", "MotionObjectPS", "ps_6_0", ActualDefines);

		// states, enable depth test but don't write it
		UHRenderPassInfo Info = UHRenderPassInfo(InRenderPass, UHDepthInfo(true, false, VK_COMPARE_OP_GREATER_OR_EQUAL)
			, InMat->GetCullMode()
			, InMat->GetBlendMode()
			, ShaderVS
			, ShaderPS
			, 1
			, PipelineLayout);

		GraphicState = InGfx->RequestGraphicState(Info);
	}
};