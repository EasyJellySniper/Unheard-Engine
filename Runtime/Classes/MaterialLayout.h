#pragma once
#include "../Engine/RenderResource.h"
#include <vector>

// header for material layout defines
enum UHBlendMode
{
	Opaque = 0,
	Masked,
	TranditionalAlpha,
	Addition
};

// constant types
enum UHConstantTypes
{
	System = 0,
	Object,
	Material,
	ConstantTypeMax
};

// shading model define, this is mainly used for creating rendering related stuff, e.g. DefaultLitMeshBuffer
enum UHShadingModel
{
	DefaultLit,
	ShadingModelMax
};

// texture type used for material
enum UHMaterialTextureType
{
	Diffuse = 0,
	Occlusion,
	Specular,
	Normal,
	Opacity,
	SkyCube,
	Metallic,
	Roughness,
	TextureTypeMax
};

// shader type used for material
enum UHMaterialShaderType
{
	VS = 0,
	PS,
	MaterialShaderTypeMax
};

// get blend state info based on input blend mode
inline VkPipelineColorBlendStateCreateInfo GetBlendStateInfo(UHBlendMode InBlendMode, int32_t RTCount, std::vector<VkPipelineColorBlendAttachmentState>& OutColorBlendAttachments)
{
	for (int32_t Idx = 0; Idx < RTCount; Idx++)
	{
		VkPipelineColorBlendAttachmentState ColorBlendAttachment{};
		ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		ColorBlendAttachment.blendEnable = (InBlendMode > UHBlendMode::Masked) ? VK_TRUE : VK_FALSE;
		ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
		ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

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