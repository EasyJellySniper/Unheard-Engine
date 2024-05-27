#include "TranslucentPassShader.h"
#include "../../Components/MeshRenderer.h"
#include "../RendererShared.h"

UHGraphicState* UHTranslucentPassShader::OcclusionState = nullptr;

UHTranslucentPassShader::UHTranslucentPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat
	, const std::vector<VkDescriptorSetLayout>& ExtraLayouts, bool bInOcclusionTest)
	: UHShaderClass(InGfx, Name, typeid(UHTranslucentPassShader), InMat, InRenderPass)
{
	// sys, obj, mat consts
	for (int32_t Idx = 0; Idx < UH_ENUM_VALUE(UHConstantTypes::ConstantTypeMax); Idx++)
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

	// bind UV0/Normal/Tangent buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// light consts (dir + point + spot)
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// RT shadow/reflection result, point & spot light culling buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// Bind envcube and sampler
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// textures and samplers will be bound on fly instead, since I go with bindless rendering
	bOcclusionTest = bInOcclusionTest;
	CreateMaterialDescriptor(ExtraLayouts);
	OnCompile();
}

void UHTranslucentPassShader::OnCompile()
{
	// early out if cached
	if (GetState() != nullptr)
	{
		// restore cached value
		const UHRenderPassInfo& PassInfo = GetState()->GetRenderPassInfo();
		ShaderVS = PassInfo.VS;
		ShaderPS = PassInfo.PS;
		MaterialPassInfo = PassInfo;
		return;
	}

	ShaderVS = Gfx->RequestShader("BaseVertexShader", "Shaders/BaseVertexShader.hlsl", "BaseVS", "vs_6_0");
	UHMaterialCompileData Data{};
	Data.MaterialCache = MaterialCache;
	ShaderPS = Gfx->RequestMaterialShader("TranslucentPixelShader", "Shaders/TranslucentPixelShader.hlsl", "TranslucentPS", "ps_6_0", Data);

	// states
	MaterialPassInfo = UHRenderPassInfo(RenderPassCache
		// translucent doesn't output depth
		, UHDepthInfo(true, false, VK_COMPARE_OP_GREATER_OR_EQUAL)
		, MaterialCache->GetCullMode()
		, MaterialCache->GetBlendMode()
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	if (OcclusionState == nullptr)
	{
		UHRenderPassInfo PassInfo = MaterialPassInfo;
		PassInfo.DepthInfo.bEnableDepthWrite = false;
		PassInfo.DepthInfo.DepthFunc = VK_COMPARE_OP_GREATER_OR_EQUAL;
		PassInfo.bEnableColorWrite = false;
		PassInfo.BlendMode = UHBlendMode::Opaque;
		PassInfo.CullMode = UHCullMode::CullNone;
		PassInfo.PS = UHINDEXNONE;
		OcclusionState = Gfx->RequestGraphicState(PassInfo);
	}

	RecreateMaterialState();
}

void UHTranslucentPassShader::BindParameters(const UHMeshRendererComponent* InRenderer, const bool bIsRaytracingEnableRT)
{
	BindConstant(GSystemConstantBuffer, 0);
	BindConstant(bOcclusionTest ? GOcclusionConstantBuffer : GObjectConstantBuffer, 1, InRenderer->GetBufferDataIndex());
	BindConstant(MaterialCache->GetMaterialConst(), 2);

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 3, 0, true);
	BindStorage(Mesh->GetNormalBuffer(), 4, 0, true);
	BindStorage(Mesh->GetTangentBuffer(), 5, 0, true);

	// bind light const
	BindStorage(GDirectionalLightBuffer, 6, 0, true);
	BindStorage(GPointLightBuffer, 7, 0, true);
	BindStorage(GSpotLightBuffer, 8, 0, true);

	if (bIsRaytracingEnableRT)
	{
		BindImage(GRTShadowResult, 9);
		BindImage(GRTReflectionResult, 10);
	}
	else
	{
		BindImage(GWhiteTexture, 9);
		BindImage(GBlackTexture, 10);
	}

	BindStorage(GPointLightListTransBuffer.get(), 11, 0, true);
	BindStorage(GSpotLightListTransBuffer.get(), 12, 0, true);
	BindStorage(GSH9Data.get(), 13, 0, true);
	BindSkyCube();
}

void UHTranslucentPassShader::BindSkyCube()
{
	BindImage(GSkyLightCube, 14);
}

void UHTranslucentPassShader::RecreateOcclusionState()
{
	Gfx->RequestReleaseGraphicState(OcclusionState);
	OcclusionState = nullptr;

	UHRenderPassInfo PassInfo = MaterialPassInfo;
	PassInfo.DepthInfo.bEnableDepthWrite = false;
	PassInfo.DepthInfo.DepthFunc = VK_COMPARE_OP_GREATER_OR_EQUAL;
	PassInfo.bEnableColorWrite = false;
	PassInfo.BlendMode = UHBlendMode::Opaque;
	PassInfo.CullMode = UHCullMode::CullNone;
	PassInfo.PS = UHINDEXNONE;
	OcclusionState = Gfx->RequestGraphicState(PassInfo);
}

UHGraphicState* UHTranslucentPassShader::GetOcclusionState() const
{
	return OcclusionState;
}