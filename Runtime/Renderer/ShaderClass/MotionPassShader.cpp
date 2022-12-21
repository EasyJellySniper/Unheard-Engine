#include "MotionPassShader.h"
#include "../../Components/MeshRenderer.h"

UHMotionCameraPassShader::UHMotionCameraPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHMotionCameraPassShader))
{
	// system const + depth texture + sampler
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateDescriptor();
	ShaderVS = InGfx->RequestShader("PostProcessVS", "Shaders/PostProcessing/PostProcessVS.hlsl", "PostProcessVS", "vs_6_0");
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

void UHMotionCameraPassShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const UHRenderTexture* DepthTexture
	, const UHSampler* PointClamppedSampler)
{
	BindConstant(SysConst, 0);
	BindImage(DepthTexture, 1);
	BindSampler(PointClamppedSampler, 2);
}

UHMotionObjectPassShader::UHMotionObjectPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat)
	: UHShaderClass(InGfx, Name, typeid(UHMotionObjectPassShader))
{
	// Motion pass: constants + opacity image for cutoff (if there is any)
	for (uint32_t Idx = 0; Idx < UHConstantTypes::ConstantTypeMax; Idx++)
	{
		if (Idx == UHConstantTypes::Material)
		{
			AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		}
		else if (Idx == UHConstantTypes::Object)
		{
			AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		}
		else
		{
			AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		}
	}

	// occlusion buffer + opacity + sampler
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	// UV0 Buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	CreateDescriptor();

	// check occlusion test define
	std::vector<std::string> OcclusionTestDefine;
	if (InGfx->IsRayTracingOcclusionTestEnabled())
	{
		OcclusionTestDefine.push_back("WITH_OCCLUSION_TEST");
	}
	ShaderVS = InGfx->RequestShader("MotionObjectVS", "Shaders/MotionVectorShader.hlsl", "MotionObjectVS", "vs_6_0", OcclusionTestDefine);

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

void UHMotionObjectPassShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const std::array<std::unique_ptr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
	, const std::array<std::unique_ptr<UHRenderBuffer<UHMaterialConstants>>, GMaxFrameInFlight>& MatConst
	, const std::array<std::unique_ptr<UHRenderBuffer<uint32_t>>, GMaxFrameInFlight>& OcclusionConst
	, const UHMeshRendererComponent* InRenderer)
{
	BindConstant(SysConst, 0);
	BindConstant(ObjConst, 1, InRenderer->GetBufferDataIndex());
	BindStorage(MatConst, 2, InRenderer->GetMaterial()->GetBufferDataIndex());

	if (Gfx->IsRayTracingOcclusionTestEnabled())
	{
		BindStorage(OcclusionConst, 3, 0, true);
	}

	if (UHTexture* OpacityTex = InRenderer->GetMaterial()->GetTex(UHMaterialTextureType::Opacity))
	{
		BindImage(OpacityTex, 4);
	}

	if (UHSampler* OpacitySampler = InRenderer->GetMaterial()->GetSampler(UHMaterialTextureType::Opacity))
	{
		BindSampler(OpacitySampler, 5);
	}

	BindStorage(InRenderer->GetMesh()->GetUV0Buffer(), 6, 0, true);
}