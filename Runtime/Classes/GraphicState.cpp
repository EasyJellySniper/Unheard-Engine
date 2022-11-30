#include "GraphicState.h"
#include <vector>
#include "../../UnheardEngine.h"

UHGraphicState::UHGraphicState()
	: UHGraphicState(UHRenderPassInfo())
{

}

UHGraphicState::UHGraphicState(UHRenderPassInfo InInfo)
	: RenderPassInfo(InInfo)
	, GraphicsPipeline(VK_NULL_HANDLE)
	, RTPipeline(VK_NULL_HANDLE)
{

}

UHGraphicState::UHGraphicState(UHRayTracingInfo InInfo)
	: RayTracingInfo(InInfo)
	, GraphicsPipeline(VK_NULL_HANDLE)
	, RTPipeline(VK_NULL_HANDLE)
{

}

void UHGraphicState::Release()
{
	vkDestroyPipeline(LogicalDevice, GraphicsPipeline, nullptr);
	vkDestroyPipeline(LogicalDevice, RTPipeline, nullptr);
}

VkPipelineVertexInputStateCreateInfo GetVertexInputInfo(VkVertexInputBindingDescription& OutBindingDesc, VkVertexInputAttributeDescription& OutAttributeDesc)
{
	VkPipelineVertexInputStateCreateInfo VertexInputInfo{};
	OutBindingDesc = VkVertexInputBindingDescription{};

	// get binding desc based on layout type
	// in UHE, it uses only position in Input Assembly stage
	// for other attribute such as UV, Normal, Tangent are stored in another buffer, and access via SV
	OutBindingDesc.binding = 0;
	OutBindingDesc.stride = sizeof(XMFLOAT3);
	OutBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	OutAttributeDesc = VkVertexInputAttributeDescription{};
	OutAttributeDesc.binding = 0;
	OutAttributeDesc.location = 0;
	OutAttributeDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
	OutAttributeDesc.offset = 0;

	VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	VertexInputInfo.vertexBindingDescriptionCount = 1;
	VertexInputInfo.vertexAttributeDescriptionCount = 1;
	VertexInputInfo.pVertexBindingDescriptions = &OutBindingDesc;
	VertexInputInfo.pVertexAttributeDescriptions = &OutAttributeDesc;

	return VertexInputInfo;
}

bool UHGraphicState::CreateState(UHRenderPassInfo InInfo)
{
	/*** function for create graphic states, currently doesn't have any dynamic usages ***/

	/*** cache variables for compare ***/
	RenderPassInfo = InInfo;
	bool bHasPixelShader = (RenderPassInfo.PS != nullptr);


	/*** Shader Stage setup, in UHEngine, all states must have at least one VS and PS (or CS) ***/
	std::string VSEntryName = RenderPassInfo.VS->GetEntryName();
	VkPipelineShaderStageCreateInfo VSStageInfo{};
	VSStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	VSStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	VSStageInfo.module = RenderPassInfo.VS->GetShader();
	VSStageInfo.pName = VSEntryName.c_str();

	VkPipelineShaderStageCreateInfo PSStageInfo{};
	std::string PSEntryName;
	if (bHasPixelShader)
	{
		PSEntryName = RenderPassInfo.PS->GetEntryName();
		PSStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		PSStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		PSStageInfo.module = RenderPassInfo.PS->GetShader();
		PSStageInfo.pName = PSEntryName.c_str();
	}

	VkPipelineShaderStageCreateInfo ShaderStages[] = { VSStageInfo, PSStageInfo };


	/*** Dynamic states, for now set viewportand scissor as dynamic usage ***/
	std::vector<VkDynamicState> DynamicStates = 
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo DynamicState{};
	DynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	DynamicState.dynamicStateCount = static_cast<uint32_t>(DynamicStates.size());
	DynamicState.pDynamicStates = DynamicStates.data();


	/*** Vertex input info, this should follow the input mesh layout ***/
	// for now, use default lit layout, and prevent value gets cleared
	VkVertexInputBindingDescription VertexInputBindingDescription;
	VkVertexInputAttributeDescription AttributeDescription;
	VkPipelineVertexInputStateCreateInfo VertexInputInfo = GetVertexInputInfo(VertexInputBindingDescription, AttributeDescription);

	/*** Input assembly, always use triangle list in UH engine ***/
	VkPipelineInputAssemblyStateCreateInfo InputAssembly{};
	InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	InputAssembly.primitiveRestartEnable = VK_FALSE;


	/*** Viewportsand scissors, only needs count since I'll use it as dynamic state ***/
	VkPipelineViewportStateCreateInfo ViewportState{};
	ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	ViewportState.viewportCount = 1;
	ViewportState.scissorCount = 1;


	/*** Rasterization info ***/
	VkPipelineRasterizationStateCreateInfo Rasterizer{};
	Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	Rasterizer.depthClampEnable = VK_FALSE;
	Rasterizer.rasterizerDiscardEnable = VK_FALSE;
	Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	Rasterizer.lineWidth = 1.0f;
	Rasterizer.cullMode = RenderPassInfo.CullMode;
	Rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	Rasterizer.depthBiasEnable = VK_FALSE;
	Rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	Rasterizer.depthBiasClamp = 0.0f; // Optional
	Rasterizer.depthBiasSlopeFactor = 0.0f; // Optional


	/*** I don't use MSAA in UH engine ***/
	VkPipelineMultisampleStateCreateInfo Multisampling{};
	Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	Multisampling.sampleShadingEnable = VK_FALSE;
	Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	Multisampling.minSampleShading = 1.0f; // Optional
	Multisampling.pSampleMask = nullptr; // Optional
	Multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	Multisampling.alphaToOneEnable = VK_FALSE; // Optional


	/*** Depth and stencil testing ***/
	VkPipelineDepthStencilStateCreateInfo DepthStencil{};
	DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	DepthStencil.depthTestEnable = RenderPassInfo.DepthInfo.bEnableDepthTest;
	DepthStencil.depthWriteEnable = RenderPassInfo.DepthInfo.bEnableDepthWrite;
	DepthStencil.depthCompareOp = RenderPassInfo.DepthInfo.DepthFunc;
	DepthStencil.depthBoundsTestEnable = VK_FALSE;
	DepthStencil.minDepthBounds = 0.0f; // Optional
	DepthStencil.maxDepthBounds = 1.0f; // Optional
	DepthStencil.stencilTestEnable = VK_FALSE;
	DepthStencil.front = {}; // Optional
	DepthStencil.back = {}; // Optional


	/*** Color blending, default to opaque ***/
	std::vector<VkPipelineColorBlendAttachmentState> ColorBlendAttachments;
	VkPipelineColorBlendStateCreateInfo ColorBlending = GetBlendStateInfo(RenderPassInfo.BlendMode, RenderPassInfo.RTCount, ColorBlendAttachments);


	/*** finally, create pipeline info ***/
	VkGraphicsPipelineCreateInfo PipelineInfo{};
	PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	PipelineInfo.stageCount = bHasPixelShader ? 2 : 1;
	PipelineInfo.pStages = ShaderStages;
	PipelineInfo.pVertexInputState = &VertexInputInfo;
	PipelineInfo.pInputAssemblyState = &InputAssembly;
	PipelineInfo.pViewportState = &ViewportState;
	PipelineInfo.pRasterizationState = &Rasterizer;
	PipelineInfo.pMultisampleState = &Multisampling;
	PipelineInfo.pDepthStencilState = &DepthStencil;
	PipelineInfo.pColorBlendState = &ColorBlending;
	PipelineInfo.pDynamicState = &DynamicState;
	PipelineInfo.layout = RenderPassInfo.PipelineLayout;
	PipelineInfo.renderPass = RenderPassInfo.RenderPass;
	PipelineInfo.subpass = 0;
	PipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	PipelineInfo.basePipelineIndex = -1; // Optional

	VkResult Result = vkCreateGraphicsPipelines(LogicalDevice, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &GraphicsPipeline);
	if (Result != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create graphics pipeline!\n");
		return false;
	}

	return true;
}

bool UHGraphicState::CreateState(UHRayTracingInfo InInfo)
{
	VkRayTracingPipelineCreateInfoKHR CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	CreateInfo.maxPipelineRayRecursionDepth = InInfo.MaxRecursionDepth;
	CreateInfo.layout = InInfo.PipelineLayout;

	// set RG shader
	std::string RGEntryName = InInfo.RayGenShader->GetEntryName();
	VkPipelineShaderStageCreateInfo RGStageInfo{};
	RGStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	RGStageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	RGStageInfo.module = InInfo.RayGenShader->GetShader();
	RGStageInfo.pName = RGEntryName.c_str();

	// set closest hit shader
	std::string CHGEntryName = InInfo.ClosestHitShader->GetEntryName();
	VkPipelineShaderStageCreateInfo CHGStageInfo{};
	CHGStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	CHGStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	CHGStageInfo.module = InInfo.ClosestHitShader->GetShader();
	CHGStageInfo.pName = CHGEntryName.c_str();

	// set any hit shader (if there is)
	std::string AHGEntryName = InInfo.AnyHitShader->GetEntryName();
	VkPipelineShaderStageCreateInfo AHGStageInfo{};
	if (InInfo.AnyHitShader != nullptr)
	{
		AHGStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		AHGStageInfo.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		AHGStageInfo.module = InInfo.AnyHitShader->GetShader();
		AHGStageInfo.pName = AHGEntryName.c_str();
	}

	VkPipelineShaderStageCreateInfo ShaderStages[] = { RGStageInfo, CHGStageInfo, AHGStageInfo };
	CreateInfo.stageCount = 2;
	if (InInfo.AnyHitShader != nullptr)
	{
		CreateInfo.stageCount = 3;
	}
	CreateInfo.pStages = ShaderStages;

	// setup group info for both RG and HG
	VkRayTracingShaderGroupCreateInfoKHR RGGroupInfo{};
	RGGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	RGGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	RGGroupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	RGGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	RGGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
	RGGroupInfo.generalShader = 0;

	VkRayTracingShaderGroupCreateInfoKHR HGGroupInfo{};
	HGGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	HGGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	HGGroupInfo.closestHitShader = 1;
	HGGroupInfo.anyHitShader = 2;
	HGGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
	HGGroupInfo.generalShader = VK_SHADER_UNUSED_KHR;

	VkRayTracingShaderGroupCreateInfoKHR GroupInfos[] = { RGGroupInfo , HGGroupInfo };
	CreateInfo.groupCount = 2;
	CreateInfo.pGroups = GroupInfos;

	// set payload size
	VkRayTracingPipelineInterfaceCreateInfoKHR PipelineInterfaceInfo{};
	PipelineInterfaceInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR;
	PipelineInterfaceInfo.maxPipelineRayPayloadSize = InInfo.PayloadSize;
	PipelineInterfaceInfo.maxPipelineRayHitAttributeSize = InInfo.AttributeSize;
	CreateInfo.pLibraryInterface = &PipelineInterfaceInfo;

	// create state for ray tracing pipeline
	PFN_vkCreateRayTracingPipelinesKHR CreateRTPipeline = 
		(PFN_vkCreateRayTracingPipelinesKHR)vkGetInstanceProcAddr(VulkanInstance, "vkCreateRayTracingPipelinesKHR");
	
	VkResult Result = CreateRTPipeline(LogicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &CreateInfo, nullptr, &RTPipeline);
	if (Result != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create ray tracing pipeline!\n");
		return false;
	}

	return true;
}

VkPipeline UHGraphicState::GetGraphicPipeline() const
{
	return GraphicsPipeline;
}

VkPipeline UHGraphicState::GetRTPipeline() const
{
	return RTPipeline;
}

bool UHGraphicState::operator==(const UHGraphicState& InState)
{
	// check if render pass info or ray tracing info is the same
	return RenderPassInfo == InState.RenderPassInfo
		&& RayTracingInfo == InState.RayTracingInfo;
}