#include "ShaderClass.h"
#include "Runtime/Engine/Graphic.h"

// layout won't change for the same shaders and can be cached
std::unordered_map<std::type_index, VkDescriptorSetLayout> UHShaderClass::DescriptorSetLayoutTable;
std::unordered_map<std::type_index, VkPipelineLayout> UHShaderClass::PipelineLayoutTable;

// graphic state table, the material is a bit more complicated since it could be involded in different passes
std::unordered_map<uint32_t, UHGraphicState*> UHShaderClass::GraphicStateTable;
std::unordered_map<uint32_t, std::unordered_map<std::type_index, UHGraphicState*>> UHShaderClass::MaterialStateTable;
std::unordered_map<uint32_t, UHComputeState*> UHShaderClass::ComputeStateTable;

#define CLEAR_EMPTY_MAPENTRY(x, y) if (x[y].size() == 0) x.erase(y);

void UHShaderClass::ClearGlobalLayoutCache(UHGraphic* InGfx)
{
	for (auto& SetLayout : DescriptorSetLayoutTable)
	{
		vkDestroyDescriptorSetLayout(InGfx->GetLogicalDevice(), SetLayout.second, nullptr);
	}

	for (auto& PipelineLayout : PipelineLayoutTable)
	{
		vkDestroyPipelineLayout(InGfx->GetLogicalDevice(), PipelineLayout.second, nullptr);
	}

	DescriptorSetLayoutTable.clear();
	PipelineLayoutTable.clear();
}

bool UHShaderClass::IsDescriptorLayoutValid(std::type_index InType)
{
	return DescriptorSetLayoutTable.find(InType) != DescriptorSetLayoutTable.end();
}

UHShaderClass::UHShaderClass(UHGraphic* InGfx, std::string InName, std::type_index InType, UHMaterial* InMat, VkRenderPass InRenderPass)
	: Gfx(InGfx)
	, TypeIndexCache(InType)
	, MaterialCache(InMat)
	, MaterialPassInfo(UHRenderPassInfo())
	, PipelineLayout(nullptr)
	, DescriptorPool(nullptr)
	, ShaderVS(UHINDEXNONE)
	, ShaderPS(UHINDEXNONE)
	, ShaderCS(UHINDEXNONE)
	, RayGenShader(UHINDEXNONE)
	, MissShader(UHINDEXNONE)
	, ShaderAS(UHINDEXNONE)
	, ShaderMS(UHINDEXNONE)
	, RTState(nullptr)
	, RayGenTable(nullptr)
	, HitGroupTable(nullptr)
	, RenderPassCache(InRenderPass)
	, PushConstantRange(VkPushConstantRange{})
	, bPushDescriptor(false)
{
	Name = InName;
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		DescriptorSets[Idx] = nullptr;
	}
}

void UHShaderClass::Release()
{
	VkDevice LogicalDevice = Gfx->GetLogicalDevice();

	ReleaseDescriptor();

	// release layout depending on type
	if (MaterialCache != nullptr)
	{
		const uint32_t MaterialID = MaterialCache->GetId();

		// remove state from lookup table
		if (MaterialStateTable.find(MaterialID) != MaterialStateTable.end())
		{
			if (MaterialStateTable[MaterialID].find(TypeIndexCache) != MaterialStateTable[MaterialID].end())
			{
				Gfx->RequestReleaseGraphicState(MaterialStateTable[MaterialID][TypeIndexCache]);
				MaterialStateTable[MaterialID].erase(TypeIndexCache);
				CLEAR_EMPTY_MAPENTRY(MaterialStateTable, MaterialID);
			}
		}

		// release pixel shader
		Gfx->RequestReleaseShader(ShaderPS);
		MaterialCache = nullptr;
	}
	else
	{
		const uint32_t ShaderID = GetId();

		if (GraphicStateTable.find(ShaderID) != GraphicStateTable.end())
		{
			Gfx->RequestReleaseGraphicState(GraphicStateTable[ShaderID]);
			GraphicStateTable.erase(ShaderID);
		}

		if (ComputeStateTable.find(ShaderID) != ComputeStateTable.end())
		{
			Gfx->RequestReleaseGraphicState(ComputeStateTable[ShaderID]);
			ComputeStateTable.erase(ShaderID);
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

// function to release descriptor only
void UHShaderClass::ReleaseDescriptor()
{
	VkDevice LogicalDevice = Gfx->GetLogicalDevice();

	if (DescriptorPool)
	{
		vkDestroyDescriptorPool(LogicalDevice, DescriptorPool, nullptr);
		DescriptorPool = nullptr;
	}
}

void UHShaderClass::BindImage(const UHTexture* InImage, const int32_t DstBinding)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
		if (InImage)
		{
			Helper.WriteImage(InImage, DstBinding, false, UHINDEXNONE);
		}
	}
}

void UHShaderClass::BindUavAsSrv(const UHTexture* InImage, const int32_t DstBinding)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
		if (InImage)
		{
			Helper.WriteImage(InImage, DstBinding, false, UHINDEXNONE, UHINDEXNONE, true);
		}
	}
}

void UHShaderClass::BindImage(const UHTexture* InImage, const int32_t DstBinding, const int32_t LayerIdx)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
		if (InImage)
		{
			Helper.WriteImage(InImage, DstBinding, false, UHINDEXNONE, LayerIdx);
		}
	}
}

void UHShaderClass::BindImage(const UHTexture* InImage, const int32_t DstBinding
	, const int32_t CurrentFrameRT, const bool bIsReadWrite, const int32_t MipIdx)
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

void UHShaderClass::BindImage(const std::vector<UHTexture*> InImages, const int32_t DstBinding)
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

void UHShaderClass::PushImage(const UHTexture* InImage, const int32_t DstBinding
	, const bool bIsReadWrite, const int32_t MipIdx, const int32_t LayerIdx, bool bUavAsSrv)
{
	VkDescriptorImageInfo ImageInfo{};
	ImageInfo.imageLayout = bIsReadWrite || bUavAsSrv ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// fetch the proper image view
	if (LayerIdx != UHINDEXNONE)
	{
		ImageInfo.imageView = InImage->GetImageViewPerLayer(LayerIdx);
	}
	else if (MipIdx != UHINDEXNONE)
	{
		ImageInfo.imageView = InImage->GetImageViewPerMip(MipIdx);
	}
	else
	{
		ImageInfo.imageView = InImage->GetImageView();
	}

	PushImageInfos.push_back(ImageInfo);

	VkWriteDescriptorSet WriteSet{};
	WriteSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	WriteSet.descriptorType = bIsReadWrite && !bUavAsSrv ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	WriteSet.dstBinding = DstBinding;
	WriteSet.descriptorCount = 1;

	PushDescriptorSets.push_back(WriteSet);
}

void UHShaderClass::PushSampler(const UHSampler* InSampler, const int32_t DstBinding)
{
	VkDescriptorImageInfo ImageInfo{};
	ImageInfo.sampler = InSampler->GetSampler();
	PushImageInfos.push_back(ImageInfo);

	VkWriteDescriptorSet WriteSet{};
	WriteSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	WriteSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
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
		else if (PushDescriptorSets[Idx].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE 
			|| PushDescriptorSets[Idx].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
			|| PushDescriptorSets[Idx].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER)
		{
			PushDescriptorSets[Idx].pImageInfo = &PushImageInfos[ImageInfoIdx++];
		}
	}

	GVkCmdPushDescriptorSetKHR(InCmdList, VK_PIPELINE_BIND_POINT_COMPUTE, PipelineLayout, 0, (uint32_t)PushDescriptorSets.size(), PushDescriptorSets.data());

	PushDescriptorSets.clear();
	PushImageInfos.clear();
	PushBufferInfos.clear();
}

// bind RW image
void UHShaderClass::BindRWImage(const UHTexture* InImage, const int32_t DstBinding)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
		if (InImage)
		{
			Helper.WriteImage(InImage, DstBinding, true, UHINDEXNONE);
		}
	}
}

// bind RW image, accept array input
void UHShaderClass::BindRWImage(const std::vector<UHRenderTexture*> InImages, const int32_t DstBinding)
{
	for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
		if (InImages.size() > 0)
		{
			const std::vector<UHTexture*> Inputs(InImages.begin(), InImages.end());
			Helper.WriteImage(Inputs, DstBinding, true);
		}
	}
}

// bind RW image with mip index
void UHShaderClass::BindRWImage(const UHTexture* InImage, const int32_t DstBinding, const int32_t MipIdx)
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

void UHShaderClass::BindSampler(const UHSampler* InSampler, const int32_t DstBinding)
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

void UHShaderClass::BindSampler(const std::vector<UHSampler*>& InSamplers, const int32_t DstBinding)
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

void UHShaderClass::BindTLAS(const UHAccelerationStructure* InTopAS, const int32_t DstBinding, const int32_t CurrentFrameRT)
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
	if (MaterialStateTable.find(MaterialID) != MaterialStateTable.end())
	{
		if (MaterialStateTable[MaterialID].find(TypeIndexCache) != MaterialStateTable[MaterialID].end())
		{
			Gfx->RequestReleaseGraphicState(MaterialStateTable[MaterialID][TypeIndexCache]);
			MaterialStateTable[MaterialID].erase(TypeIndexCache);
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
	MaterialPassInfo.AS = ShaderAS;
	MaterialPassInfo.MS = ShaderMS;
	MaterialPassInfo.bIsIntegerBuffer = PrevPassInfo.bIsIntegerBuffer;

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

const std::vector<uint32_t>& UHShaderClass::GetClosestShaders() const
{
	return ClosestHitShaders;
}

const std::vector<uint32_t>& UHShaderClass::GetAnyHitShaders() const
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
		if (MaterialStateTable.find(MatId) != MaterialStateTable.end())
		{
			if (MaterialStateTable[MatId].find(TypeIndexCache) != MaterialStateTable[MatId].end())
			{
				return MaterialStateTable[MaterialCache->GetId()][TypeIndexCache];
			}
		}

		return nullptr;
	}

	return GraphicStateTable[GetId()];
}

UHGraphicState* UHShaderClass::GetRTState() const
{
	return RTState;
}

UHComputeState* UHShaderClass::GetComputeState() const
{
	return ComputeStateTable[GetId()];
}

UHMaterial* UHShaderClass::GetMaterialCache() const
{
	return MaterialCache;
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
	return DescriptorSetLayoutTable[TypeIndexCache];
}

VkPipelineLayout UHShaderClass::GetPipelineLayout() const
{
	return PipelineLayoutTable[TypeIndexCache];
}

VkDescriptorSet UHShaderClass::GetDescriptorSet(int32_t FrameIdx) const
{
	return DescriptorSets[FrameIdx];
}

const VkPushConstantRange& UHShaderClass::GetPushConstantRange() const
{
	return PushConstantRange;
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

void UHShaderClass::CreateLayoutAndDescriptor(std::vector<VkDescriptorSetLayout> AdditionalLayout)
{
	VkDevice LogicalDevice = Gfx->GetLogicalDevice();

	// don't duplicate layout
	if (DescriptorSetLayoutTable.find(TypeIndexCache) == DescriptorSetLayoutTable.end())
	{
		VkDescriptorSetLayoutCreateInfo LayoutInfo{};
		LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		LayoutInfo.bindingCount = static_cast<uint32_t>(LayoutBindings.size());
		LayoutInfo.pBindings = LayoutBindings.data();

		if (bPushDescriptor)
		{
			LayoutInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		}

		if (vkCreateDescriptorSetLayout(LogicalDevice, &LayoutInfo, nullptr, &DescriptorSetLayoutTable[TypeIndexCache]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create descriptor set layout for shader: " + UHUtilities::ToStringW(Name) + L"\n");
		}

#if WITH_EDITOR
		Gfx->SetDebugUtilsObjectName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)DescriptorSetLayoutTable[TypeIndexCache]
			, Name + "_DescriptorSetLayout");
#endif
	}

	// don't duplicate layout
	if (PipelineLayoutTable.find(TypeIndexCache) == PipelineLayoutTable.end())
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

		std::vector<VkDescriptorSetLayout> Layouts = { DescriptorSetLayoutTable[TypeIndexCache] };
		if (AdditionalLayout.size() > 0)
		{
			Layouts.insert(Layouts.end(), AdditionalLayout.begin(), AdditionalLayout.end());
		}
		PipelineLayoutInfo.pSetLayouts = Layouts.data();

		if (vkCreatePipelineLayout(LogicalDevice, &PipelineLayoutInfo, nullptr, &PipelineLayoutTable[TypeIndexCache]) != VK_SUCCESS)
		{
			UHE_LOG(L"Failed to create pipeline layout for shader: " + UHUtilities::ToStringW(Name) + L"\n");
		}

#if WITH_EDITOR
		Gfx->SetDebugUtilsObjectName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)PipelineLayoutTable[TypeIndexCache]
			, Name + "_PipelineLayout");
#endif
	}
	PipelineLayout = PipelineLayoutTable[TypeIndexCache];

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

#if WITH_EDITOR
	Gfx->SetDebugUtilsObjectName(VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)DescriptorPool
		, Name + "_DescriptorPool");
#endif

	if (bPushDescriptor)
	{
		// skip allocating descriptor if it's pushing
		return;
	}

	std::vector<VkDescriptorSetLayout> SetLayouts(GMaxFrameInFlight, DescriptorSetLayoutTable[TypeIndexCache]);
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

// create graphic state functions
void UHShaderClass::CreateGraphicState(UHRenderPassInfo InInfo)
{
	// prevent duplicate creating
	if (GraphicStateTable.find(GetId()) == GraphicStateTable.end())
	{
		GraphicStateTable[GetId()] = Gfx->RequestGraphicState(InInfo);
	}
}

void UHShaderClass::CreateMaterialState(UHRenderPassInfo InInfo)
{
	// prevent duplicating
	uint32_t MaterialID = MaterialCache->GetId();

	if (MaterialStateTable.find(MaterialID) == MaterialStateTable.end()
		|| MaterialStateTable[MaterialID].find(TypeIndexCache) == MaterialStateTable[MaterialID].end())
	{
		MaterialStateTable[MaterialID][TypeIndexCache] = Gfx->RequestGraphicState(InInfo);
	}
}

void UHShaderClass::CreateComputeState(UHComputePassInfo InInfo)
{
	// prevent duplicate creating
	if (ComputeStateTable.find(GetId()) == ComputeStateTable.end())
	{
		ComputeStateTable[GetId()] = Gfx->RequestComputeState(InInfo);
	}
}

void UHShaderClass::InitRayGenTable()
{
	std::vector<BYTE> TempData(Gfx->GetShaderRecordSize());
	if (GVkGetRayTracingShaderGroupHandlesKHR(Gfx->GetLogicalDevice(), RTState->GetRTPipeline(), GRayGenTableSlot, 1, Gfx->GetShaderRecordSize(), TempData.data()) != VK_SUCCESS)
	{
		UHE_LOG(L"Failed to get ray gen group handle!\n");
	}

	RayGenTable = Gfx->RequestRenderBuffer<UHShaderRecord>(1, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		, Name + "_RayGenTable");
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
	MissTable = Gfx->RequestRenderBuffer<UHShaderRecord>(1, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		, Name + "_MissTable");
	MissTable->UploadAllData(TempData.data());
}

void UHShaderClass::InitHitGroupTable(size_t NumMaterials)
{
	std::vector<BYTE> TempData(Gfx->GetShaderRecordSize());

	// get data for HG as well
	// create HG buffer
	HitGroupTable = Gfx->RequestRenderBuffer<UHShaderRecord>(NumMaterials, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		, Name + "_HitGroupTable");

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