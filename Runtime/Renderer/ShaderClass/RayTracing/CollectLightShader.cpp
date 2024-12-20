#include "CollectLightShader.h"
#include "../../RendererShared.h"

// accept an entry point name as it could be used for both point/spot lights
UHCollectLightShader::UHCollectLightShader(UHGraphic* InGfx, std::string Name, bool bInCollectPointLight)
	: UHShaderClass(InGfx, Name, typeid(UHCollectLightShader))
	, bCollectPointLight(bInCollectPointLight)
{
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
	AddLayoutBinding(1, VK_SHADER_STAGE_COMPUTE_BIT, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	CreateLayoutAndDescriptor();
	OnCompile();
}

void UHCollectLightShader::OnCompile()
{
	std::vector<std::string> Defines;
	if (bCollectPointLight)
	{
		Defines.push_back("COLLECT_POINTLIGHT");
		ShaderCS = Gfx->RequestShader("CollectLightComputeShader", "Shaders/RayTracing/CollectLightComputeShader.hlsl"
			, "CollectPointLightCS", "cs_6_0", Defines);
	}
	else
	{
		Defines.push_back("COLLECT_SPOTLIGHT");
		ShaderCS = Gfx->RequestShader("CollectLightComputeShader", "Shaders/RayTracing/CollectLightComputeShader.hlsl"
			, "CollectSpotLightCS", "cs_6_0", Defines);
	}

	// state
	UHComputePassInfo Info(PipelineLayout);
	Info.CS = ShaderCS;

	CreateComputeState(Info);
}

void UHCollectLightShader::BindParameters()
{
	BindConstant(GSystemConstantBuffer, 0, 0);
	BindStorage(GInstanceLightsBuffer.get(), 1, 0, true);
	BindStorage(GPointLightBuffer, 2, 0, true);
	BindStorage(GSpotLightBuffer, 3, 0, true);
	BindStorage(GObjectConstantBuffer, 4, 0, true);
}