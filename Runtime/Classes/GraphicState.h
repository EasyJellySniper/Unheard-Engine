#pragma once
#include "../Engine/RenderResource.h"
#include "MaterialLayout.h"
#include "Shader.h"
#include "../Renderer/RenderingTypes.h"

class UHGraphic;

// class for graphic states, needs to be requested from UHGraphic class
class UHGraphicState : public UHRenderResource
{
public:
	UHGraphicState();
	UHGraphicState(UHRenderPassInfo InInfo);
	UHGraphicState(UHRayTracingInfo InInfo);
	UHGraphicState(UHComputePassInfo InInfo);

	void Release();

	VkPipeline GetPassPipeline() const;
	VkPipeline GetRTPipeline() const;
	UHRenderPassInfo GetRenderPassInfo() const;

	bool operator==(const UHGraphicState& InState);

private:
	bool CreateState(UHRenderPassInfo InInfo);
	bool CreateState(UHRayTracingInfo InInfo);
	bool CreateState(UHComputePassInfo InInfo);

	VkPipeline PassPipeline;
	VkPipeline RTPipeline;

	// variable caches
	UHRenderPassInfo RenderPassInfo;
	UHRayTracingInfo RayTracingInfo;
	UHComputePassInfo ComputePassInfo;

	friend UHGraphic;
};

using UHComputeState = UHGraphicState;