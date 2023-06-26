#include "BasePassShader.h"
#include "../../Components/MeshRenderer.h"

UHBasePassShader::UHBasePassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bEnableDepthPrePass
	, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHBasePassShader), InMat)
{
	// DeferredPass: Bind all constants, visiable in VS/PS only
	for (int32_t Idx = 0; Idx < UHConstantTypes::ConstantTypeMax; Idx++)
	{
		AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}

	// bind occlusion visible buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// bind UV0/Normal/Tangent buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// Bind envcube and sampler
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	// textures and samplers will be bound on fly instead, since I go with bindless rendering
	CreateMaterialDescriptor(ExtraLayouts);

	// also check occlusion define for VS
	std::vector<std::string> VSDefines = InMat->GetMaterialDefines(VS);
	if (InGfx->IsRayTracingOcclusionTestEnabled())
	{
		VSDefines.push_back("WITH_OCCLUSION_TEST");
	}
	std::vector<std::string> PSDefines = InMat->GetMaterialDefines(PS);
	if (bEnableDepthPrePass)
	{
		PSDefines.push_back("WITH_DEPTHPREPASS");
	}

	ShaderVS = InGfx->RequestShader("BaseVertexShader", "Shaders/BaseVertexShader.hlsl", "BaseVS", "vs_6_0", VSDefines);

	UHMaterialCompileData Data{};
	Data.MaterialCache = InMat;
	ShaderPS = InGfx->RequestMaterialShader("BasePixelShader", "Shaders/BasePixelShader.hlsl", "BasePS", "ps_6_0", Data, PSDefines);

	// states
	UHRenderPassInfo Info = UHRenderPassInfo(InRenderPass
		// adjust depth info based on depth pre pass setting
		, UHDepthInfo(true, !bEnableDepthPrePass, (bEnableDepthPrePass) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER)
		, InMat->GetCullMode()
		, InMat->GetBlendMode()
		, ShaderVS
		, ShaderPS
		, GNumOfGBuffers
		, PipelineLayout);

	CreateMaterialState(Info);
}

void UHBasePassShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
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

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 4, 0, true);
	BindStorage(Mesh->GetNormalBuffer(), 5, 0, true);
	BindStorage(Mesh->GetTangentBuffer(), 6, 0, true);

	// write textures/samplers when they are available
	UHTexture* Tex = InRenderer->GetMaterial()->GetSystemTex(UHSystemTextureType::SkyCube);
	if (Tex)
	{
		BindImage(Tex, 7);
	}

	UHSampler* Sampler = InRenderer->GetMaterial()->GetSystemSampler(UHSystemTextureType::SkyCube);
	if (Sampler)
	{
		BindSampler(Sampler, 8);
	}
}