#include "ShaderClass.h"

static std::unordered_map<std::type_index, VkDescriptorSetLayout> GSetLayoutTable;
static std::unordered_map<std::type_index, VkPipelineLayout> GPipelineLayoutTable;
static std::unordered_map<uint32_t, VkDescriptorSetLayout> GMaterialSetLayoutTable;
static std::unordered_map<uint32_t, VkPipelineLayout> GMaterialPipelineLayoutTable;
std::unordered_map<uint32_t, UHGraphicState*> GGraphicStateTable;

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
	, ShaderPS(nullptr)
	, RayGenShader(nullptr)
	, ClosestHitShader(nullptr)
	, AnyHitShader(nullptr)
	, RTState(nullptr)
	, RayGenTable(nullptr)
	, HitGroupTable(nullptr)
	, MaterialCache(nullptr)
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

	// release layout depending on type
	if (MaterialCache != nullptr)
	{
		if (GMaterialSetLayoutTable.find(MaterialCache->GetId()) != GMaterialSetLayoutTable.end())
		{
			vkDestroyDescriptorSetLayout(LogicalDevice, GMaterialSetLayoutTable[MaterialCache->GetId()], nullptr);
			GMaterialSetLayoutTable.erase(MaterialCache->GetId());
		}

		if (GMaterialPipelineLayoutTable.find(MaterialCache->GetId()) != GMaterialPipelineLayoutTable.end())
		{
			vkDestroyPipelineLayout(LogicalDevice, GMaterialPipelineLayoutTable[MaterialCache->GetId()], nullptr);
			GMaterialPipelineLayoutTable.erase(MaterialCache->GetId());
		}

		// remove state from lookup table
		if (GGraphicStateTable.find(MaterialCache->GetId()) != GGraphicStateTable.end())
		{
			Gfx->RequestReleaseGraphicState(GGraphicStateTable[MaterialCache->GetId()]);
			GGraphicStateTable.erase(MaterialCache->GetId());
		}
	}
	else
	{
		if (GSetLayoutTable.find(TypeIndexCache) != GSetLayoutTable.end())
		{
			vkDestroyDescriptorSetLayout(LogicalDevice, GSetLayoutTable[TypeIndexCache], nullptr);
			GSetLayoutTable.erase(TypeIndexCache);
		}

		if (GPipelineLayoutTable.find(TypeIndexCache) != GPipelineLayoutTable.end())
		{
			vkDestroyPipelineLayout(LogicalDevice, GPipelineLayoutTable[TypeIndexCache], nullptr);
			GPipelineLayoutTable.erase(TypeIndexCache);
		}
	}

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

UHShader* UHShaderClass::GetPixelShader() const
{
	return ShaderPS;
}

UHShader* UHShaderClass::GetRayGenShader() const
{
	return RayGenShader;
}

UHShader* UHShaderClass::GetClosestShader() const
{
	return ClosestHitShader;
}

UHShader* UHShaderClass::GetAnyHitShader() const
{
	return AnyHitShader;
}

UHGraphicState* UHShaderClass::GetState() const
{
	return (MaterialCache) ? GGraphicStateTable[MaterialCache->GetId()] : GGraphicStateTable[GetId()];
}

UHGraphicState* UHShaderClass::GetRTState() const
{
	return RTState;
}

UHRenderBuffer<UHShaderRecord>* UHShaderClass::GetRayGenTable() const
{
	return RayGenTable.get();
}

UHRenderBuffer<UHShaderRecord>* UHShaderClass::GetHitGroupTable() const
{
	return HitGroupTable.get();
}

VkDescriptorSetLayout UHShaderClass::GetDescriptorSetLayout() const
{
	return (MaterialCache) ? GMaterialSetLayoutTable[MaterialCache->GetId()] : GSetLayoutTable[TypeIndexCache];
}

VkPipelineLayout UHShaderClass::GetPipelineLayout() const
{
	return (MaterialCache) ? GMaterialPipelineLayoutTable[MaterialCache->GetId()] : GPipelineLayoutTable[TypeIndexCache];
}

VkDescriptorSet UHShaderClass::GetDescriptorSet(int32_t FrameIdx) const
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
	if (GSetLayoutTable.find(TypeIndexCache) == GSetLayoutTable.end())
	{
		VkDescriptorSetLayoutCreateInfo LayoutInfo{};
		LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		LayoutInfo.bindingCount = static_cast<uint32_t>(LayoutBindings.size());
		LayoutInfo.pBindings = LayoutBindings.data();

		if (vkCreateDescriptorSetLayout(LogicalDevice, &LayoutInfo, nullptr, &GSetLayoutTable[TypeIndexCache]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create descriptor set layout for shader: " + UHUtilities::ToStringW(Name) + L"\n");
		}
	}

	// don't duplicate layout
	if (GPipelineLayoutTable.find(TypeIndexCache) == GPipelineLayoutTable.end())
	{
		// one layout per set
		VkPipelineLayoutCreateInfo PipelineLayoutInfo{};
		PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		PipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(1 + AdditionalLayout.size());

		std::vector<VkDescriptorSetLayout> Layouts = { GSetLayoutTable[TypeIndexCache] };
		if (AdditionalLayout.size() > 0)
		{
			Layouts.insert(Layouts.end(), AdditionalLayout.begin(), AdditionalLayout.end());
		}
		PipelineLayoutInfo.pSetLayouts = Layouts.data();

		if (vkCreatePipelineLayout(LogicalDevice, &PipelineLayoutInfo, nullptr, &GPipelineLayoutTable[TypeIndexCache]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create pipeline layout for shader: " + UHUtilities::ToStringW(Name) + L"\n");
		}
	}
	PipelineLayout = GPipelineLayoutTable[TypeIndexCache];

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

	std::vector<VkDescriptorSetLayout> SetLayouts(GMaxFrameInFlight, GSetLayoutTable[TypeIndexCache]);
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

void UHShaderClass::CreateMaterialDescriptor()
{
	VkDevice LogicalDevice = Gfx->GetLogicalDevice();

	// don't duplicate layout
	if (GMaterialSetLayoutTable.find(MaterialCache->GetId()) == GMaterialSetLayoutTable.end())
	{
		VkDescriptorSetLayoutCreateInfo LayoutInfo{};
		LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		LayoutInfo.bindingCount = static_cast<uint32_t>(LayoutBindings.size());
		LayoutInfo.pBindings = LayoutBindings.data();

		if (vkCreateDescriptorSetLayout(LogicalDevice, &LayoutInfo, nullptr, &GMaterialSetLayoutTable[MaterialCache->GetId()]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create descriptor set layout for shader: " + UHUtilities::ToStringW(Name) + L"\n");
		}
	}

	// don't duplicate layout
	if (GMaterialPipelineLayoutTable.find(MaterialCache->GetId()) == GMaterialPipelineLayoutTable.end())
	{
		// one layout per set
		VkPipelineLayoutCreateInfo PipelineLayoutInfo{};
		PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		PipelineLayoutInfo.setLayoutCount = 1;

		std::vector<VkDescriptorSetLayout> Layouts = { GMaterialSetLayoutTable[MaterialCache->GetId()] };
		PipelineLayoutInfo.pSetLayouts = Layouts.data();

		if (vkCreatePipelineLayout(LogicalDevice, &PipelineLayoutInfo, nullptr, &GMaterialPipelineLayoutTable[MaterialCache->GetId()]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create pipeline layout for shader: " + UHUtilities::ToStringW(Name) + L"\n");
		}
	}
	PipelineLayout = GMaterialPipelineLayoutTable[MaterialCache->GetId()];

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

	std::vector<VkDescriptorSetLayout> SetLayouts(GMaxFrameInFlight, GMaterialSetLayoutTable[MaterialCache->GetId()]);
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