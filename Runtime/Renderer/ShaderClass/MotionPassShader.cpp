#include "MotionPassShader.h"
#include "../../Components/MeshRenderer.h"
#include "../RendererShared.h"

UHMotionCameraPassShader::UHMotionCameraPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHMotionCameraPassShader), nullptr, InRenderPass)
{
	// system const + depth texture + sampler
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateDescriptor();
	OnCompile();
}

void UHMotionCameraPassShader::OnCompile()
{
	ShaderVS = Gfx->RequestShader("PostProcessVS", "Shaders/PostProcessing/PostProcessVS.hlsl", "PostProcessVS", "vs_6_0");
	ShaderPS = Gfx->RequestShader("MotionCameraShader", "Shaders/MotionCameraShader.hlsl", "CameraMotionPS", "ps_6_0");

	// states
	UHRenderPassInfo Info = UHRenderPassInfo(RenderPassCache, UHDepthInfo(false, false, VK_COMPARE_OP_ALWAYS)
		, UHCullMode::CullNone
		, UHBlendMode::Opaque
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	CreateGraphicState(Info);
}

void UHMotionCameraPassShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0);
	BindImage(GSceneDepth, 1);
	BindSampler(GPointClampedSampler, 2);
}

UHMotionObjectPassShader::UHMotionObjectPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHMotionObjectPassShader), InMat, InRenderPass)
{
	// Motion pass: constants + opacity image for cutoff (if there is any)
	for (uint32_t Idx = 0; Idx < UH_ENUM_VALUE(UHConstantTypes::ConstantTypeMax); Idx++)
	{
		if (Idx != UH_ENUM_VALUE(UHConstantTypes::Material))
		{
			AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		}
		else
		{
			AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		}
	}

	// UV0/normal/tangent Buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	CreateMaterialDescriptor(ExtraLayouts);
	OnCompile();
}

void UHMotionObjectPassShader::OnCompile()
{
	const bool bIsTranslucent = MaterialCache->GetBlendMode() > UHBlendMode::Masked;
	const VkCompareOp DepthCompareOp = (Gfx->IsDepthPrePassEnabled() && !bIsTranslucent) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER_OR_EQUAL;

	// early out if cached and depth compare op didn't change
	if (GetState() != nullptr && GetState()->GetRenderPassInfo().DepthInfo.DepthFunc == DepthCompareOp)
	{
		// restore cached value
		const UHRenderPassInfo& PassInfo = GetState()->GetRenderPassInfo();
		ShaderVS = PassInfo.VS;
		ShaderPS = PassInfo.PS;
		MaterialPassInfo = PassInfo;
		return;
	}

	ShaderVS = Gfx->RequestShader("MotionVertexShader", "Shaders/MotionVertexShader.hlsl", "MotionObjectVS", "vs_6_0");

	UHMaterialCompileData Data;
	Data.MaterialCache = MaterialCache;
	ShaderPS = Gfx->RequestMaterialShader("MotionPixelShader", "Shaders/MotionPixelShader.hlsl", "MotionObjectPS", "ps_6_0", Data);

	// states, enable depth test, and write depth for translucent object only
	MaterialPassInfo = UHRenderPassInfo(RenderPassCache,
		UHDepthInfo(true, bIsTranslucent, DepthCompareOp)
		, MaterialCache->GetCullMode()
		, MaterialCache->GetBlendMode()
		, ShaderVS
		, ShaderPS
		, bIsTranslucent ? GNumOfGBuffersTrans : 1
		, PipelineLayout);

	// disable blending intentionally if it's translucent
	if (bIsTranslucent)
	{
		MaterialPassInfo.bForceBlendOff = true;
	}

	RecreateMaterialState();
}

void UHMotionObjectPassShader::BindParameters(const UHMeshRendererComponent* InRenderer)
{
	BindConstant(GSystemConstantBuffer, 0);
	BindConstant(GObjectConstantBuffer, 1, InRenderer->GetBufferDataIndex());
	BindConstant(MaterialCache->GetMaterialConst(), 2);

	BindStorage(InRenderer->GetMesh()->GetUV0Buffer(), 3, 0, true);
	BindStorage(InRenderer->GetMesh()->GetNormalBuffer(), 4, 0, true);
	BindStorage(InRenderer->GetMesh()->GetTangentBuffer(), 5, 0, true);
}