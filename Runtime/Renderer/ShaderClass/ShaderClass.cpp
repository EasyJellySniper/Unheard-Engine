#include "ShaderClass.h"

static std::unordered_map<std::type_index, VkDescriptorSetLayout> SetLayoutTable;
static std::unordered_map<std::type_index, VkPipelineLayout> PipelineLayoutTable;

UHShaderClass::UHShaderClass()
	: UHShaderClass(nullptr, "", typeid(UHShaderClass))
{

}

UHShaderClass::UHShaderClass(UHGraphic* InGfx, std::string InName, std::type_index InType)
	: Gfx(InGfx)
	, Name(InName)
	, TypeIndexCache(InType)
	, PipelineLayout(VK_NULL_HANDLE)
	, DescriptorPool(VK_NULL_HANDLE)
	, ShaderVS(nullptr)
	, ShaderGS(nullptr)
	, ShaderPS(nullptr)
	, RayGenShader(nullptr)
	, ClosestHitShader(nullptr)
	, AnyHitShader(nullptr)
	, GraphicState(nullptr)
	, RTState(nullptr)
	, RayGenTable(nullptr)
	, HitGroupTable(nullptr)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		DescriptorSets[Idx] = VK_NULL_HANDLE;
	}
}

void UHShaderClass::Release()
{
	VkDevice LogicalDevice = Gfx->GetLogicalDevice();
	vkDestroyDescriptorPool(LogicalDevice, DescriptorPool, nullptr);

	// release layout
	for (auto& SetLayout : SetLayoutTable)
	{
		vkDestroyDescriptorSetLayout(LogicalDevice, SetLayout.second, nullptr);
	}
	SetLayoutTable.clear();

	for (auto& PipelineLayout : PipelineLayoutTable)
	{
		vkDestroyPipelineLayout(LogicalDevice, PipelineLayout.second, nullptr);
	}
	PipelineLayoutTable.clear();

	UH_SAFE_RELEASE(RayGenTable);
	RayGenTable.reset();

	UH_SAFE_RELEASE(HitGroupTable);
	HitGroupTable.reset();
}

void UHShaderClass::BindImage(const UHTexture* InImage, int32_t DstBinding, int32_t CurrentFrame)
{
	if (CurrentFrame < 0)
	{
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
			if (InImage)
			{
				Helper.WriteImage(InImage, DstBinding);
			}
		}
	}
	else
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[CurrentFrame]);
		if (InImage)
		{
			Helper.WriteImage(InImage, DstBinding);
		}
	}
}

void UHShaderClass::BindImage(const std::vector<UHTexture*> InImages, int32_t DstBinding)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
		if (InImages.size() > 0)
		{
			Helper.WriteImage(InImages, DstBinding);
		}
	}
}

void UHShaderClass::BindRWImage(const UHTexture* InImage, int32_t DstBinding)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
		if (InImage)
		{
			Helper.WriteImage(InImage, DstBinding, true);
		}
	}
}

void UHShaderClass::BindSampler(const UHSampler* InSampler, int32_t DstBinding)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
		if (InSampler)
		{
			Helper.WriteSampler(InSampler, DstBinding);
		}
	}
}

void UHShaderClass::BindSampler(const std::vector<UHSampler*>& InSamplers, int32_t DstBinding)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
		if (InSamplers.size() > 0)
		{
			Helper.WriteSampler(InSamplers, DstBinding);
		}
	}
}

void UHShaderClass::BindTLAS(const UHAccelerationStructure* InTopAS, int32_t DstBinding, int32_t CurrentFrame)
{
	UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[CurrentFrame]);
	if (InTopAS)
	{
		Helper.WriteTLAS(InTopAS, DstBinding);
	}
}

UHShader* UHShaderClass::GetRayGenShader()
{
	return RayGenShader;
}

UHShader* UHShaderClass::GetClosestShader()
{
	return ClosestHitShader;
}

UHShader* UHShaderClass::GetAnyHitShader()
{
	return AnyHitShader;
}

UHGraphicState* UHShaderClass::GetState()
{
	return GraphicState;
}

UHGraphicState* UHShaderClass::GetRTState()
{
	return RTState;
}

UHRenderBuffer<UHShaderRecord>* UHShaderClass::GetRayGenTable()
{
	return RayGenTable.get();
}

UHRenderBuffer<UHShaderRecord>* UHShaderClass::GetHitGroupTable()
{
	return HitGroupTable.get();
}

VkDescriptorSetLayout UHShaderClass::GetDescriptorSetLayout()
{
	return SetLayoutTable[TypeIndexCache];
}

VkPipelineLayout UHShaderClass::GetPipelineLayout()
{
	return PipelineLayoutTable[TypeIndexCache];
}

VkDescriptorSet UHShaderClass::GetDescriptorSet(int32_t FrameIdx)
{
	return DescriptorSets[FrameIdx];
}

// add layout binding
void UHShaderClass::AddLayoutBinding(uint32_t DescriptorCount, VkShaderStageFlags StageFlags, VkDescriptorType DescriptorType, int32_t OverrideBind)
{
	VkDescriptorSetLayoutBinding LayoutBinding{};
	LayoutBinding.descriptorCount = DescriptorCount;
	LayoutBinding.stageFlags = StageFlags;
	LayoutBinding.descriptorType = DescriptorType;

	// binding idx will accumulate during adding
	if (OverrideBind < 0)
	{
		LayoutBinding.binding = static_cast<uint32_t>(LayoutBindings.size());
	}
	else
	{
		LayoutBinding.binding = OverrideBind;
	}
	LayoutBindings.push_back(LayoutBinding);
}

void UHShaderClass::CreateDescriptor(std::vector<VkDescriptorSetLayout> AdditionalLayout)
{
	VkDevice LogicalDevice = Gfx->GetLogicalDevice();

	// don't duplicate layout
	if (SetLayoutTable.find(TypeIndexCache) == SetLayoutTable.end())
	{
		VkDescriptorSetLayoutCreateInfo LayoutInfo{};
		LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		LayoutInfo.bindingCount = static_cast<uint32_t>(LayoutBindings.size());
		LayoutInfo.pBindings = LayoutBindings.data();

		if (vkCreateDescriptorSetLayout(LogicalDevice, &LayoutInfo, nullptr, &SetLayoutTable[TypeIndexCache]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create descriptor set layout for shader: " + UHUtilities::ToStringW(Name) + L"\n");
		}
	}

	// don't duplicate layout
	if (PipelineLayoutTable.find(TypeIndexCache) == PipelineLayoutTable.end())
	{
		// one layout per set
		VkPipelineLayoutCreateInfo PipelineLayoutInfo{};
		PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		PipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(1 + AdditionalLayout.size());

		std::vector<VkDescriptorSetLayout> Layouts = { SetLayoutTable[TypeIndexCache] };
		if (AdditionalLayout.size() > 0)
		{
			Layouts.insert(Layouts.end(), AdditionalLayout.begin(), AdditionalLayout.end());
		}
		PipelineLayoutInfo.pSetLayouts = Layouts.data();

		if (vkCreatePipelineLayout(LogicalDevice, &PipelineLayoutInfo, nullptr, &PipelineLayoutTable[TypeIndexCache]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create pipeline layout for shader: " + UHUtilities::ToStringW(Name) + L"\n");
		}
	}
	PipelineLayout = PipelineLayoutTable[TypeIndexCache];

	// create DescriptorPools
	std::vector<VkDescriptorPoolSize> PoolSizes;

	// pool type can be single or multiple
	for (size_t Idx = 0; Idx < LayoutBindings.size(); Idx++)
	{
		VkDescriptorPoolSize PoolSize{};
		PoolSize.type = LayoutBindings[Idx].descriptorType;
		PoolSize.descriptorCount = GMaxFrameInFlight;
		PoolSizes.push_back(PoolSize);
	}

	VkDescriptorPoolCreateInfo PoolInfo{};
	PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	PoolInfo.poolSizeCount = static_cast<uint32_t>(PoolSizes.size());
	PoolInfo.pPoolSizes = PoolSizes.data();
	PoolInfo.maxSets = GMaxFrameInFlight;

	if (vkCreateDescriptorPool(LogicalDevice, &PoolInfo, nullptr, &DescriptorPool) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create descriptor pool for shader: " + UHUtilities::ToStringW(Name) + L"\n");
	}

	std::vector<VkDescriptorSetLayout> SetLayouts(GMaxFrameInFlight, SetLayoutTable[TypeIndexCache]);
	VkDescriptorSetAllocateInfo AllocInfo{};
	AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	AllocInfo.descriptorPool = DescriptorPool;
	AllocInfo.descriptorSetCount = GMaxFrameInFlight;
	AllocInfo.pSetLayouts = SetLayouts.data();

	if (vkAllocateDescriptorSets(LogicalDevice, &AllocInfo, DescriptorSets.data()) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to create descriptor sets for shader: " + UHUtilities::ToStringW(Name) + L"\n");
	}
}