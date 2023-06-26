#include "MotionPassShader.h"
#include "../../Components/MeshRenderer.h"

UHMotionCameraPassShader::UHMotionCameraPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHMotionCameraPassShader), nullptr)
{
	// system const + depth texture + sampler
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateDescriptor();
	ShaderVS = InGfx->RequestShader("PostProcessVS", "Shaders/PostProcessing/PostProcessVS.hlsl", "PostProcessVS", "vs_6_0");
	ShaderPS = InGfx->RequestShader("MotionCameraShader", "Shaders/MotionCameraShader.hlsl", "CameraMotionPS", "ps_6_0");

	// states
	UHRenderPassInfo Info = UHRenderPassInfo(InRenderPass, UHDepthInfo(false, false, VK_COMPARE_OP_ALWAYS)
		, VK_CULL_MODE_NONE
		, UHBlendMode::Opaque
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	CreateGraphicState(Info);
}

void UHMotionCameraPassShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const UHRenderTexture* DepthTexture
	, const UHSampler* PointClamppedSampler)
{
	BindConstant(SysConst, 0);
	BindImage(DepthTexture, 1);
	BindSampler(PointClamppedSampler, 2);
}

UHMotionObjectPassShader::UHMotionObjectPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bEnableDepthPrePass
	, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHMotionObjectPassShader), InMat)
{
	// Motion pass: constants + opacity image for cutoff (if there is any)
	for (uint32_t Idx = 0; Idx < UHConstantTypes::ConstantTypeMax; Idx++)
	{
		AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}

	// occlusion buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// UV0 Buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	CreateMaterialDescriptor(ExtraLayouts);

	// check occlusion test define
	std::vector<std::string> OcclusionTestDefine;
	if (InGfx->IsRayTracingOcclusionTestEnabled())
	{
		OcclusionTestDefine.push_back("WITH_OCCLUSION_TEST");
	}
	ShaderVS = InGfx->RequestShader("MotionVertexShader", "Shaders/MotionVertexShader.hlsl", "MotionObjectVS", "vs_6_0", OcclusionTestDefine);

	// find opacity define
	std::vector<std::string> Defines = InMat->GetMaterialDefines(PS);
	std::vector<std::string> ActualDefines;

	if (bEnableDepthPrePass)
	{
		ActualDefines.push_back("WITH_DEPTHPREPASS");
	}

	UHMaterialCompileData Data;
	Data.MaterialCache = InMat;
	Data.bIsDepthOrMotionPass = true;
	ShaderPS = InGfx->RequestMaterialShader("MotionPixelShader", "Shaders/MotionPixelShader.hlsl", "MotionObjectPS", "ps_6_0", Data, ActualDefines);

	// states, enable depth test but don't write it
	UHRenderPassInfo Info = UHRenderPassInfo(InRenderPass, 
		UHDepthInfo(true, false, (bEnableDepthPrePass) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER_OR_EQUAL)
		, InMat->GetCullMode()
		, InMat->GetBlendMode()
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	CreateMaterialState(Info);
}

void UHMotionObjectPassShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const std::array<std::unique_ptr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
	, const std::unique_ptr<UHRenderBuffer<uint32_t>>& OcclusionConst
	, const UHMeshRendererComponent* InRenderer)
{
	BindConstant(SysConst, 0);
	BindConstant(ObjConst, 1, InRenderer->GetBufferDataIndex());
	BindConstant(MaterialCache->GetMaterialConst(), 2);

	if (Gfx->IsRayTracingOcclusionTestEnabled())
	{
		BindStorage(OcclusionConst.get(), 3, 0, true);
	}

	BindStorage(InRenderer->GetMesh()->GetUV0Buffer(), 4, 0, true);
}