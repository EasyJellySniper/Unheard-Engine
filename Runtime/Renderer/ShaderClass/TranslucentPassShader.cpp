#include "TranslucentPassShader.h"
#include "../../Components/MeshRenderer.h"

UHTranslucentPassShader::UHTranslucentPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bHasEnvCube
	, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHTranslucentPassShader), InMat)
{
	// sys, obj, mat consts
	for (int32_t Idx = 0; Idx < UHConstantTypes::ConstantTypeMax; Idx++)
	{
		AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}

	// bind UV0/Normal/Tangent buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// light consts (dir + point + spot)
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// RT shadow result, point & spot light culling buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	// Bind envcube and sampler
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	// textures and samplers will be bound on fly instead, since I go with bindless rendering
	CreateMaterialDescriptor(ExtraLayouts);

	// define macros
	std::vector<std::string> VSDefines = InMat->GetMaterialDefines();
	if (bHasEnvCube)
	{
		VSDefines.push_back("WITH_ENVCUBE");
	}

	std::vector<std::string> PSDefines = VSDefines;
	if (InGfx->IsRayTracingEnabled())
	{
		PSDefines.push_back("WITH_RTSHADOWS");
	}

	ShaderVS = InGfx->RequestShader("BaseVertexShader", "Shaders/BaseVertexShader.hlsl", "BaseVS", "vs_6_0", VSDefines);
	UHMaterialCompileData Data{};
	Data.MaterialCache = InMat;
	ShaderPS = InGfx->RequestMaterialShader("TranslucentPixelShader", "Shaders/TranslucentPixelShader.hlsl", "TranslucentPS", "ps_6_0", Data, PSDefines);

	// states
	MaterialPassInfo = UHRenderPassInfo(InRenderPass
		// translucent doesn't output depth
		, UHDepthInfo(true, false, VK_COMPARE_OP_GREATER_OR_EQUAL)
		, InMat->GetCullMode()
		, InMat->GetBlendMode()
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	CreateMaterialState(MaterialPassInfo);
}

void UHTranslucentPassShader::BindParameters(const std::array<UniquePtr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const std::array<UniquePtr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
	, const std::array<UniquePtr<UHRenderBuffer<UHDirectionalLightConstants>>, GMaxFrameInFlight>& DirLightConst
	, const std::array<UniquePtr<UHRenderBuffer<UHPointLightConstants>>, GMaxFrameInFlight>& PointLightConst
	, const std::array<UniquePtr<UHRenderBuffer<UHSpotLightConstants>>, GMaxFrameInFlight>& SpotLightConst
	, const UniquePtr<UHRenderBuffer<uint32_t>>& PointLightList
	, const UniquePtr<UHRenderBuffer<uint32_t>>& SpotLightList
	, const UHRenderTexture* RTShadowResult
	, const UHSampler* LinearClamppedSampler
	, const UHMeshRendererComponent* InRenderer
	, const int32_t RTInstanceCount
	, const UHTextureCube* EnvCube
	, const UHSampler* EnvSampler
)
{
	BindConstant(SysConst, 0);
	BindConstant(ObjConst, 1, InRenderer->GetBufferDataIndex());
	BindConstant(MaterialCache->GetMaterialConst(), 2);

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 3, 0, true);
	BindStorage(Mesh->GetNormalBuffer(), 4, 0, true);
	BindStorage(Mesh->GetTangentBuffer(), 5, 0, true);

	// bind light const
	BindStorage(DirLightConst, 6, 0, true);
	BindStorage(PointLightConst, 7, 0, true);
	BindStorage(SpotLightConst, 8, 0, true);

	if (Gfx->IsRayTracingEnabled() && RTInstanceCount > 0)
	{
		BindImage(RTShadowResult, 9);
	}
	BindStorage(PointLightList.get(), 10, 0, true);
	BindStorage(SpotLightList.get(), 11, 0, true);
	BindSampler(LinearClamppedSampler, 12);

	// write textures/samplers when they are available
	BindImage(EnvCube, 13);
	BindSampler(EnvSampler, 14);
}