#include "GraphicState.h"
#include <vector>
#include "../../UnheardEngine.h"
#include "../Classes/Utility.h"
#include "../../Runtime/Engine/GraphicFunction.h"

UHGraphicState::UHGraphicState()
	: UHGraphicState(UHRenderPassInfo())
{

}

UHGraphicState::UHGraphicState(UHRenderPassInfo InInfo)
	: RenderPassInfo(InInfo)
	, PassPipeline(nullptr)
	, RTPipeline(nullptr)
{

}

UHGraphicState::UHGraphicState(UHRayTracingInfo InInfo)
	: RayTracingInfo(InInfo)
	, PassPipeline(nullptr)
	, RTPipeline(nullptr)
{

}

UHGraphicState::UHGraphicState(UHComputePassInfo InInfo)
	: ComputePassInfo(InInfo)
	, PassPipeline(nullptr)
	, RTPipeline(nullptr)
{

}

void UHGraphicState::Release()
{
	vkDestroyPipeline(LogicalDevice, PassPipeline, nullptr);
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

// get blend state info based on input blend mode
VkPipelineColorBlendStateCreateInfo GetBlendStateInfo(UHBlendMode InBlendMode, int32_t RTCount, std::vector<VkPipelineColorBlendAttachmentState>& OutColorBlendAttachments
	, bool bForceBlendOff)
{
	for (int32_t Idx = 0; Idx < RTCount; Idx++)
	{
		VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
		ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		ColorBlendAttachment.blendEnable = VK_FALSE;
		ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

		if (!bForceBlendOff)
		{
			ColorBlendAttachment.blendEnable = VK_TRUE;

			// TranditionalAlpha blend mode: Color = SrcAlpha * SrcColor + (1.0f - SrcAlpha) * DestColor
			if (InBlendMode == UHBlendMode::TranditionalAlpha)
			{
				ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			}

			// addition mode: Color = SrcColor + DstColor
			if (InBlendMode == UHBlendMode::Addition)
			{
				ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			}
		}

		OutColorBlendAttachments.push_back(ColorBlendAttachment);
	}

	VkPipelineColorBlendStateCreateInfo ColorBlending{};
	ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	ColorBlending.logicOpEnable = VK_FALSE;
	ColorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	ColorBlending.attachmentCount = RTCount;
	ColorBlending.pAttachments = OutColorBlendAttachments.data();
	ColorBlending.blendConstants[0] = 0.0f; // Optional
	ColorBlending.blendConstants[1] = 0.0f; // Optional
	ColorBlending.blendConstants[2] = 0.0f; // Optional
	ColorBlending.blendConstants[3] = 0.0f; // Optional

	return ColorBlending;
}

bool UHGraphicState::CreateState(UHRenderPassInfo InInfo)
{
	/*** function for create graphic states, currently doesn't have any dynamic usages ***/

	/*** cache variables for compare ***/
	RenderPassInfo = InInfo;
	bool bHasPixelShader = (RenderPassInfo.PS != UHINDEXNONE);
	bool bHasGeometryShader = (RenderPassInfo.GS != UHINDEXNONE);

	// lookup shader in object table
	const UHShader* VS = SafeGetObjectFromTable<const UHShader>(RenderPassInfo.VS);
	const UHShader* PS = SafeGetObjectFromTable<const UHShader>(RenderPassInfo.PS);
	const UHShader* GS = SafeGetObjectFromTable<const UHShader>(RenderPassInfo.GS);

	/*** Shader Stage setup, in UHEngine, all states must have at least one VS and PS (or CS) ***/
	std::string VSEntryName = VS->GetEntryName();
	VkPipelineShaderStageCreateInfo VSStageInfo{};
	VSStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	VSStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	VSStageInfo.module = VS->GetShader();
	VSStageInfo.pName = VSEntryName.c_str();

	VkPipelineShaderStageCreateInfo PSStageInfo{};
	std::string PSEntryName;
	if (bHasPixelShader)
	{
		PSEntryName = PS->GetEntryName();
		PSStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		PSStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		PSStageInfo.module = PS->GetShader();
		PSStageInfo.pName = PSEntryName.c_str();
	}

	VkPipelineShaderStageCreateInfo GSStageInfo{};
	std::string GSEntryName;
	if (bHasGeometryShader)
	{
		GSEntryName = GS->GetEntryName();
		GSStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		GSStageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
		GSStageInfo.module = GS->GetShader();
		GSStageInfo.pName = GSEntryName.c_str();
	}

	// collect shader stage
	std::vector<VkPipelineShaderStageCreateInfo> ShaderStages = { VSStageInfo };
	if (bHasPixelShader)
	{
		ShaderStages.push_back(PSStageInfo);
	}

	if (bHasGeometryShader)
	{
		ShaderStages.push_back(GSStageInfo);
	}


	/*** Dynamic states, for now set viewportand scissor as dynamic usage ***/
	std::vector<VkDynamicState> DynamicStates = 
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_LINE_WIDTH
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
	InputAssembly.topology = (RenderPassInfo.bDrawLine) ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
	Rasterizer.polygonMode = (InInfo.bDrawWireFrame) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
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
	VkPipelineColorBlendStateCreateInfo ColorBlending = GetBlendStateInfo(RenderPassInfo.BlendMode, RenderPassInfo.RTCount, ColorBlendAttachments
		, RenderPassInfo.bForceBlendOff);


	/*** finally, create pipeline info ***/
	VkGraphicsPipelineCreateInfo PipelineInfo{};
	PipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	PipelineInfo.stageCount = static_cast<uint32_t>(ShaderStages.size());
	PipelineInfo.pStages = ShaderStages.data();
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
	PipelineInfo.basePipelineHandle = nullptr; // Optional
	PipelineInfo.basePipelineIndex = UHINDEXNONE; // Optional

	VkResult Result = vkCreateGraphicsPipelines(LogicalDevice, nullptr, 1, &PipelineInfo, nullptr, &PassPipeline);
	if (Result != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create graphics pipeline!\n");
		return false;
	}

	return true;
}

bool UHGraphicState::CreateState(UHRayTracingInfo InInfo)
{
	RayTracingInfo = InInfo;
	const UHShader* RG = SafeGetObjectFromTable<const UHShader>(RayTracingInfo.RayGenShader);
	const UHShader* Miss = SafeGetObjectFromTable<const UHShader>(RayTracingInfo.MissShader);

	VkRayTracingPipelineCreateInfoKHR CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	CreateInfo.maxPipelineRayRecursionDepth = RayTracingInfo.MaxRecursionDepth;
	CreateInfo.layout = RayTracingInfo.PipelineLayout;

	// set RG shader
	std::string RGEntryName = RG->GetEntryName();
	VkPipelineShaderStageCreateInfo RGStageInfo{};
	RGStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	RGStageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	RGStageInfo.module = RG->GetShader();
	RGStageInfo.pName = RGEntryName.c_str();

	// set miss shader
	std::string MissEntryName = Miss->GetEntryName();
	VkPipelineShaderStageCreateInfo MissStageInfo{};
	MissStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	MissStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	MissStageInfo.module = Miss->GetShader();
	MissStageInfo.pName = MissEntryName.c_str();

	// closest hit and any hit should have the same size
	assert(RayTracingInfo.ClosestHitShaders.size() == RayTracingInfo.AnyHitShaders.size());

	// set closest hit shader
	std::vector<VkPipelineShaderStageCreateInfo> CHGStageInfos(RayTracingInfo.ClosestHitShaders.size());
	std::vector<std::string> CHGEntryNames(RayTracingInfo.ClosestHitShaders.size());

	std::vector<UHShader*> ClosestHits(RayTracingInfo.ClosestHitShaders.size());
	for (size_t Idx = 0; Idx < RayTracingInfo.ClosestHitShaders.size(); Idx++)
	{
		ClosestHits[Idx] = SafeGetObjectFromTable<UHShader>(RayTracingInfo.ClosestHitShaders[Idx]);
		VkPipelineShaderStageCreateInfo CHGStageInfo{};
		if (ClosestHits[Idx] != nullptr)
		{
			CHGEntryNames[Idx] = ClosestHits[Idx]->GetEntryName();
			CHGStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			CHGStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			CHGStageInfo.module = ClosestHits[Idx]->GetShader();
			CHGStageInfo.pName = CHGEntryNames[Idx].c_str();
			CHGStageInfos[Idx] = CHGStageInfo;
		}
	}

	// set any hit shader
	std::vector<VkPipelineShaderStageCreateInfo> AHGStageInfos(RayTracingInfo.AnyHitShaders.size());
	std::vector<std::string> AHGEntryNames(RayTracingInfo.AnyHitShaders.size());

	std::vector<UHShader*> AnyHits(RayTracingInfo.AnyHitShaders.size());
	for (size_t Idx = 0; Idx < RayTracingInfo.AnyHitShaders.size(); Idx++)
	{
		AnyHits[Idx] = SafeGetObjectFromTable<UHShader>(RayTracingInfo.AnyHitShaders[Idx]);
		VkPipelineShaderStageCreateInfo AHGStageInfo{};
		if (AnyHits[Idx] != nullptr)
		{
			AHGEntryNames[Idx] = AnyHits[Idx]->GetEntryName();
			AHGStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			AHGStageInfo.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
			AHGStageInfo.module = AnyHits[Idx]->GetShader();
			AHGStageInfo.pName = AHGEntryNames[Idx].c_str();
			AHGStageInfos[Idx] = AHGStageInfo;
		}
	}

	std::vector<VkPipelineShaderStageCreateInfo> ShaderStages = { RGStageInfo, MissStageInfo };
	// insert hit groups
	for (size_t Idx = 0; Idx < CHGStageInfos.size(); Idx++)
	{
		ShaderStages.push_back(CHGStageInfos[Idx]);
		ShaderStages.push_back(AHGStageInfos[Idx]);
	}

	CreateInfo.stageCount = static_cast<uint32_t>(ShaderStages.size());
	CreateInfo.pStages = ShaderStages.data();

	// setup group info for RG, MissG and HG
	VkRayTracingShaderGroupCreateInfoKHR RGGroupInfo{};
	RGGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	RGGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	RGGroupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	RGGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	RGGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
	RGGroupInfo.generalShader = GRayGenTableSlot;

	VkRayTracingShaderGroupCreateInfoKHR MissGroupInfo{};
	MissGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	MissGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	MissGroupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	MissGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	MissGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
	MissGroupInfo.generalShader = GMissTableSlot;

	// setup all hit groups, for each record has one closest and one anyhit
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> HGGroupInfos(RayTracingInfo.AnyHitShaders.size());
	for (size_t Idx = 0; Idx < RayTracingInfo.AnyHitShaders.size(); Idx++)
	{
		VkRayTracingShaderGroupCreateInfoKHR HGGroupInfo{};
		HGGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		HGGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		HGGroupInfo.closestHitShader = static_cast<uint32_t>(GHitGroupTableSlot + Idx * GHitGroupShaderPerSlot);
		HGGroupInfo.anyHitShader = static_cast<uint32_t>(GHitGroupTableSlot + 1 + Idx * GHitGroupShaderPerSlot);
		HGGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
		HGGroupInfo.generalShader = VK_SHADER_UNUSED_KHR;
		HGGroupInfos[Idx] = HGGroupInfo;
	}

	std::vector<VkRayTracingShaderGroupCreateInfoKHR> GroupInfos = { RGGroupInfo, MissGroupInfo };
	GroupInfos.insert(GroupInfos.end(), HGGroupInfos.begin(), HGGroupInfos.end());
	CreateInfo.groupCount = static_cast<uint32_t>(GroupInfos.size());
	CreateInfo.pGroups = GroupInfos.data();

	// set payload size
	VkRayTracingPipelineInterfaceCreateInfoKHR PipelineInterfaceInfo{};
	PipelineInterfaceInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_INTERFACE_CREATE_INFO_KHR;
	PipelineInterfaceInfo.maxPipelineRayPayloadSize = RayTracingInfo.PayloadSize;
	PipelineInterfaceInfo.maxPipelineRayHitAttributeSize = RayTracingInfo.AttributeSize;
	CreateInfo.pLibraryInterface = &PipelineInterfaceInfo;

	// create state for ray tracing pipeline
	VkResult Result = GVkCreateRayTracingPipelinesKHR(LogicalDevice, nullptr, nullptr, 1, &CreateInfo, nullptr, &RTPipeline);
	if (Result != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create ray tracing pipeline!\n");
		return false;
	}

	return true;
}

bool UHGraphicState::CreateState(UHComputePassInfo InInfo)
{
	ComputePassInfo = InInfo;
	const UHShader* CS = SafeGetObjectFromTable<const UHShader>(ComputePassInfo.CS);

	VkPipelineShaderStageCreateInfo CSStageInfo{};
	std::string CSEntryName = CS->GetEntryName();
	CSStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	CSStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	CSStageInfo.module = CS->GetShader();
	CSStageInfo.pName = CSEntryName.c_str();

	// similar to graphic pipeline creation but this is for compute pipeline
	VkComputePipelineCreateInfo PipelineInfo{};
	PipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	PipelineInfo.layout = ComputePassInfo.PipelineLayout;
	PipelineInfo.stage = CSStageInfo;

	// optional
	PipelineInfo.flags = 0;
	PipelineInfo.basePipelineHandle = nullptr;
	PipelineInfo.basePipelineIndex = UHINDEXNONE;

	VkResult Result = vkCreateComputePipelines(LogicalDevice, nullptr, 1, &PipelineInfo, nullptr, &PassPipeline);
	if (Result != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create graphics pipeline!\n");
		return false;
	}

	return true;
}

VkPipeline UHGraphicState::GetPassPipeline() const
{
	return PassPipeline;
}

VkPipeline UHGraphicState::GetRTPipeline() const
{
	return RTPipeline;
}

bool UHGraphicState::operator==(const UHGraphicState& InState)
{
	// check if render pass info or ray tracing info is the same
	bool bRenderPassEqual = RenderPassInfo == InState.RenderPassInfo;
	bool bRayTracingEqual = RayTracingInfo == InState.RayTracingInfo;
	bool bComputePassEqual = ComputePassInfo == InState.ComputePassInfo;

	return bRenderPassEqual && bRayTracingEqual && bComputePassEqual;
}