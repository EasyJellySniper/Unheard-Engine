#include "ShaderClass.h"

static std::unordered_map<std::type_index, VkDescriptorSetLayout> GSetLayoutTable;
static std::unordered_map<std::type_index, VkPipelineLayout> GPipelineLayoutTable;
static std::unordered_map<uint32_t, std::unordered_map<std::type_index, VkDescriptorSetLayout>> GMaterialSetLayoutTable;
static std::unordered_map<uint32_t, std::unordered_map<std::type_index, VkPipelineLayout>> GMaterialPipelineLayoutTable;
std::unordered_map<uint32_t, UHGraphicState*> GGraphicStateTable;
std::unordered_map<uint32_t, std::unordered_map<std::type_index, UHGraphicState*>> GMaterialStateTable;

UHShaderClass::UHShaderClass()
	: UHShaderClass(nullptr, "", typeid(UHShaderClass), nullptr)
{

}

UHShaderClass::UHShaderClass(UHGraphic* InGfx, std::string InName, std::type_index InType, UHMaterial* InMat)
	: Gfx(InGfx)
	, Name(InName)
	, TypeIndexCache(InType)
	, MaterialCache(InMat)
	, PipelineLayout(VK_NULL_HANDLE)
	, DescriptorPool(VK_NULL_HANDLE)
	, ShaderVS(nullptr)
	, ShaderPS(nullptr)
	, RayGenShader(nullptr)
	, ClosestHitShader(nullptr)
	, MissShader(nullptr)
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

	// release layout depending on type
	if (MaterialCache != nullptr)
	{
		uint32_t MaterialID = MaterialCache->GetId();
		if (GMaterialSetLayoutTable.find(MaterialID) != GMaterialSetLayoutTable.end())
		{
			if (GMaterialSetLayoutTable[MaterialID].find(TypeIndexCache) != GMaterialSetLayoutTable[MaterialID].end())
			{
				vkDestroyDescriptorSetLayout(LogicalDevice, GMaterialSetLayoutTable[MaterialID][TypeIndexCache], nullptr);
				GMaterialSetLayoutTable[MaterialID].erase(TypeIndexCache);
			}
		}

		if (GMaterialPipelineLayoutTable.find(MaterialID) != GMaterialPipelineLayoutTable.end())
		{
			if (GMaterialPipelineLayoutTable[MaterialID].find(TypeIndexCache) != GMaterialPipelineLayoutTable[MaterialID].end())
			{
				vkDestroyPipelineLayout(LogicalDevice, GMaterialPipelineLayoutTable[MaterialID][TypeIndexCache], nullptr);
				GMaterialPipelineLayoutTable[MaterialID].erase(TypeIndexCache);
			}
		}

		// remove state from lookup table
		if (GMaterialStateTable.find(MaterialID) != GMaterialStateTable.end())
		{
			if (GMaterialStateTable[MaterialID].find(TypeIndexCache) != GMaterialStateTable[MaterialID].end())
			{
				Gfx->RequestReleaseGraphicState(GMaterialStateTable[MaterialID][TypeIndexCache]);
				GMaterialStateTable[MaterialID].erase(TypeIndexCache);
			}
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

	if (RTState != nullptr)
	{
		Gfx->RequestReleaseGraphicState(RTState);
	}

	UH_SAFE_RELEASE(RayGenTable);
	RayGenTable.reset();

	UH_SAFE_RELEASE(MissTable);
	MissTable.reset();

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

std::vector<UHShader*> UHShaderClass::GetAnyHitShaders() const
{
	return AnyHitShaders;
}

UHShader* UHShaderClass::GetMissShader() const
{
	return MissShader;
}

UHGraphicState* UHShaderClass::GetState() const
{
	return (MaterialCache) ? GMaterialStateTable[MaterialCache->GetId()][TypeIndexCache] : GGraphicStateTable[GetId()];
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

UHRenderBuffer<UHShaderRecord>* UHShaderClass::GetMissTable() const
{
	return MissTable.get();
}

VkDescriptorSetLayout UHShaderClass::GetDescriptorSetLayout() const
{
	return (MaterialCache) ? GMaterialSetLayoutTable[MaterialCache->GetId()][TypeIndexCache] : GSetLayoutTable[TypeIndexCache];
}

VkPipelineLayout UHShaderClass::GetPipelineLayout() const
{
	return (MaterialCache) ? GMaterialPipelineLayoutTable[MaterialCache->GetId()][TypeIndexCache] : GPipelineLayoutTable[TypeIndexCache];
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

void UHShaderClass::CreateMaterialDescriptor(std::vector<VkDescriptorSetLayout> AdditionalLayouts)
{
	VkDevice LogicalDevice = Gfx->GetLogicalDevice();
	uint32_t MaterialID = MaterialCache->GetId();

	// don't duplicate layout
	if (GMaterialSetLayoutTable.find(MaterialID) == GMaterialSetLayoutTable.end()
		|| GMaterialSetLayoutTable[MaterialID].find(TypeIndexCache) == GMaterialSetLayoutTable[MaterialID].end())
	{
		VkDescriptorSetLayoutCreateInfo LayoutInfo{};
		LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		LayoutInfo.bindingCount = static_cast<uint32_t>(LayoutBindings.size());
		LayoutInfo.pBindings = LayoutBindings.data();

		if (vkCreateDescriptorSetLayout(LogicalDevice, &LayoutInfo, nullptr, &GMaterialSetLayoutTable[MaterialID][TypeIndexCache]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create descriptor set layout for shader: " + UHUtilities::ToStringW(Name) + L"\n");
		}
	}

	// don't duplicate layout
	if (GMaterialPipelineLayoutTable.find(MaterialID) == GMaterialPipelineLayoutTable.end()
		|| GMaterialPipelineLayoutTable[MaterialID].find(TypeIndexCache) == GMaterialPipelineLayoutTable[MaterialID].end())
	{
		// one layout per set
		VkPipelineLayoutCreateInfo PipelineLayoutInfo{};
		PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		PipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(1 + AdditionalLayouts.size());

		std::vector<VkDescriptorSetLayout> Layouts = { GMaterialSetLayoutTable[MaterialID][TypeIndexCache] };
		if (AdditionalLayouts.size() > 0)
		{
			Layouts.insert(Layouts.end(), AdditionalLayouts.begin(), AdditionalLayouts.end());
		}
		PipelineLayoutInfo.pSetLayouts = Layouts.data();

		if (vkCreatePipelineLayout(LogicalDevice, &PipelineLayoutInfo, nullptr, &GMaterialPipelineLayoutTable[MaterialID][TypeIndexCache]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create pipeline layout for shader: " + UHUtilities::ToStringW(Name) + L"\n");
		}
	}
	PipelineLayout = GMaterialPipelineLayoutTable[MaterialID][TypeIndexCache];

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

	std::vector<VkDescriptorSetLayout> SetLayouts(GMaxFrameInFlight, GMaterialSetLayoutTable[MaterialID][TypeIndexCache]);
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

// create graphic state functions
void UHShaderClass::CreateGraphicState(UHRenderPassInfo InInfo)
{
	// prevent duplicate creating
	if (GGraphicStateTable.find(GetId()) == GGraphicStateTable.end())
	{
		GGraphicStateTable[GetId()] = Gfx->RequestGraphicState(InInfo);
	}
}

void UHShaderClass::CreateMaterialState(UHRenderPassInfo InInfo)
{
	// prevent duplicating
	uint32_t MaterialID = MaterialCache->GetId();

	if (GMaterialStateTable.find(MaterialID) == GMaterialStateTable.end()
		|| GMaterialStateTable[MaterialID].find(TypeIndexCache) == GMaterialStateTable[MaterialID].end())
	{
		GMaterialStateTable[MaterialID][TypeIndexCache] = Gfx->RequestGraphicState(InInfo);
	}
}

void UHShaderClass::InitRayGenTable()
{
	std::vector<BYTE> TempData(Gfx->GetShaderRecordSize());

	PFN_vkGetRayTracingShaderGroupHandlesKHR GetRTShaderGroupHandle = (PFN_vkGetRayTracingShaderGroupHandlesKHR)
		vkGetInstanceProcAddr(Gfx->GetInstance(), "vkGetRayTracingShaderGroupHandlesKHR");
	if (GetRTShaderGroupHandle(Gfx->GetLogicalDevice(), RTState->GetRTPipeline(), 0, 1, Gfx->GetShaderRecordSize(), TempData.data()) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to get ray gen group handle!\n");
	}

	RayGenTable = Gfx->RequestRenderBuffer<UHShaderRecord>();
	RayGenTable->CreateBuffer(1, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	RayGenTable->UploadAllData(TempData.data());
}

void UHShaderClass::InitMissTable()
{
	std::vector<BYTE> TempData(Gfx->GetShaderRecordSize());

	// get data for HG as well
	PFN_vkGetRayTracingShaderGroupHandlesKHR GetRTShaderGroupHandle = (PFN_vkGetRayTracingShaderGroupHandlesKHR)
		vkGetInstanceProcAddr(Gfx->GetInstance(), "vkGetRayTracingShaderGroupHandlesKHR");
	if (GetRTShaderGroupHandle(Gfx->GetLogicalDevice(), RTState->GetRTPipeline(), 1, 1, Gfx->GetShaderRecordSize(), TempData.data()) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to get hit group handle!\n");
	}

	// copy HG records to the buffer, both closest hit and any hit will be put in the same hit group
	MissTable = Gfx->RequestRenderBuffer<UHShaderRecord>();
	MissTable->CreateBuffer(1, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	MissTable->UploadAllData(TempData.data());
}

void UHShaderClass::InitHitGroupTable(size_t NumMaterials)
{
	std::vector<BYTE> TempData(Gfx->GetShaderRecordSize());

	// get data for HG as well
	PFN_vkGetRayTracingShaderGroupHandlesKHR GetRTShaderGroupHandle = (PFN_vkGetRayTracingShaderGroupHandlesKHR)
		vkGetInstanceProcAddr(Gfx->GetInstance(), "vkGetRayTracingShaderGroupHandlesKHR");

	// create HG buffer
	HitGroupTable = Gfx->RequestRenderBuffer<UHShaderRecord>();
	HitGroupTable->CreateBuffer(NumMaterials, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

	for (size_t Idx = 0; Idx < NumMaterials; Idx++)
	{
		if (GetRTShaderGroupHandle(Gfx->GetLogicalDevice(), RTState->GetRTPipeline(), 2 + static_cast<uint32_t>(Idx), 1, Gfx->GetShaderRecordSize(), TempData.data()) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to get hit group handle!\n");
			continue;
		}

		// copy HG records to the buffer, both closest hit and any hit will be put in the same hit group
		HitGroupTable->UploadData(TempData.data(), Idx);
	}
}