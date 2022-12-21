#include "BasePassShader.h"
#include "../../Components/MeshRenderer.h"

UHBasePassShader::UHBasePassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, bool bEnableDepthPrePass)
	: UHShaderClass(InGfx, Name, typeid(UHBasePassShader))
{
	// DeferredPass: Bind all constants, visiable in VS/PS only
	// use storage buffer on materials
	for (uint32_t Idx = 0; Idx < UHConstantTypes::ConstantTypeMax; Idx++)
	{
		if (Idx == UHConstantTypes::Material)
		{
			AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
		}
		else if (Idx == UHConstantTypes::Object)
		{
			AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		}
		else
		{
			AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		}
	}

	// bind occlusion visible buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// Bind texture/sampler inputs as well, following constants
	for (int32_t Idx = 0; Idx < UHMaterialTextureType::TextureTypeMax; Idx++)
	{
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);
	}

	// bind UV0/Normal/Tangent buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	CreateDescriptor();

	// also check occlusion define for VS
	std::vector<std::string> VSDefines = InMat->GetMaterialDefines(VS);
	if (InGfx->IsRayTracingOcclusionTestEnabled())
	{
		VSDefines.push_back("WITH_OCCLUSION_TEST");
	}

	ShaderVS = InGfx->RequestShader("BaseVertexShader", "Shaders/BaseVertexShader.hlsl", "BaseVS", "vs_6_0", VSDefines);
	ShaderPS = InGfx->RequestShader("BasePixelShader", "Shaders/BasePixelShader.hlsl", "BasePS", "ps_6_0", InMat->GetMaterialDefines(PS));

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

	GraphicState = InGfx->RequestGraphicState(Info);
}

void UHBasePassShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const std::array<std::unique_ptr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
	, const std::array<std::unique_ptr<UHRenderBuffer<UHMaterialConstants>>, GMaxFrameInFlight>& MatConst
	, const std::array<std::unique_ptr<UHRenderBuffer<uint32_t>>, GMaxFrameInFlight>& OcclusionConst
	, const UHMeshRendererComponent* InRenderer)
{
	BindConstant(SysConst, 0);
	BindConstant(ObjConst, 1, InRenderer->GetBufferDataIndex());
	BindStorage(MatConst, 2, InRenderer->GetMaterial()->GetBufferDataIndex());

	if (Gfx->IsRayTracingOcclusionTestEnabled())
	{
		BindStorage(OcclusionConst, 3, 0, true);
	}

	// write textures/samplers when they are available
	for (int32_t Tdx = 0; Tdx < UHMaterialTextureType::TextureTypeMax; Tdx++)
	{
		UHTexture* Tex = InRenderer->GetMaterial()->GetTex(static_cast<UHMaterialTextureType>(Tdx));
		if (Tex)
		{
			BindImage(Tex, UHConstantTypes::ConstantTypeMax + 1 + Tdx * 2);
		}

		UHSampler* Sampler = InRenderer->GetMaterial()->GetSampler(static_cast<UHMaterialTextureType>(Tdx));
		if (Sampler)
		{
			BindSampler(Sampler, UHConstantTypes::ConstantTypeMax + 2 + Tdx * 2);
		}
	}

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 20, 0, true);
	BindStorage(Mesh->GetNormalBuffer(), 21, 0, true);
	BindStorage(Mesh->GetTangentBuffer(), 22, 0, true);
}