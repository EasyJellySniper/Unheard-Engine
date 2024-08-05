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

	// textures and samplers will be bound on fly instead, since I go with bindless rendering
	bOcclusionTest = bInOcclusionTest;
	CreateLayoutAndDescriptor(ExtraLayouts);
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

	std::vector<std::string> Defines;
	if (MaterialCache->GetMaterialUsages().bIsTangentSpace)
	{
		Defines.push_back("TANGENT_SPACE");
	}

	ShaderVS = Gfx->RequestShader("BaseVertexShader", "Shaders/BaseVertexShader.hlsl", "BaseVS", "vs_6_0", Defines);
	UHMaterialCompileData Data{};
	Data.MaterialCache = MaterialCache;
	ShaderPS = Gfx->RequestMaterialShader("BasePixelShader", "Shaders/BasePixelShader.hlsl", "BasePS", "ps_6_0", Data, Defines);
	
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

UHBaseMeshShader::UHBaseMeshShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHBaseMeshShader), InMat, InRenderPass)
{
	// system
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_MESH_BIT_EXT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	// object constant
	AddLayoutBinding(1, VK_SHADER_STAGE_MESH_BIT_EXT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// material
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	// current mesh shader data and renderer instances
	AddLayoutBinding(1, VK_SHADER_STAGE_MESH_BIT_EXT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_MESH_BIT_EXT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	CreateLayoutAndDescriptor(ExtraLayouts);

	OnCompile();
}

void UHBaseMeshShader::OnCompile()
{
	// early out if cached
	if (GetState() != nullptr && GetState()->GetRenderPassInfo().DepthInfo.bEnableDepthWrite == !Gfx->IsDepthPrePassEnabled())
	{
		// restore cached value
		const UHRenderPassInfo& PassInfo = GetState()->GetRenderPassInfo();
		ShaderMS = PassInfo.MS;
		ShaderPS = PassInfo.PS;
		MaterialPassInfo = PassInfo;
		return;
	}

	std::vector<std::string> Defines;
	if (MaterialCache->GetMaterialUsages().bIsTangentSpace)
	{
		Defines.push_back("TANGENT_SPACE");
	}

	ShaderMS = Gfx->RequestShader("BaseMeshShader", "Shaders/BaseMeshShader.hlsl", "BaseMS", "ms_6_5", Defines);
	UHMaterialCompileData Data{};
	Data.MaterialCache = MaterialCache;
	ShaderPS = Gfx->RequestMaterialShader("BasePixelShader", "Shaders/BasePixelShader.hlsl", "BasePS", "ps_6_0", Data, Defines);

	// states
	MaterialPassInfo = UHRenderPassInfo(RenderPassCache
		// adjust depth info based on depth pre pass setting
		, UHDepthInfo(true, !Gfx->IsDepthPrePassEnabled(), (Gfx->IsDepthPrePassEnabled()) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER)
		, MaterialCache->GetCullMode()
		, MaterialCache->GetBlendMode()
		, UHINDEXNONE
		, ShaderPS
		, GNumOfGBuffers
		, PipelineLayout);
	MaterialPassInfo.MS = ShaderMS;

	RecreateMaterialState();
}

void UHBaseMeshShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0);
	BindStorage(GObjectConstantBuffer, 1, 0, true);
	BindConstant(MaterialCache->GetMaterialConst(), 2);

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		BindStorage(GMeshShaderData[Idx][MaterialCache->GetBufferDataIndex()].get(), 3, 0, true, Idx);
	}

	BindStorage(GRendererInstanceBuffer.get(), 4, 0, true);
}