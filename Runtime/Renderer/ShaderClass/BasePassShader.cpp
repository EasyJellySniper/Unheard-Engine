#include "BasePassShader.h"
#include "../../Components/MeshRenderer.h"
#include "../RendererShared.h"

UHBasePassShader::UHBasePassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bEnableDepthPrePass, bool bHasEnvCube
	, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHBasePassShader), InMat, InRenderPass)
	, bHasDepthPrePass(bEnableDepthPrePass)
	, bHasEnvCubemap(bHasEnvCube)
{
	// DeferredPass: Bind all constants, visiable in VS/PS only
	for (int32_t Idx = 0; Idx < UHConstantTypes::ConstantTypeMax; Idx++)
	{
		AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}

	// bind UV0/Normal/Tangent buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// Bind envcube and sampler
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	// textures and samplers will be bound on fly instead, since I go with bindless rendering
	CreateMaterialDescriptor(ExtraLayouts);
	OnCompile();
}

void UHBasePassShader::OnCompile()
{
	// also check prepass define
	std::vector<std::string> MatDefines = MaterialCache->GetMaterialDefines();
	if (bHasDepthPrePass)
	{
		MatDefines.push_back("WITH_DEPTHPREPASS");
	}

	if (bHasEnvCubemap)
	{
		MatDefines.push_back("WITH_ENVCUBE");
	}

	ShaderVS = Gfx->RequestShader("BaseVertexShader", "Shaders/BaseVertexShader.hlsl", "BaseVS", "vs_6_0", MatDefines);
	UHMaterialCompileData Data{};
	Data.MaterialCache = MaterialCache;
	ShaderPS = Gfx->RequestMaterialShader("BasePixelShader", "Shaders/BasePixelShader.hlsl", "BasePS", "ps_6_0", Data, MatDefines);

	// states
	MaterialPassInfo = UHRenderPassInfo(RenderPassCache
		// adjust depth info based on depth pre pass setting
		, UHDepthInfo(true, !bHasDepthPrePass, (bHasDepthPrePass) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER)
		, MaterialCache->GetCullMode()
		, MaterialCache->GetBlendMode()
		, ShaderVS
		, ShaderPS
		, GNumOfGBuffers
		, PipelineLayout);

	CreateMaterialState(MaterialPassInfo);
}

void UHBasePassShader::BindParameters(const UHMeshRendererComponent* InRenderer)
{
	BindConstant(GSystemConstantBuffer, 0);
	BindConstant(GObjectConstantBuffer, 1, InRenderer->GetBufferDataIndex());
	BindConstant(MaterialCache->GetMaterialConst(), 2);

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 3, 0, true);
	BindStorage(Mesh->GetNormalBuffer(), 4, 0, true);
	BindStorage(Mesh->GetTangentBuffer(), 5, 0, true);

	// write textures/samplers when they are available
	BindImage(GSkyLightCube, 6);
	BindSampler(GSkyCubeSampler, 7);
}