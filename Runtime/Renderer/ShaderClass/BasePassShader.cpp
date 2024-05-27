#include "BasePassShader.h"
#include "../../Components/MeshRenderer.h"
#include "../RendererShared.h"

UHGraphicState* UHBasePassShader::OcclusionState = nullptr;

UHBasePassShader::UHBasePassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts
	, bool bInOcclusionTest)
	: UHShaderClass(InGfx, Name, typeid(UHBasePassShader), InMat, InRenderPass)
{
	// DeferredPass: Bind all constants, visiable in VS/PS only
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

	// Bind envcube and sampler
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);

	// textures and samplers will be bound on fly instead, since I go with bindless rendering
	bOcclusionTest = bInOcclusionTest;
	CreateMaterialDescriptor(ExtraLayouts);
	OnCompile();
}

void UHBasePassShader::OnCompile()
{
	// early out if cached and depth write state didn't change
	if (GetState() != nullptr && GetState()->GetRenderPassInfo().DepthInfo.bEnableDepthWrite == !Gfx->IsDepthPrePassEnabled())
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
	ShaderPS = Gfx->RequestMaterialShader("BasePixelShader", "Shaders/BasePixelShader.hlsl", "BasePS", "ps_6_0", Data);
	
	// states
	MaterialPassInfo = UHRenderPassInfo(RenderPassCache
		// adjust depth info based on depth pre pass setting
		, UHDepthInfo(true, !Gfx->IsDepthPrePassEnabled(), (Gfx->IsDepthPrePassEnabled()) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER)
		, MaterialCache->GetCullMode()
		, MaterialCache->GetBlendMode()
		, ShaderVS
		, ShaderPS
		, GNumOfGBuffers
		, PipelineLayout);

	RecreateMaterialState();
}

void UHBasePassShader::BindParameters(const UHMeshRendererComponent* InRenderer)
{
	BindConstant(GSystemConstantBuffer, 0);
	BindConstant(bOcclusionTest ? GOcclusionConstantBuffer : GObjectConstantBuffer, 1, InRenderer->GetBufferDataIndex());
	BindConstant(MaterialCache->GetMaterialConst(), 2);

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 3, 0, true);
	BindStorage(Mesh->GetNormalBuffer(), 4, 0, true);
	BindStorage(Mesh->GetTangentBuffer(), 5, 0, true);

	BindSkyCube();
}

void UHBasePassShader::BindSkyCube()
{
	BindImage(GSkyLightCube, 6);
}

void UHBasePassShader::RecreateOcclusionState()
{
	Gfx->RequestReleaseGraphicState(OcclusionState);
	OcclusionState = nullptr;

	UHRenderPassInfo PassInfo = MaterialPassInfo;
	PassInfo.RenderPass = RenderPassCache;
	PassInfo.DepthInfo.bEnableDepthWrite = false;
	PassInfo.DepthInfo.DepthFunc = VK_COMPARE_OP_GREATER_OR_EQUAL;
	PassInfo.bEnableColorWrite = false;
	PassInfo.BlendMode = UHBlendMode::Opaque;
	PassInfo.CullMode = UHCullMode::CullNone;
	PassInfo.PS = UHINDEXNONE;
	OcclusionState = Gfx->RequestGraphicState(PassInfo);
}

UHGraphicState* UHBasePassShader::GetOcclusionState() const
{
	return OcclusionState;
}