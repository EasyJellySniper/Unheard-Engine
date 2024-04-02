#include "TranslucentPassShader.h"
#include "../../Components/MeshRenderer.h"
#include "../RendererShared.h"

UHTranslucentPassShader::UHTranslucentPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat
	, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHTranslucentPassShader), InMat, InRenderPass)
{
	// sys, obj, mat consts
	for (int32_t Idx = 0; Idx < UHConstantTypes::ConstantTypeMax; Idx++)
	{
		if (Idx != UHConstantTypes::Material)
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
	CreateMaterialDescriptor(ExtraLayouts);
	OnCompile();
}

void UHTranslucentPassShader::OnCompile()
{
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

	RecreateMaterialState();
}

void UHTranslucentPassShader::BindParameters(const UHMeshRendererComponent* InRenderer)
{
	BindConstant(GSystemConstantBuffer, 0);
	BindConstant(GObjectConstantBuffer, 1, InRenderer->GetBufferDataIndex());
	BindConstant(MaterialCache->GetMaterialConst(), 2);

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 3, 0, true);
	BindStorage(Mesh->GetNormalBuffer(), 4, 0, true);
	BindStorage(Mesh->GetTangentBuffer(), 5, 0, true);

	// bind light const
	BindStorage(GDirectionalLightBuffer, 6, 0, true);
	BindStorage(GPointLightBuffer, 7, 0, true);
	BindStorage(GSpotLightBuffer, 8, 0, true);

	if (Gfx->IsRayTracingEnabled())
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