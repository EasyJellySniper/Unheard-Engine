#include "DepthPassShader.h"
#include "../../Components/MeshRenderer.h"
#include "../RendererShared.h"

UHDepthPassShader::UHDepthPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass, UHMaterial* InMat, const std::vector<VkDescriptorSetLayout>& ExtraLayouts)
	: UHShaderClass(InGfx, Name, typeid(UHDepthPassShader), InMat)
{
	// Depth pass: Bind all constants, visiable in VS/PS only
	for (int32_t Idx = 0; Idx < UHConstantTypes::ConstantTypeMax; Idx++)
	{
		AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	}

	// bind UV0 Buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	// textures and samplers will be bound on fly instead, since I go with bindless rendering
	CreateMaterialDescriptor(ExtraLayouts);

	// check opacity texture
	std::vector<std::string> MatDefines = InMat->GetMaterialDefines();

	if (InMat->GetBlendMode() == UHBlendMode::Masked)
	{
		UHMaterialCompileData Data;
		Data.MaterialCache = InMat;
		Data.bIsDepthOrMotionPass = true;
		ShaderPS = InGfx->RequestMaterialShader("DepthPassPS", "Shaders/DepthPixelShader.hlsl", "DepthPS", "ps_6_0", Data, MatDefines);
	}

	ShaderVS = InGfx->RequestShader("DepthPassVS", "Shaders/DepthVertexShader.hlsl", "DepthVS", "vs_6_0", MatDefines);

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

void UHDepthPassShader::BindParameters(const UHMeshRendererComponent* InRenderer)
{
	BindConstant(GSystemConstantBuffer, 0);
	BindConstant(GObjectConstantBuffer, 1, InRenderer->GetBufferDataIndex());
	BindConstant(MaterialCache->GetMaterialConst(), 2);

	UHMesh* Mesh = InRenderer->GetMesh();
	BindStorage(Mesh->GetUV0Buffer(), 3, 0, true);
}