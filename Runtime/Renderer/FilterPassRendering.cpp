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
void UHDeferredShadingRenderer::CalculateBlurWeights(const int32_t InRadius, float* OutWeights)
{
	const int32_t BlurRadius = std::clamp(InRadius, 1, GMaxGaussianFilterRadius);

	const float Sigma = BlurRadius / 2.0f;
	const float Sigma2Squared = 2.0f * Sigma * Sigma;

	float WeightSum = 0.0f;
	for (int32_t I = -BlurRadius; I <= BlurRadius; I++)
	{
		const float X = static_cast<float>(I);
		const int32_t Index = I + BlurRadius;

		OutWeights[Index] = expf(-X * X / Sigma2Squared);
		WeightSum += OutWeights[Index];
	}

	for (size_t I = 0; I < GMaxGaussianFilterRadius * 2 + 1; I++)
	{
		OutWeights[I] /= WeightSum;
	}
}

// dispatch the gaussian filter, input could be from any 2D source, but the output must be render texture.
bool UHDeferredShadingRenderer::DispatchGaussianFilter(UHRenderBuilder& RenderBuilder
	, const std::string& InName
	, UHTexture* Input
	, UHRenderTexture* Output
	, const UHGaussianFilterConstants& Constants)
{
	VkExtent2D FilterResolution;
	FilterResolution.width = Constants.GBlurResolution[0];
	FilterResolution.height = Constants.GBlurResolution[1];

	// early return if the resolution is too small
	if (FilterResolution.width * FilterResolution.height < 16)
	{
		return false;
	}

	// get gaussian temp buffer
	UHRenderTexture* GaussianFilterTempRT0 = Constants.GaussianFilterTempRT0[Constants.InputMip];
	UHRenderTexture* GaussianFilterTempRT1 = Constants.GaussianFilterTempRT1[Constants.InputMip];

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), InName);

	// Blit the input to RT1
	const int32_t MipLevel = Constants.InputMip;
	RenderBuilder.PushResourceBarrier(UHImageBarrier(Input, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, MipLevel));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GaussianFilterTempRT1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();
	RenderBuilder.Blit(Input, GaussianFilterTempRT1, FilterResolution, FilterResolution, MipLevel);

	// update constants
	vkCmdPushConstants(RenderBuilder.GetCmdList(), GaussianFilterHShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Constants), &Constants);
	vkCmdPushConstants(RenderBuilder.GetCmdList(), GaussianFilterVShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Constants), &Constants);

	for (int32_t Idx = 0; Idx < Constants.IterationCount; Idx++)
	{
		// Horizontal pass to the temp texture
		{
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GaussianFilterTempRT1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GaussianFilterTempRT0, VK_IMAGE_LAYOUT_GENERAL));
			RenderBuilder.FlushResourceBarrier();

			// bind compute state
			UHGraphicState* State = GaussianFilterHShader->GetComputeState();
			RenderBuilder.BindComputeState(State);
			GaussianFilterHShader->BindParameters(RenderBuilder, CurrentFrameRT, GaussianFilterTempRT1, GaussianFilterTempRT0);

			// dispatch compute
			RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(FilterResolution.width, GThreadGroup1D), FilterResolution.height, 1);
		}

		// Vertical pass from temp horizontal blur result to output
		{
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GaussianFilterTempRT0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GaussianFilterTempRT1, VK_IMAGE_LAYOUT_GENERAL));
			RenderBuilder.FlushResourceBarrier();

			// bind compute state
			UHGraphicState* State = GaussianFilterVShader->GetComputeState();
			RenderBuilder.BindComputeState(State);
			GaussianFilterVShader->BindParameters(RenderBuilder, CurrentFrameRT, GaussianFilterTempRT0, GaussianFilterTempRT1);

			// dispatch compute
			RenderBuilder.Dispatch(FilterResolution.width, MathHelpers::RoundUpDivide(FilterResolution.height, GThreadGroup1D), 1);
		}
	}

	// blit result to output
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GaussianFilterTempRT1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(Output, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, MipLevel));
	RenderBuilder.FlushResourceBarrier();
	RenderBuilder.Blit(GaussianFilterTempRT1, Output, FilterResolution, FilterResolution, 0, MipLevel);

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
	return true;
}