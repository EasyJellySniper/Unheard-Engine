#pragma once
#include "../../Engine/Graphic.h"
#include "../DescriptorHelper.h"
#include <typeindex>

const int32_t GMaterialSlotInRT = 16;

// 32 byte structure for shader table record
struct UHShaderRecord
{
	BYTE Record[32];
};

// high-level shader class, this will be actually used for rendering
// descriptor is set here too, each shader class is aim for keeping a minium set of descriptors
class UHShaderClass
{
public:
	UHShaderClass();
	UHShaderClass(UHGraphic* InGfx, std::string Name, std::type_index InType);
	void Release();

	template <typename T>
	void BindConstant(std::array<std::unique_ptr<UHRenderBuffer<T>>, GMaxFrameInFlight>& InBuffer, int32_t DstBinding, int32_t InOffset = 0)
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
	void BindStorage(std::array<std::unique_ptr<UHRenderBuffer<T>>, GMaxFrameInFlight>& InBuffer, int32_t DstBinding, int32_t InOffset = 0, bool bFullRange = false)
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

	// bind multiple storage
	template <typename T>
	void BindStorage(std::vector<UHRenderBuffer<T>*> InBuffers, int32_t DstBinding)
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

	void BindImage(UHTexture* InImage, int32_t DstBinding, int32_t CurrentFrame = -1);
	void BindImage(std::vector<UHTexture*> InImages, int32_t DstBinding);
	void BindRWImage(UHTexture* InImage, int32_t DstBinding);
	void BindSampler(UHSampler* InSampler, int32_t DstBinding);
	void BindSampler(std::vector<UHSampler*> InSamplers, int32_t DstBinding);
	void BindTLAS(UHAccelerationStructure* InTopAS, int32_t DstBinding, int32_t CurrentFrame);

	UHShader* GetVS();
	UHShader* GetPS();
	UHShader* GetRayGenShader();
	UHShader* GetClosestShader();
	UHShader* GetAnyHitShader();
	UHGraphicState* GetState();
	UHGraphicState* GetRTState();
	UHRenderBuffer<UHShaderRecord>* GetRayGenTable();
	UHRenderBuffer<UHShaderRecord>* GetHitGroupTable();

	VkDescriptorSetLayout GetDescriptorSetLayout();
	VkPipelineLayout GetPipelineLayout();
	VkDescriptorSet GetDescriptorSet(int32_t FrameIdx);

protected:
	// add layout binding
	void AddLayoutBinding(uint32_t DescriptorCount, VkShaderStageFlags StageFlags, VkDescriptorType DescriptorType, int32_t OverrideBind = -1);

	// create descriptor, call this after shader class is done adding the layout binding
	// each shader is default to one layout, but it can use additional layouts
	void CreateDescriptor(std::vector<VkDescriptorSetLayout> AdditionalLayouts = std::vector<VkDescriptorSetLayout>());

	UHGraphic* Gfx;
	std::string Name;
	std::type_index TypeIndexCache;

	std::vector<VkDescriptorSetLayoutBinding> LayoutBindings;
	VkPipelineLayout PipelineLayout;
	VkDescriptorPool DescriptorPool;
	std::array<VkDescriptorSet, GMaxFrameInFlight> DescriptorSets;

	UHShader* ShaderVS;
	UHShader* ShaderPS;
	UHShader* RayGenShader;
	UHShader* ClosestHitShader;
	UHShader* AnyHitShader;
	UHGraphicState* GraphicState;
	UHGraphicState* RTState;

	// shader table
	std::unique_ptr<UHRenderBuffer<UHShaderRecord>> RayGenTable;
	std::unique_ptr<UHRenderBuffer<UHShaderRecord>> HitGroupTable;
};

