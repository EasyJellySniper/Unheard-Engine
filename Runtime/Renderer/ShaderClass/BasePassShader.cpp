#include "BasePassShader.h"
#include "../../Components/MeshRenderer.h"

UHBasePassShader::UHBasePassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bEnableDepthPrePass)
	: UHShaderClass(InGfx, Name, typeid(UHBasePassShader))
{
	MaterialCache = InMat;

	// DeferredPass: Bind all constants, visiable in VS/PS only
	// use storage buffer on materials
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// bind occlusion visible buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// bind UV0/Normal/Tangent buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// Bind envcube and sampler
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	// bind textures from material
	int32_t TexSlot = GMaterialTextureRegisterStart;
	for (const std::string RegisteredTexture : InMat->GetRegisteredTextureNames())
	{
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	}
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	CreateMaterialDescriptor();

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
	ShaderPS = InGfx->RequestMaterialPixelShader("BasePixelShader", "Shaders/BasePixelShader.hlsl", "BasePS", "ps_6_0", InMat, PSDefines);

	// prevent duplicating
	if (GGraphicStateTable.find(InMat->GetId()) == GGraphicStateTable.end())
	{
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

		GGraphicStateTable[InMat->GetId()] = InGfx->RequestGraphicState(Info);
	}
}

void UHBasePassShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const std::array<std::unique_ptr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
	, const std::array<std::unique_ptr<UHRenderBuffer<UHMaterialConstants>>, GMaxFrameInFlight>& MatConst
	, const std::array<std::unique_ptr<UHRenderBuffer<uint32_t>>, GMaxFrameInFlight>& OcclusionConst
	, const UHMeshRendererComponent* InRenderer
	, const UHAssetManager* InAssetMgr
	, const UHSampler* InDefaultSampler)
{
	BindConstant(SysConst, 0);
	BindConstant(ObjConst, 1, InRenderer->GetBufferDataIndex());
	BindStorage(MatConst, 2, InRenderer->GetMaterial()->GetBufferDataIndex());

	if (Gfx->IsRayTracingOcclusionTestEnabled())
	{
		BindStorage(OcclusionConst, 3, 0, true);
	}

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 4, 0, true);
	BindStorage(Mesh->GetNormalBuffer(), 5, 0, true);
	BindStorage(Mesh->GetTangentBuffer(), 6, 0, true);

	// write textures/samplers when they are available
	UHTexture* Tex = InRenderer->GetMaterial()->GetTex(UHMaterialTextureType::SkyCube);
	if (Tex)
	{
		BindImage(Tex, 7);
	}

	UHSampler* Sampler = InRenderer->GetMaterial()->GetSampler(UHMaterialTextureType::SkyCube);
	if (Sampler)
	{
		BindSampler(Sampler, 8);
	}

	// bind textures from material
	int32_t TexSlot = GMaterialTextureRegisterStart;
	for (const std::string RegisteredTexture : InRenderer->GetMaterial()->GetRegisteredTextureNames())
	{
		BindImage(InAssetMgr->GetTexture2D(RegisteredTexture), TexSlot);
		TexSlot++;
	}

	BindSampler(InDefaultSampler, TexSlot);
}