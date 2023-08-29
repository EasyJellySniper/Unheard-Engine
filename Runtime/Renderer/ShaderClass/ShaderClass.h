#pragma once
#include "../../Engine/Graphic.h"
#include "../DescriptorHelper.h"
#include <typeindex>

extern std::unordered_map<uint32_t, UHGraphicState*> GGraphicStateTable;
extern std::unordered_map<uint32_t, std::unordered_map<std::type_index, UHGraphicState*>> GMaterialStateTable;

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
	UHShaderClass();
	UHShaderClass(UHGraphic* InGfx, std::string Name, std::type_index InType, UHMaterial* InMat);
	void Release();

	template <typename T>
	void BindConstant(const std::array<UniquePtr<UHRenderBuffer<T>>, GMaxFrameInFlight>& InBuffer, int32_t DstBinding, int32_t InOffset = 0)
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
	void BindConstant(const UniquePtr<UHRenderBuffer<T>>& InBuffer, int32_t DstBinding, int32_t CurrentFrame, int32_t InOffset = 0)
	{
		UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[CurrentFrame]);
		if (InBuffer)
		{
			Helper.WriteConstantBuffer(InBuffer.get(), DstBinding, InOffset);
		}
	}

	template <typename T>
	void BindStorage(const std::array<UniquePtr<UHRenderBuffer<T>>, GMaxFrameInFlight>& InBuffer, int32_t DstBinding, int32_t InOffset = 0, bool bFullRange = false)
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
	void BindStorage(const UHRenderBuffer<T>* InBuffer, int32_t DstBinding, uint64_t InOffset = 0, bool bFullRange = false, int32_t FrameIdx = -1)
	{
		if (FrameIdx != -1)
		{
			UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[FrameIdx]);
			Helper.WriteStorageBuffer(InBuffer, DstBinding, InOffset, bFullRange);
		}
		else
		{
			for (int32_t Idx = 0; Idx < GMaxFrameInFlight; Idx++)
			{
				UHDescriptorHelper Helper(Gfx->GetLogicalDevice(), DescriptorSets[Idx]);
				Helper.WriteStorageBuffer(InBuffer, DstBinding, InOffset, bFullRange);
			}
		}
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

	void BindImage(const UHTexture* InImage, int32_t DstBinding, int32_t CurrentFrameRT = -1, bool bIsReadWrite = false);
	void BindImage(const std::vector<UHTexture*> InImages, int32_t DstBinding);
	void BindRWImage(const UHTexture* InImage, int32_t DstBinding);
	void BindSampler(const UHSampler* InSampler, int32_t DstBinding);
	void BindSampler(const std::vector<UHSampler*>& InSamplers, int32_t DstBinding);
	void BindTLAS(const UHAccelerationStructure* InTopAS, int32_t DstBinding, int32_t CurrentFrameRT);
	void RecreateMaterialState();

	uint32_t GetRayGenShader() const;
	std::vector<uint32_t> GetClosestShaders() const;
	std::vector<uint32_t> GetAnyHitShaders() const;
	uint32_t GetMissShader() const;
	UHGraphicState* GetState() const;
	UHGraphicState* GetRTState() const;
	UHComputeState* GetComputeState() const;
	UHRenderBuffer<UHShaderRecord>* GetRayGenTable() const;
	UHRenderBuffer<UHShaderRecord>* GetHitGroupTable() const;
	UHRenderBuffer<UHShaderRecord>* GetMissTable() const;

	VkDescriptorSetLayout GetDescriptorSetLayout() const;
	VkPipelineLayout GetPipelineLayout() const;
	VkDescriptorSet GetDescriptorSet(int32_t FrameIdx) const;

protected:
	// add layout binding
	void AddLayoutBinding(uint32_t DescriptorCount, VkShaderStageFlags StageFlags, VkDescriptorType DescriptorType, int32_t OverrideBind = -1);

	// create descriptor, call this after shader class is done adding the layout binding
	// each shader is default to one layout, but it can use additional layouts
	void CreateDescriptor(std::vector<VkDescriptorSetLayout> AdditionalLayouts = std::vector<VkDescriptorSetLayout>());

	// create descriptor but for material
	void CreateMaterialDescriptor(std::vector<VkDescriptorSetLayout> AdditionalLayouts = std::vector<VkDescriptorSetLayout>());

	// create state functions
	void CreateGraphicState(UHRenderPassInfo InInfo);
	void CreateMaterialState(UHRenderPassInfo InInfo);
	void CreateComputeState(UHComputePassInfo InInfo);

	// ray tracing functions
	void InitRayGenTable();
	void InitMissTable();
	void InitHitGroupTable(size_t NumMaterials);

	UHGraphic* Gfx;
	std::string Name;
	std::type_index TypeIndexCache;
	UHMaterial* MaterialCache;
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
	UHGraphicState* RTState;

	// shader table
	UniquePtr<UHRenderBuffer<UHShaderRecord>> RayGenTable;
	UniquePtr<UHRenderBuffer<UHShaderRecord>> HitGroupTable;
	UniquePtr<UHRenderBuffer<UHShaderRecord>> MissTable;
};

