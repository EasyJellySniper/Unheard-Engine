#include "DepthPassShader.h"
#include "../../Components/MeshRenderer.h"

UHDepthPassShader::UHDepthPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat)
	: UHShaderClass(InGfx, Name, typeid(UHDepthPassShader), InMat)
{
	// Depth pass: Bind all constants, visiable in VS/PS only
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

	// bind UV0 Buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// bind textures from material
	int32_t TexSlot = GMaterialTextureRegisterStart;
	for (const std::string RegisteredTexture : InMat->GetRegisteredTextureNames(true))
	{
		AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, TexSlot++);
	}
	AddLayoutBinding(1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_SAMPLER, TexSlot);

	CreateMaterialDescriptor();

	// check opacity texture
	std::vector<std::string> OpacityDefine = {};

	// check occlusion test define
	if (InGfx->IsRayTracingOcclusionTestEnabled())
	{
		OpacityDefine.push_back("WITH_OCCLUSION_TEST");
	}

	if (InMat->GetBlendMode() == UHBlendMode::Masked)
	{
		OpacityDefine.push_back("WITH_ALPHATEST");

		UHMaterialCompileData Data;
		Data.MaterialCache = InMat;
		Data.bIsDepthOrMotionPass = true;
		ShaderPS = InGfx->RequestMaterialPixelShader("DepthPassPS", "Shaders/DepthPassShader.hlsl", "DepthPS", "ps_6_0", Data, OpacityDefine);
	}

	ShaderVS = InGfx->RequestShader("DepthPassVS", "Shaders/DepthPassShader.hlsl", "DepthVS", "vs_6_0", OpacityDefine);

	// states
	UHRenderPassInfo Info = UHRenderPassInfo(InRenderPass, UHDepthInfo(true, true, VK_COMPARE_OP_GREATER)
		, InMat->GetCullMode()
		, InMat->GetBlendMode()
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	CreateMaterialState(Info);
}

void UHDepthPassShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
	, const std::array<std::unique_ptr<UHRenderBuffer<UHObjectConstants>>, GMaxFrameInFlight>& ObjConst
	, const std::array<std::unique_ptr<UHRenderBuffer<UHMaterialConstants>>, GMaxFrameInFlight>& MatConst
	, const std::unique_ptr<UHRenderBuffer<uint32_t>>& OcclusionConst
	, const UHMeshRendererComponent* InRenderer
	, const UHAssetManager* InAssetMgr
	, const UHSampler* InDefaultSampler)
{
	BindConstant(SysConst, 0);
	BindConstant(ObjConst, 1, InRenderer->GetBufferDataIndex());
	BindStorage(MatConst, 2, InRenderer->GetMaterial()->GetBufferDataIndex());

	if (Gfx->IsRayTracingOcclusionTestEnabled())
	{
		BindStorage(OcclusionConst.get(), 3, 0, true);
	}

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 4, 0, true);

	// bind textures from material
	int32_t TexSlot = GMaterialTextureRegisterStart;
	for (const std::string RegisteredTexture : InRenderer->GetMaterial()->GetRegisteredTextureNames(true))
	{
		BindImage(InAssetMgr->GetTexture2D(RegisteredTexture), TexSlot);
		TexSlot++;
	}

	BindSampler(InDefaultSampler, TexSlot);
}