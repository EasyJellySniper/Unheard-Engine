#include "DepthPassShader.h"
#include "../../Components/MeshRenderer.h"

UHDepthPassShader::UHDepthPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHDepthPassShader), InMat)
{
	// Depth pass: Bind all constants, visiable in VS/PS only
	for (int32_t Idx = 0; Idx < UHConstantTypes::ConstantTypeMax; Idx++)
	{
		AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}

	// bind occlusion visible buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// bind UV0 Buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// textures and samplers will be bound on fly instead, since I go with bindless rendering
	CreateMaterialDescriptor(ExtraLayouts);

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
		ShaderPS = InGfx->RequestMaterialShader("DepthPassPS", "Shaders/DepthPixelShader.hlsl", "DepthPS", "ps_6_0", Data, OpacityDefine);
	}

	ShaderVS = InGfx->RequestShader("DepthPassVS", "Shaders/DepthVertexShader.hlsl", "DepthVS", "vs_6_0", OpacityDefine);

	// states
	MaterialPassInfo = UHRenderPassInfo(InRenderPass, UHDepthInfo(true, true, VK_COMPARE_OP_GREATER)
		, InMat->GetCullMode()
		, InMat->GetBlendMode()
		, ShaderVS
		, ShaderPS
		, 1
		, PipelineLayout);

	CreateMaterialState(MaterialPassInfo);
}

void UHDepthPassShader::BindParameters(const std::array<std::unique_ptr<UHRenderBuffer<UHSystemConstants>>, GMaxFrameInFlight>& SysConst
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
}