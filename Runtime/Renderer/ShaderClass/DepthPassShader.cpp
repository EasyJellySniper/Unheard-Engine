#include "DepthPassShader.h"
#include "../../Components/MeshRenderer.h"

UHDepthPassShader::UHDepthPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat)
	: UHShaderClass(InGfx, Name, typeid(UHDepthPassShader))
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

	// bind opacity 
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER);

	// bind UV0 Buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	CreateDescriptor();

	// check opacity texture
	std::vector<std::string> OpacityDefine = {};
	if (UHTexture* OpacityTex = InMat->GetTex(UHMaterialTextureType::Opacity))
	{
		OpacityDefine.push_back("WITH_OPACITY");
	}

	// check occlusion test define
	if (InGfx->IsRayTracingOcclusionTestEnabled())
	{
		OpacityDefine.push_back("WITH_OCCLUSION_TEST");
	}

	ShaderVS = InGfx->RequestShader("DepthPassVS", "Shaders/DepthPassShader.hlsl", "DepthVS", "vs_6_0", OpacityDefine);
	if (InMat->GetBlendMode() == UHBlendMode::Masked)
	{
		ShaderPS = InGfx->RequestShader("DepthPassPS", "Shaders/DepthPassShader.hlsl", "DepthPS", "ps_6_0", OpacityDefine);
	}

	// states
	UHRenderPassInfo Info = UHRenderPassInfo(InRenderPass, UHDepthInfo(true, true, VK_COMPARE_OP_GREATER)
		, InMat->GetCullMode()
		, InMat->GetBlendMode()
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	GraphicState = InGfx->RequestGraphicState(Info);
}

void UHDepthPassShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
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

	// check alpha test textures
	if (UHTexture* Tex = InRenderer->GetMaterial()->GetTex(UHMaterialTextureType::Opacity))
	{
		BindImage(Tex, 4);
		BindSampler(InRenderer->GetMaterial()->GetSampler(UHMaterialTextureType::Opacity), 5);
	}

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 6, 0, true);
}