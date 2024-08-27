#include "OcclusionPassShader.h"
#include "../RendererShared.h"
#include "../../Components/MeshRenderer.h"

UHGraphicState* UHOcclusionPassShader::OcclusionStateCache;

UHOcclusionPassShader::UHOcclusionPassShader(UHGraphic* InGfx, std::string Name, VkRenderPass InRenderPass)
	: UHShaderClass(InGfx, Name, typeid(UHOcclusionPassShader), nullptr, InRenderPass)
{
	// only need a system constant buffer
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_VERTEX_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHOcclusionPassShader::OnCompile()
{
	// early out if cached
	if (OcclusionStateCache != nullptr)
	{
		// restore cached value
		const UHRenderPassInfo& PassInfo = OcclusionStateCache->GetRenderPassInfo();
		ShaderVS = PassInfo.VS;
		return;
	}

	ShaderVS = Gfx->RequestShader("DepthPassVS", "Shaders/DepthVertexShader.hlsl", "DepthVS", "vs_6_0");

	UHRenderPassInfo RenderPassInfo = UHRenderPassInfo(RenderPassCache
		, UHDepthInfo(true, false, VK_COMPARE_OP_GREATER_OR_EQUAL)
		, UHCullMode::CullNone
		, UHBlendMode::Opaque
		, ShaderVS
		, UHINDEXNONE
		, 1
		, PipelineLayout);

	RenderPassInfo.bEnableColorWrite = false;
	RenderPassInfo.bForceBlendOff = true;

	// create occlusion state and cache it
	CreateGraphicState(RenderPassInfo);
	OcclusionStateCache = GetState();
}

void UHOcclusionPassShader::BindParameters(const UHMeshRendererComponent* InRenderer)
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindConstant(GOcclusionConstantBuffer, 1, InRenderer->GetBufferDataIndex());
}

void UHOcclusionPassShader::ResetOcclusionState()
{
	OcclusionStateCache = nullptr;
}

UHGraphicState* UHOcclusionPassShader::GetOcclusionState()
{
	return OcclusionStateCache;
}