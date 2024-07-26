#include "DepthPassShader.h"
#include "../../Components/MeshRenderer.h"
#include "../RendererShared.h"

// -------------------------------------------------------------- UHDepthPassShader
UHDepthPassShader::UHDepthPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHDepthPassShader), InMat, InRenderPass)
{
	// Depth pass: Bind all constants, visiable in VS/PS only
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

	// bind UV0 Buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// textures and samplers will be bound on fly instead, since I go with bindless rendering
	CreateLayoutAndDescriptor(ExtraLayouts);

	OnCompile();
}

void UHDepthPassShader::OnCompile()
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

	if (MaterialCache->GetBlendMode() == UHBlendMode::Masked)
	{
		UHMaterialCompileData Data;
		Data.MaterialCache = MaterialCache;
		ShaderPS = Gfx->RequestMaterialShader("DepthPassPS", "Shaders/DepthPixelShader.hlsl", "DepthPS", "ps_6_0", Data);
	}

	ShaderVS = Gfx->RequestShader("DepthPassVS", "Shaders/DepthVertexShader.hlsl", "DepthVS", "vs_6_0");

	// states
	MaterialPassInfo = UHRenderPassInfo(RenderPassCache, UHDepthInfo(true, true, VK_COMPARE_OP_GREATER)
		, MaterialCache->GetCullMode()
		, MaterialCache->GetBlendMode()
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	RecreateMaterialState();
}

void UHDepthPassShader::BindParameters(const UHMeshRendererComponent* InRenderer)
{
	BindConstant(GSystemConstantBuffer, 0);
	BindConstant(GObjectConstantBuffer, 1, InRenderer->GetBufferDataIndex());
	BindConstant(MaterialCache->GetMaterialConst(), 2);

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 3, 0, true);
}

// -------------------------------------------------------------- UHDepthMeshShader
UHDepthMeshShader::UHDepthMeshShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHDepthMeshShader), InMat, InRenderPass)
{
	// system
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	// object constant
	AddLayoutBinding(1, VK_SHADER_STAGE_MESH_BIT_EXT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// material
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	// renderer instances
	AddLayoutBinding(1, VK_SHADER_STAGE_TASK_BIT_EXT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// amplification data
	AddLayoutBinding(1, VK_SHADER_STAGE_TASK_BIT_EXT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	CreateLayoutAndDescriptor(ExtraLayouts);

	OnCompile();
}

void UHDepthMeshShader::OnCompile()
{
	// early out if cached
	if (GetState() != nullptr)
	{
		// restore cached value
		const UHRenderPassInfo& PassInfo = GetState()->GetRenderPassInfo();
		ShaderAS = PassInfo.AS;
		ShaderMS = PassInfo.MS;
		ShaderPS = PassInfo.PS;
		MaterialPassInfo = PassInfo;
		return;
	}

	if (MaterialCache->GetBlendMode() == UHBlendMode::Masked)
	{
		UHMaterialCompileData Data;
		Data.MaterialCache = MaterialCache;
		ShaderPS = Gfx->RequestMaterialShader("DepthPassPS", "Shaders/DepthPixelShader.hlsl", "DepthPS", "ps_6_0", Data);
	}

	ShaderAS = Gfx->RequestShader("BaseAmplificationShader", "Shaders/BaseAmplificationShader.hlsl", "MainAS", "as_6_5");
	ShaderMS = Gfx->RequestShader("DepthMeshShader", "Shaders/DepthMeshShader.hlsl", "DepthMS", "ms_6_5");

	// states
	MaterialPassInfo = UHRenderPassInfo(RenderPassCache, UHDepthInfo(true, true, VK_COMPARE_OP_GREATER)
		, MaterialCache->GetCullMode()
		, MaterialCache->GetBlendMode()
		, UHINDEXNONE
		, ShaderPS
		, 1
		, PipelineLayout);
	MaterialPassInfo.AS = ShaderAS;
	MaterialPassInfo.MS = ShaderMS;

	RecreateMaterialState();
}

void UHDepthMeshShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0);
	BindStorage(GObjectConstantBuffer, 1, 0, true);
	BindConstant(MaterialCache->GetMaterialConst(), 2);

	for (uint32_t FrameIdx = 0; FrameIdx < GMaxFrameInFlight; FrameIdx++)
	{
		BindStorage(GMeshShaderInstances[FrameIdx][MaterialCache->GetBufferDataIndex()].get(), 3, 0, true, FrameIdx);
	}
	BindStorage(GAmplificationParameters[MaterialCache->GetBufferDataIndex()].get(), 4);
}