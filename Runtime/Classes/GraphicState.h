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

	void Release();

	VkPipeline GetGraphicPipeline() const;
	VkPipeline GetRTPipeline() const;

	bool operator==(const UHGraphicState& InState);

private:
	bool CreateState(UHRenderPassInfo InInfo);
	bool CreateState(UHRayTracingInfo InInfo);

	VkPipeline GraphicsPipeline;
	VkPipeline RTPipeline;

	// variable caches
	UHRenderPassInfo RenderPassInfo;
	UHRayTracingInfo RayTracingInfo;

	friend UHGraphic;
};