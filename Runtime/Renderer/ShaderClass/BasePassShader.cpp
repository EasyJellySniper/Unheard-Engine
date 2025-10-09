#include "BasePassShader.h"
#include "../../Components/MeshRenderer.h"
#include "../RendererShared.h"

UHBasePassShader::UHBasePassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
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

	ShaderVS = Gfx->RequestShader("BaseVertexShader", "Shaders/BaseVertexShader.hlsl", "BaseVS", "vs_6_0", MaterialCache->GetShaderDefines());
	UHMaterialCompileData Data{};
	Data.MaterialCache = MaterialCache;
	ShaderPS = Gfx->RequestMaterialShader("BasePixelShader", "Shaders/BasePixelShader.hlsl", "BasePS", "ps_6_0", Data, MaterialCache->GetShaderDefines());
	
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

	MaterialPassInfo.bIsIntegerBuffer = { false,false,false,false,false,true };

	RecreateMaterialState();
}

void UHBasePassShader::BindParameters(const UHMeshRendererComponent* InRenderer)
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindConstant(GObjectConstantBuffer, 1, InRenderer->GetBufferDataIndex());
	BindConstant(MaterialCache->GetMaterialConst(), 2, 0);

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 3, 0, true);
	BindStorage(Mesh->GetNormalBuffer(), 4, 0, true);
	BindStorage(Mesh->GetTangentBuffer(), 5, 0, true);
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

	ShaderMS = Gfx->RequestShader("BaseMeshShader", "Shaders/BaseMeshShader.hlsl", "BaseMS", "ms_6_5", MaterialCache->GetShaderDefines());
	UHMaterialCompileData Data{};
	Data.MaterialCache = MaterialCache;
	ShaderPS = Gfx->RequestMaterialShader("BasePixelShader", "Shaders/BasePixelShader.hlsl", "BasePS", "ps_6_0", Data, MaterialCache->GetShaderDefines());

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
	MaterialPassInfo.bIsIntegerBuffer = { false,false,false,false,false,true };

	RecreateMaterialState();
}

void UHBaseMeshShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindStorage(GObjectConstantBuffer, 1, 0, true);
	BindConstant(MaterialCache->GetMaterialConst(), 2, 0);

	for (uint32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		BindStorage(GMeshShaderData[Idx][MaterialCache->GetBufferDataIndex()].get(), 3, 0, true, Idx);

		// use the occlusion result from the previous frame
		const uint32_t PrevFrame = (Idx - 1) % GMaxFrameInFlight;
		if (GOcclusionResult[PrevFrame] != nullptr)
		{
			BindStorage(GOcclusionResult[PrevFrame].get(), 4, 0, true);
		}
	}

	BindStorage(GRendererInstanceBuffer.get(), 5, 0, true);
}