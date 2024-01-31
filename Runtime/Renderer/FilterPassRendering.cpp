#include "DeferredShadingRenderer.h"

// Use a fixed kernal for now, the weights are pre-calculated based on the following lines:
// Sigma2 = 2.0f * Sigma * Sigma;
// BlurRadius = ceil(2.0f * sigma);
// Loop I = -BlurRadius to BlurRadius
	// X = I
	// Weights[I] = expf(-X*X/Sigma2);
	// WeightSum += Weights[I]
// Loop I = 0 to Weights.size()
	// Weights[I] / WeightSum
std::vector<float> UHDeferredShadingRenderer::CalculateBlurWeights(const int32_t InRadius)
{
	const int32_t BlurRadius = std::clamp(InRadius, 1, GMaxGaussianFilterRadius);
	std::vector<float> Weights(GMaxGaussianFilterRadius * 2 + 1);

	const float Sigma = BlurRadius / 2.0f;
	const float Sigma2Squared = 2.0f * Sigma * Sigma;

	float WeightSum = 0.0f;
	for (int32_t I = -BlurRadius; I <= BlurRadius; I++)
	{
		const float X = static_cast<float>(I);
		const int32_t Index = I + BlurRadius;

		Weights[Index] = expf(-X * X / Sigma2Squared);
		WeightSum += Weights[Index];
	}

	for (size_t I = 0; I < Weights.size(); I++)
	{
		Weights[I] /= WeightSum;
	}

	return Weights;
}

void UHDeferredShadingRenderer::DispatchGaussianFilter(UHRenderBuilder& RenderBuilder
	, std::string InName
	, UHRenderTexture* Input
	, UHRenderTexture* Output
	, UHGaussianFilterConstants Constants
	, UHGaussianFilterShader* BlurHShader, UHGaussianFilterShader* BlurVShader)
{
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), InName);

	VkExtent2D FilterResolution = RenderResolution;
	FilterResolution.width >>= Constants.DownsizeFactor;
	FilterResolution.height >>= Constants.DownsizeFactor;

	// Blit the input to RT1
	RenderBuilder.PushResourceBarrier(UHImageBarrier(Input, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GGaussianFilterTempRT1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();
	RenderBuilder.Blit(Input, GGaussianFilterTempRT1, Input->GetExtent(), FilterResolution);

	BlurHShader->SetGaussianConstants(Constants, CurrentFrameRT);
	BlurVShader->SetGaussianConstants(Constants, CurrentFrameRT);

	for (int32_t Idx = 0; Idx < Constants.IterationCount; Idx++)
	{
		// Horizontal pass to the temp texture
		{
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GGaussianFilterTempRT1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GGaussianFilterTempRT0, VK_IMAGE_LAYOUT_GENERAL));
			RenderBuilder.FlushResourceBarrier();

			// bind compute state
			UHGraphicState* State = BlurHShader->GetComputeState();
			RenderBuilder.BindComputeState(State);
			RenderBuilder.BindDescriptorSetCompute(BlurHShader->GetPipelineLayout(), BlurHShader->GetDescriptorSet(CurrentFrameRT));

			// dispatch compute
			RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(FilterResolution.width, GThreadGroup1D), FilterResolution.height, 1);
		}

		// Vertical pass from temp horizontal blur result to output
		{
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GGaussianFilterTempRT0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GGaussianFilterTempRT1, VK_IMAGE_LAYOUT_GENERAL));
			RenderBuilder.FlushResourceBarrier();

			// bind compute state
			UHGraphicState* State = BlurVShader->GetComputeState();
			RenderBuilder.BindComputeState(State);
			RenderBuilder.BindDescriptorSetCompute(BlurVShader->GetPipelineLayout(), BlurVShader->GetDescriptorSet(CurrentFrameRT));

			// dispatch compute
			RenderBuilder.Dispatch(FilterResolution.width, MathHelpers::RoundUpDivide(FilterResolution.height, GThreadGroup1D), 1);
		}
	}

	// blit result to output
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GGaussianFilterTempRT1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(Output, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();
	RenderBuilder.Blit(GGaussianFilterTempRT1, Output, FilterResolution, Output->GetExtent());

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}