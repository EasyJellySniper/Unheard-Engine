#pragma once
#include "../../Engine/Graphic.h"
#include "../DescriptorHelper.h"
#include <typeindex>

// 32 byte structure for shader table record
struct UHShaderRecord
{
	BYTE Record[32];
};

// high-level shader class, this will be actually used for rendering
// descriptor is set here too, each shader class is aim for keeping a minium set of descriptors
class UHShaderClass : public UHObject
{
public:
	UHShaderClass(UHGraphic* InGfx, std::string Name, std::type_index InType, UHMaterial* InMat = nullptr, VkRenderPass InRenderPass = nullptr);

	virtual void Release();
	virtual void OnCompile() = 0;
	void ReleaseDescriptor();

	static void ClearGlobalLayoutCache(UHGraphic* InGfx);
	static bool IsDescriptorLayoutValid(std::type_index InType);

	template <typename T>
	void BindConstant(const std::array<UniquePtr<UHRenderBuffer<T>>, GMaxFrameInFlight>& InBuffer, int32_t DstBinding, int32_t InOffset)
	{
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
			if (InBuffer[Idx])
			{
				Helper.WriteConstantBuffer(InBuffer[Idx].get(), DstBinding, InOffset);
			}
		}
	}

	template <typename T>
	void BindConstant(const UniquePtr<UHRenderBuffer<T>> InBuffer[GMaxFrameInFlight], int32_t DstBinding, int32_t InOffset)
	{
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
			if (InBuffer[Idx])
			{
				Helper.WriteConstantBuffer(InBuffer[Idx].get(), DstBinding, InOffset);
			}
		}
	}

	template <typename T>
	void BindConstant(const UniquePtr<UHRenderBuffer<T>>& InBuffer, int32_t DstBinding, int32_t CurrentFrame, int32_t InOffset)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[CurrentFrame]);
		if (InBuffer)
		{
			Helper.WriteConstantBuffer(InBuffer.get(), DstBinding, InOffset);
		}
	}

	template <typename T>
	void PushConstantBuffer(const UniquePtr<UHRenderBuffer<T>>& InBuffer, int32_t DstBinding, int32_t InOffset)
	{
		VkDescriptorBufferInfo NewInfo{};
		NewInfo.buffer = InBuffer->GetBuffer();
		NewInfo.range = InBuffer->GetBufferStride();
		NewInfo.offset = InOffset * InBuffer->GetBufferStride();
		PushBufferInfos.push_back(NewInfo);

		VkWriteDescriptorSet WriteSet{};
		WriteSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		WriteSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		WriteSet.dstBinding = DstBinding;
		WriteSet.descriptorCount = 1;

		PushDescriptorSets.push_back(WriteSet);
	}

	template <typename T>
	void BindStorage(const std::array<UniquePtr<UHRenderBuffer<T>>, GMaxFrameInFlight>& InBuffer, int32_t DstBinding, int32_t InOffset, bool bFullRange)
	{
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
			if (InBuffer[Idx])
			{
				Helper.WriteStorageBuffer(InBuffer[Idx].get(), DstBinding, InOffset, bFullRange);
			}
		}
	}

	template <typename T>
	void BindStorage(const UniquePtr<UHRenderBuffer<T>> InBuffer[GMaxFrameInFlight], int32_t DstBinding, int32_t InOffset, bool bFullRange)
	{
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
			if (InBuffer[Idx])
			{
				Helper.WriteStorageBuffer(InBuffer[Idx].get(), DstBinding, InOffset, bFullRange);
			}
		}
	}

	// bind single storage
	template <typename T>
	void BindStorage(const UHRenderBuffer<T>* InBuffer, int32_t DstBinding, uint64_t InOffset, bool bFullRange)
	{
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
			Helper.WriteStorageBuffer(InBuffer, DstBinding, InOffset, bFullRange);
		}
	}

	// bind single storage with frame index
	template <typename T>
	void BindStorage(const UHRenderBuffer<T>* InBuffer, int32_t DstBinding, uint64_t InOffset, bool bFullRange, int32_t FrameIdx)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[FrameIdx]);
		Helper.WriteStorageBuffer(InBuffer, DstBinding, InOffset, bFullRange);
	}

	// bind multiple storage
	template <typename T>
	void BindStorage(const std::vector<UHRenderBuffer<T>*>& InBuffers, int32_t DstBinding)
	{
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
			if (InBuffers.size() > 0)
			{
				Helper.WriteStorageBuffer(InBuffers, DstBinding);
			}
		}
	}

	// bind multiple storage with frame index
	template <typename T>
	void BindStorage(const std::vector<UHRenderBuffer<T>*>& InBuffers, int32_t DstBinding, int32_t FrameIdx)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[FrameIdx]);
		if (InBuffers.size() > 0)
		{
			Helper.WriteStorageBuffer(InBuffers, DstBinding);
		}
	}

	// bind multiple storage, but this one uses VkDescriptorBufferInfo as input directly
	void BindStorage(const std::vector<VkDescriptorBufferInfo>& InBufferInfos, int32_t DstBinding)
	{
		for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
		{
			UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
			if (InBufferInfos.size() > 0)
			{
				Helper.WriteStorageBuffer(InBufferInfos, DstBinding);
			}
		}
	}

	void BindImage(const UHTexture* InImage, const int32_t DstBinding);
	void BindUavAsSrv(const UHTexture* InImage, const int32_t DstBinding);
	void BindImage(const UHTexture* InImage, const int32_t DstBinding, const int32_t LayerIdx);
	void BindImage(const UHTexture* InImage, const int32_t DstBinding, const int32_t CurrentFrameRT, const bool bIsReadWrite, const int32_t MipIdx);
	void BindImage(const std::vector<UHTexture*> InImages, const int32_t DstBinding);
	void PushImage(const UHTexture* InImage, const int32_t DstBinding, const bool bIsReadWrite, const int32_t MipIdx
		, const int32_t LayerIdx = UHINDEXNONE, bool bUavAsSrv = false);
	void PushSampler(const UHSampler* InSampler, const int32_t DstBinding);
	void FlushPushDescriptor(VkCommandBuffer InCmdList);

	void BindRWImage(const UHTexture* InImage, const int32_t DstBinding);
	void BindRWImage(const std::vector<UHRenderTexture*> InImages, const int32_t DstBinding);
	void BindRWImage(const UHTexture* InImage, const int32_t DstBinding, const int32_t MipIdx);

	void BindSampler(const UHSampler* InSampler, const int32_t DstBinding);
	void BindSampler(const std::vector<UHSampler*>& InSamplers, const int32_t DstBinding);
	void BindTLAS(const UHAccelerationStructure* InTopAS, const int32_t DstBinding, const int32_t CurrentFrameRT);
	void RecreateMaterialState();
	void SetNewMaterialCache(UHMaterial* InMat);
	void SetNewRenderPass(VkRenderPass InRenderPass);

	uint32_t GetRayGenShader() const;
	const std::vector<uint32_t>& GetClosestShaders() const;
	const std::vector<uint32_t>& GetAnyHitShaders() const;
	uint32_t GetMissShader() const;
	UHGraphicState* GetState() const;
	UHGraphicState* GetRTState() const;
	UHComputeState* GetComputeState() const;
	UHMaterial* GetMaterialCache() const;
	UHRenderBuffer<UHShaderRecord>* GetRayGenTable() const;
	UHRenderBuffer<UHShaderRecord>* GetHitGroupTable() const;
	UHRenderBuffer<UHShaderRecord>* GetMissTable() const;

	VkDescriptorSetLayout GetDescriptorSetLayout() const;
	VkPipelineLayout GetPipelineLayout() const;
	VkDescriptorSet GetDescriptorSet(int32_t FrameIdx) const;
	const VkPushConstantRange& GetPushConstantRange() const;

protected:
	// add layout binding
	void AddLayoutBinding(uint32_t DescriptorCount, VkShaderStageFlags StageFlags, VkDescriptorType DescriptorType, int32_t OverrideBind = UHINDEXNONE);

	// create descriptor, call this after shader class is done adding the layout binding
	// each shader is default to one layout, but it can use additional layouts
	void CreateLayoutAndDescriptor(std::vector<VkDescriptorSetLayout> AdditionalLayouts = std::vector<VkDescriptorSetLayout>());

	// create state functions
	void CreateGraphicState(UHRenderPassInfo InInfo);
	void CreateMaterialState(UHRenderPassInfo InInfo);
	void CreateComputeState(UHComputePassInfo InInfo);

	// ray tracing functions
	void InitRayGenTable();
	void InitMissTable();
	void InitHitGroupTable(size_t NumMaterials);

	UHGraphic* Gfx;
	std::type_index TypeIndexCache;
	UHMaterial* MaterialCache;
	VkRenderPass RenderPassCache;
	UHRenderPassInfo MaterialPassInfo;

	std::vector<VkDescriptorSetLayoutBinding> LayoutBindings;
	VkPipelineLayout PipelineLayout;
	VkDescriptorPool DescriptorPool;
	std::array<VkDescriptorSet, GMaxFrameInFlight> DescriptorSets;

	// store shader's id instead of UHShader*, prevent dangling pointer issue
	uint32_t ShaderVS;
	uint32_t ShaderPS;
	uint32_t ShaderCS;
	uint32_t RayGenShader;
	std::vector<uint32_t> ClosestHitShaders;
	std::vector<uint32_t> AnyHitShaders;
	uint32_t MissShader;
	uint32_t ShaderAS;
	uint32_t ShaderMS;

	UHGraphicState* RTState;

	// shader table
	UniquePtr<UHRenderBuffer<UHShaderRecord>> RayGenTable;
	UniquePtr<UHRenderBuffer<UHShaderRecord>> HitGroupTable;
	UniquePtr<UHRenderBuffer<UHShaderRecord>> MissTable;

	// Push constant range & descriptor support
	VkPushConstantRange PushConstantRange;
	bool bPushDescriptor;
	std::vector<VkWriteDescriptorSet> PushDescriptorSets;
	std::vector<VkDescriptorImageInfo> PushImageInfos;
	std::vector<VkDescriptorBufferInfo> PushBufferInfos;

private:
	// layout won't change for the same shaders and can be cached
	static std::unordered_map<std::type_index, VkDescriptorSetLayout> DescriptorSetLayoutTable;
	static std::unordered_map<std::type_index, VkPipelineLayout> PipelineLayoutTable;

	// graphic state table, the material is a bit more complicated since it could be involded in different passes
	static std::unordered_map<uint32_t, UHGraphicState*> GraphicStateTable;
	static std::unordered_map<uint32_t, std::unordered_map<std::type_index, UHGraphicState*>> MaterialStateTable;
	static std::unordered_map<uint32_t, UHComputeState*> ComputeStateTable;
};
