#include "ShaderClass.h"

static std::unordered_map<uint32_t, std::unordered_map<std::type_index, VkDescriptorSetLayout>> GSetLayoutTable;
static std::unordered_map<uint32_t, std::unordered_map<std::type_index, VkPipelineLayout>> GPipelineLayoutTable;
static std::unordered_map<uint32_t, std::unordered_map<std::type_index, VkDescriptorSetLayout>> GMaterialSetLayoutTable;
static std::unordered_map<uint32_t, std::unordered_map<std::type_index, VkPipelineLayout>> GMaterialPipelineLayoutTable;
std::unordered_map<uint32_t, UHGraphicState*> GGraphicStateTable;
std::unordered_map<uint32_t, std::unordered_map<std::type_index, UHGraphicState*>> GMaterialStateTable;
std::unordered_map<uint32_t, UHComputeState*> GComputeStateTable;
#define CLEAR_EMPTY_MAPENTRY(x, y) if (x[y].size() == 0) x.erase(y);

UHShaderClass::UHShaderClass(UHGraphic* InGfx, std::string InName, std::type_index InType, UHMaterial* InMat, VkRenderPass InRenderPass)
	: Gfx(InGfx)
	, Name(InName)
	, TypeIndexCache(InType)
	, MaterialCache(InMat)
	, MaterialPassInfo(UHRenderPassInfo())
	, PipelineLayout(nullptr)
	, DescriptorPool(nullptr)
	, ShaderVS(-1)
	, ShaderPS(-1)
	, ShaderCS(-1)
	, RayGenShader(-1)
	, MissShader(-1)
	, RTState(nullptr)
	, RayGenTable(nullptr)
	, HitGroupTable(nullptr)
	, RenderPassCache(InRenderPass)
	, PushConstantRange(VkPushConstantRange{})
	, bPushDescriptor(false)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		DescriptorSets[Idx] = nullptr;
	}
}

void UHShaderClass::Release(bool bDescriptorOnly)
{
	VkDevice LogicalDevice = Gfx->GetLogicalDevice();

	if (DescriptorPool)
	{
		vkDestroyDescriptorPool(LogicalDevice, DescriptorPool, nullptr);
		DescriptorPool = nullptr;
	}

	if (bDescriptorOnly)
	{
		return;
	}

	// release layout depending on type
	if (MaterialCache != nullptr)
	{
		const uint32_t MaterialID = MaterialCache->GetId();
		if (GMaterialSetLayoutTable.find(MaterialID) != GMaterialSetLayoutTable.end())
		{
			if (GMaterialSetLayoutTable[MaterialID].find(TypeIndexCache) != GMaterialSetLayoutTable[MaterialID].end())
			{
				vkDestroyDescriptorSetLayout(LogicalDevice, GMaterialSetLayoutTable[MaterialID][TypeIndexCache], nullptr);
				GMaterialSetLayoutTable[MaterialID].erase(TypeIndexCache);
				CLEAR_EMPTY_MAPENTRY(GMaterialSetLayoutTable, MaterialID);
			}
		}

		if (GMaterialPipelineLayoutTable.find(MaterialID) != GMaterialPipelineLayoutTable.end())
		{
			if (GMaterialPipelineLayoutTable[MaterialID].find(TypeIndexCache) != GMaterialPipelineLayoutTable[MaterialID].end())
			{
				vkDestroyPipelineLayout(LogicalDevice, GMaterialPipelineLayoutTable[MaterialID][TypeIndexCache], nullptr);
				GMaterialPipelineLayoutTable[MaterialID].erase(TypeIndexCache);
				CLEAR_EMPTY_MAPENTRY(GMaterialPipelineLayoutTable, MaterialID);
			}
		}

		// remove state from lookup table
		if (GMaterialStateTable.find(MaterialID) != GMaterialStateTable.end())
		{
			if (GMaterialStateTable[MaterialID].find(TypeIndexCache) != GMaterialStateTable[MaterialID].end())
			{
				Gfx->RequestReleaseGraphicState(GMaterialStateTable[MaterialID][TypeIndexCache]);
				GMaterialStateTable[MaterialID].erase(TypeIndexCache);
				CLEAR_EMPTY_MAPENTRY(GMaterialStateTable, MaterialID);
			}
		}

		// release pixel shader
		Gfx->RequestReleaseShader(ShaderPS);
		MaterialCache = nullptr;
	}
	else
	{
		const uint32_t ShaderID = GetId();
		if (GSetLayoutTable.find(ShaderID) != GSetLayoutTable.end())
		{
			if (GSetLayoutTable[ShaderID].find(TypeIndexCache) != GSetLayoutTable[ShaderID].end())
			{
				vkDestroyDescriptorSetLayout(LogicalDevice, GSetLayoutTable[ShaderID][TypeIndexCache], nullptr);
				GSetLayoutTable[ShaderID].erase(TypeIndexCache);
				CLEAR_EMPTY_MAPENTRY(GSetLayoutTable, ShaderID);
			}
		}

		if (GPipelineLayoutTable.find(ShaderID) != GPipelineLayoutTable.end())
		{
			if (GPipelineLayoutTable[ShaderID].find(TypeIndexCache) != GPipelineLayoutTable[ShaderID].end())
			{
				vkDestroyPipelineLayout(LogicalDevice, GPipelineLayoutTable[ShaderID][TypeIndexCache], nullptr);
				GPipelineLayoutTable[ShaderID].erase(TypeIndexCache);
				CLEAR_EMPTY_MAPENTRY(GPipelineLayoutTable, ShaderID);
			}
		}

		if (GGraphicStateTable.find(ShaderID) != GGraphicStateTable.end())
		{
			Gfx->RequestReleaseGraphicState(GGraphicStateTable[ShaderID]);
			GGraphicStateTable.erase(ShaderID);
		}

		if (GComputeStateTable.find(ShaderID) != GComputeStateTable.end())
		{
			Gfx->RequestReleaseGraphicState(GComputeStateTable[ShaderID]);
			GComputeStateTable.erase(ShaderID);
		}
	}

	if (RTState != nullptr)
	{
		Gfx->RequestReleaseGraphicState(RTState);
		RTState = nullptr;
	}

	UH_SAFE_RELEASE(RayGenTable);
	RayGenTable.reset();

	UH_SAFE_RELEASE(MissTable);
	MissTable.reset();

	UH_SAFE_RELEASE(HitGroupTable);
	HitGroupTable.reset();

	PushConstantRange = VkPushConstantRange{};
	bPushDescriptor = false;
}

void UHShaderClass::BindImage(const UHTexture* InImage, int32_t DstBinding, int32_t CurrentFrameRT, bool bIsReadWrite, int32_t MipIdx)
{
	if (CurrentFrameRT < 0)
	{
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
			if (InImage)
			{
				Helper.WriteImage(InImage, DstBinding, bIsReadWrite, MipIdx);
			}
		}
	}
	else
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[CurrentFrameRT]);
		if (InImage)
		{
			Helper.WriteImage(InImage, DstBinding, bIsReadWrite, MipIdx);
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

void UHShaderClass::PushImage(const UHTexture* InImage, int32_t DstBinding, bool bIsReadWrite, int32_t MipIdx)
{
	VkDescriptorImageInfo ImageInfo{};
	ImageInfo.imageLayout = bIsReadWrite ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ImageInfo.imageView = (MipIdx == UHINDEXNONE) ? InImage->GetImageView() : InImage->GetImageView(MipIdx);
	PushImageInfos.push_back(ImageInfo);

	VkWriteDescriptorSet WriteSet{};
	WriteSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	WriteSet.descriptorType = bIsReadWrite ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	WriteSet.dstBinding = DstBinding;
	WriteSet.descriptorCount = 1;

	PushDescriptorSets.push_back(WriteSet);
}

void UHShaderClass::FlushPushDescriptor(VkCommandBuffer InCmdList)
{
	// link the pImageInfo & pBufferInfo
	int32_t ImageInfoIdx = 0;
	int32_t BufferInfoIdx = 0;

	for (size_t Idx = 0; Idx < PushDescriptorSets.size(); Idx++)
	{
		if (PushDescriptorSets[Idx].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
		{
			PushDescriptorSets[Idx].pBufferInfo = &PushBufferInfos[BufferInfoIdx++];
		}
		else if (PushDescriptorSets[Idx].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || PushDescriptorSets[Idx].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		{
			PushDescriptorSets[Idx].pImageInfo = &PushImageInfos[ImageInfoIdx++];
		}
	}

	GVkCmdPushDescriptorSetKHR(InCmdList, VK_PIPELINE_BIND_POINT_COMPUTE, PipelineLayout, 0, (uint32_t)PushDescriptorSets.size(), PushDescriptorSets.data());

	PushDescriptorSets.clear();
	PushImageInfos.clear();
	PushBufferInfos.clear();
}

void UHShaderClass::BindRWImage(const UHTexture* InImage, int32_t DstBinding, int32_t MipIdx)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
		if (InImage)
		{
			Helper.WriteImage(InImage, DstBinding, true, MipIdx);
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

void UHShaderClass::BindTLAS(const UHAccelerationStructure* InTopAS, int32_t DstBinding, int32_t CurrentFrameRT)
{
	UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[CurrentFrameRT]);
	if (InTopAS)
	{
		Helper.WriteTLAS(InTopAS, DstBinding);
	}
}

void UHShaderClass::RecreateMaterialState()
{
	// remove old state from lookup table
	const uint32_t MaterialID = MaterialCache->GetId();
	if (GMaterialStateTable.find(MaterialID) != GMaterialStateTable.end())
	{
		if (GMaterialStateTable[MaterialID].find(TypeIndexCache) != GMaterialStateTable[MaterialID].end())
		{
			Gfx->RequestReleaseGraphicState(GMaterialStateTable[MaterialID][TypeIndexCache]);
			GMaterialStateTable[MaterialID].erase(TypeIndexCache);
		}
	}

	UHRenderPassInfo PrevPassInfo = MaterialPassInfo;
	MaterialPassInfo = UHRenderPassInfo(PrevPassInfo.RenderPass
		, PrevPassInfo.DepthInfo
		, MaterialCache->GetCullMode()
		, MaterialCache->GetBlendMode()
		, ShaderVS
		, ShaderPS
		, PrevPassInfo.RTCount
		, PipelineLayout);

	MaterialPassInfo.bForceBlendOff = PrevPassInfo.bForceBlendOff;

	CreateMaterialState(MaterialPassInfo);
}

void UHShaderClass::SetNewMaterialCache(UHMaterial* InMat)
{
	MaterialCache = InMat;
}

void UHShaderClass::SetNewRenderPass(VkRenderPass InRenderPass)
{
	RenderPassCache = InRenderPass;
}

uint32_t UHShaderClass::GetRayGenShader() const
{
	return RayGenShader;
}

std::vector<uint32_t> UHShaderClass::GetClosestShaders() const
{
	return ClosestHitShaders;
}

std::vector<uint32_t> UHShaderClass::GetAnyHitShaders() const
{
	return AnyHitShaders;
}

uint32_t UHShaderClass::GetMissShader() const
{
	return MissShader;
}

UHGraphicState* UHShaderClass::GetState() const
{
	if (MaterialCache)
	{
		const uint32_t MatId = MaterialCache->GetId();
		if (GMaterialStateTable.find(MatId) != GMaterialStateTable.end())
		{
			if (GMaterialStateTable[MatId].find(TypeIndexCache) != GMaterialStateTable[MatId].end())
			{
				return GMaterialStateTable[MaterialCache->GetId()][TypeIndexCache];
			}
		}

		return nullptr;
	}

	return GGraphicStateTable[GetId()];
}

UHGraphicState* UHShaderClass::GetRTState() const
{
	return RTState;
}

UHComputeState* UHShaderClass::GetComputeState() const
{
	return GComputeStateTable[GetId()];
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
	return (MaterialCache) ? GMaterialSetLayoutTable[MaterialCache->GetId()][TypeIndexCache] : GSetLayoutTable[GetId()][TypeIndexCache];
}

VkPipelineLayout UHShaderClass::GetPipelineLayout() const
{
	return (MaterialCache) ? GMaterialPipelineLayoutTable[MaterialCache->GetId()][TypeIndexCache] : GPipelineLayoutTable[GetId()][TypeIndexCache];
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
	if (GSetLayoutTable[GetId()].find(TypeIndexCache) == GSetLayoutTable[GetId()].end())
	{
		VkDescriptorSetLayoutCreateInfo LayoutInfo{};
		LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		LayoutInfo.bindingCount = static_cast<uint32_t>(LayoutBindings.size());
		LayoutInfo.pBindings = LayoutBindings.data();

		if (bPushDescriptor)
		{
			LayoutInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		}

		if (vkCreateDescriptorSetLayout(LogicalDevice, &LayoutInfo, nullptr, &GSetLayoutTable[GetId()][TypeIndexCache]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create descriptor set layout for shader: " + UHUtilities::ToStringW(Name) + L"\n");
		}
	}

	// don't duplicate layout
	if (GPipelineLayoutTable[GetId()].find(TypeIndexCache) == GPipelineLayoutTable[GetId()].end())
	{
		// one layout per set
		VkPipelineLayoutCreateInfo PipelineLayoutInfo{};
		PipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		PipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(1 + AdditionalLayout.size());

		// Setup push constant range if requested, for now count 1 should suffice
		if (PushConstantRange.stageFlags != 0)
		{
			PipelineLayoutInfo.pushConstantRangeCount = 1;
			PipelineLayoutInfo.pPushConstantRanges = &PushConstantRange;
		}

		std::vector<VkDescriptorSetLayout> Layouts = { GSetLayoutTable[GetId()][TypeIndexCache] };
		if (AdditionalLayout.size() > 0)
		{
			Layouts.insert(Layouts.end(), AdditionalLayout.begin(), AdditionalLayout.end());
		}
		PipelineLayoutInfo.pSetLayouts = Layouts.data();

		if (vkCreatePipelineLayout(LogicalDevice, &PipelineLayoutInfo, nullptr, &GPipelineLayoutTable[GetId()][TypeIndexCache]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create pipeline layout for shader: " + UHUtilities::ToStringW(Name) + L"\n");
		}
	}
	PipelineLayout = GPipelineLayoutTable[GetId()][TypeIndexCache];

	// create DescriptorPools
	std::vector<VkDescriptorPoolSize> PoolSizes;

	// pool type can be single or multiple
	for (size_t Idx = 0; Idx < LayoutBindings.size(); Idx++)
	{
		VkDescriptorPoolSize PoolSize{};
		PoolSize.type = LayoutBindings[Idx].descriptorType;
		// number of this count needs to be [Count * GMaxFrameInFlight]
		PoolSize.descriptorCount = LayoutBindings[Idx].descriptorCount * GMaxFrameInFlight;
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

	if (bPushDescriptor)
	{
		// skip allocating descriptor if it's pushing
		return;
	}

	std::vector<VkDescriptorSetLayout> SetLayouts(GMaxFrameInFlight, GSetLayoutTable[GetId()][TypeIndexCache]);
	VkDescriptorSetAllocateInfo AllocInfo{};
	AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	AllocInfo.descriptorPool = DescriptorPool;
	AllocInfo.descriptorSetCount = GMaxFrameInFlight;
	AllocInfo.pSetLayouts = SetLayouts.data();

	const VkResult Result = vkAllocateDescriptorSets(LogicalDevice, &AllocInfo, DescriptorSets.data());
	if (Result != VK_SUCCESS)
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
		// number of this count needs to be [Count * GMaxFrameInFlight]
		PoolSize.descriptorCount = LayoutBindings[Idx].descriptorCount * GMaxFrameInFlight;
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

void UHShaderClass::CreateComputeState(UHComputePassInfo InInfo)
{
	// prevent duplicate creating
	if (GComputeStateTable.find(GetId()) == GComputeStateTable.end())
	{
		GComputeStateTable[GetId()] = Gfx->RequestComputeState(InInfo);
	}
}

void UHShaderClass::InitRayGenTable()
{
	std::vector<BYTE> TempData(Gfx->GetShaderRecordSize());
	if (GVkGetRayTracingShaderGroupHandlesKHR(Gfx->GetLogicalDevice(), RTState->GetRTPipeline(), GRayGenTableSlot, 1, Gfx->GetShaderRecordSize(), TempData.data()) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to get ray gen group handle!\n");
	}

	RayGenTable = Gfx->RequestRenderBuffer<UHShaderRecord>(1, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	RayGenTable->UploadAllData(TempData.data());
}

void UHShaderClass::InitMissTable()
{
	std::vector<BYTE> TempData(Gfx->GetShaderRecordSize());

	// get data for HG as well
	if (GVkGetRayTracingShaderGroupHandlesKHR(Gfx->GetLogicalDevice(), RTState->GetRTPipeline(), GMissTableSlot, 1, Gfx->GetShaderRecordSize(), TempData.data()) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to get hit group handle!\n");
	}

	// copy HG records to the buffer, both closest hit and any hit will be put in the same hit group
	MissTable = Gfx->RequestRenderBuffer<UHShaderRecord>(1, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	MissTable->UploadAllData(TempData.data());
}

void UHShaderClass::InitHitGroupTable(size_t NumMaterials)
{
	std::vector<BYTE> TempData(Gfx->GetShaderRecordSize());

	// get data for HG as well
	// create HG buffer
	HitGroupTable = Gfx->RequestRenderBuffer<UHShaderRecord>(NumMaterials, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

	for (size_t Idx = 0; Idx < NumMaterials; Idx++)
	{
		// copy hit group
		if (GVkGetRayTracingShaderGroupHandlesKHR(Gfx->GetLogicalDevice(), RTState->GetRTPipeline(), GHitGroupTableSlot + static_cast<uint32_t>(Idx), 1, Gfx->GetShaderRecordSize(), TempData.data()) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to get hit group handle!\n");
			continue;
		}

		// copy HG records to the buffer, both closest hit and any hit will be put in the same hit group
		HitGroupTable->UploadData(TempData.data(), Idx);
	}
}