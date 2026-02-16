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
void UHDeferredShadingRenderer::DispatchGaussianFilter(UHRenderBuilder& RenderBuilder
	, const std::string& InName
	, UHRenderTexture* Input
	, UHRenderTexture* Output
	, const UHGaussianFilterConstants& Constants)
{
	VkExtent2D FilterResolution;
	FilterResolution.width = Constants.GBlurResolution[0];
	FilterResolution.height = Constants.GBlurResolution[1];

	// early return if the resolution is too small
	if (FilterResolution.width * FilterResolution.height < 16)
	{
		return;
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
	const VkPushConstantRange& CosntRange = GaussianFilterHShader->GetPushConstantRange();
	RenderBuilder.PushConstant(GaussianFilterHShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, CosntRange.size, &Constants);
	RenderBuilder.PushConstant(GaussianFilterVShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, CosntRange.size, &Constants);

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
			GaussianFilterHShader->BindParameters(RenderBuilder, GaussianFilterTempRT1, GaussianFilterTempRT0);

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
			GaussianFilterVShader->BindParameters(RenderBuilder, GaussianFilterTempRT0, GaussianFilterTempRT1);

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
}

// dispatch bilateral filtering
void UHDeferredShadingRenderer::DispatchBilateralFilter(UHRenderBuilder& RenderBuilder, const std::string& InName
	, UHRenderTexture* Input, UHRenderTexture* Output
	, const UHBilateralFilterConstants& Constants)
{
	VkExtent2D FilterResolution;
	FilterResolution.width = Constants.FilterResolution[0];
	FilterResolution.height = Constants.FilterResolution[1];

	// early return if the resolution is too small
	if (FilterResolution.width * FilterResolution.height < 16)
	{
		return;
	}

	// get temp buffers
	UHRenderTexture* FilterTempRT0 = Constants.FilterTempRT0;
	UHRenderTexture* FilterTempRT1 = Constants.FilterTempRT1;

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), InName);

	// blit the input to RT1
	RenderBuilder.PushResourceBarrier(UHImageBarrier(Input, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(FilterTempRT1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();
	RenderBuilder.Blit(Input, FilterTempRT1, FilterResolution, FilterResolution);

	// update constants
	const VkPushConstantRange& CosntRange = BilateralFilterHShader->GetPushConstantRange();
	RenderBuilder.PushConstant(BilateralFilterHShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, CosntRange.size, &Constants);
	RenderBuilder.PushConstant(BilateralFilterVShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, CosntRange.size, &Constants);

	for (int32_t Idx = 0; Idx < Constants.IterationCount; Idx++)
	{
		// Horizontal pass to the temp texture
		{
			RenderBuilder.PushResourceBarrier(UHImageBarrier(FilterTempRT1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(FilterTempRT0, VK_IMAGE_LAYOUT_GENERAL));
			RenderBuilder.FlushResourceBarrier();

			// bind compute state
			UHGraphicState* State = BilateralFilterHShader->GetComputeState();
			RenderBuilder.BindComputeState(State);
			BilateralFilterHShader->BindParameters(RenderBuilder, FilterTempRT1, FilterTempRT0, CurrentFrameRT);

			// dispatch compute
			RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(FilterResolution.width, GThreadGroup1D), FilterResolution.height, 1);
		}

		// Vertical pass from temp horizontal blur result to output
		{
			RenderBuilder.PushResourceBarrier(UHImageBarrier(FilterTempRT0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(FilterTempRT1, VK_IMAGE_LAYOUT_GENERAL));
			RenderBuilder.FlushResourceBarrier();

			// bind compute state
			UHGraphicState* State = BilateralFilterVShader->GetComputeState();
			RenderBuilder.BindComputeState(State);
			BilateralFilterVShader->BindParameters(RenderBuilder, FilterTempRT0, FilterTempRT1, CurrentFrameRT);

			// dispatch compute
			RenderBuilder.Dispatch(FilterResolution.width, MathHelpers::RoundUpDivide(FilterResolution.height, GThreadGroup1D), 1);
		}
	}

	// blit result to output
	RenderBuilder.PushResourceBarrier(UHImageBarrier(FilterTempRT1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(Output, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();
	RenderBuilder.Blit(FilterTempRT1, Output, FilterResolution, FilterResolution);

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}

void UHDeferredShadingRenderer::DispatchKawaseBlur(UHRenderBuilder& RenderBuilder, const std::string& InName
	, UHRenderTexture* Input, UHRenderTexture* Output
	, const UHKawaseBlurConstants& InConstants)
{
	VkExtent2D FilterResolution = Input->GetExtent();

	if (InConstants.PassCount <= 0)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), InName);

	std::vector<UHRenderTexture*> KawaseRTs(InConstants.PassCount + 1);
	KawaseRTs[0] = Input;

	// N downsample passes
	for (int32_t Idx = 0; Idx < InConstants.PassCount; Idx++)
	{
		const int32_t OutputIdx = Idx + 1;
		KawaseRTs[OutputIdx] = InConstants.KawaseTempRT[Idx];

		// whether to use mip as temp RT
		const int32_t InputMip = InConstants.bUseMipAsTempRT ? InConstants.StartInputMip + Idx : 0;
		const int32_t OutputMip = InConstants.bUseMipAsTempRT ? InConstants.StartOutputMip + 1 + Idx : 0;

		// bind compute state
		UHGraphicState* State = KawaseDownsampleShader->GetComputeState();
		RenderBuilder.BindComputeState(State);

		// push constants
		VkExtent2D KawaseExtent;
		KawaseExtent.width = InConstants.bUseMipAsTempRT ? FilterResolution.width >> OutputMip : FilterResolution.width >> OutputIdx;
		KawaseExtent.height = InConstants.bUseMipAsTempRT ? FilterResolution.height >> OutputMip : FilterResolution.height >> OutputIdx;

		UHKawaseBlurConstants KawaseConstants;
		KawaseConstants.Width = KawaseExtent.width;
		KawaseConstants.Height = KawaseExtent.height;
		KawaseConstants.PreserveAlpha = InConstants.PreserveAlpha;

		const VkPushConstantRange& CosntRange = KawaseDownsampleShader->GetPushConstantRange();
		RenderBuilder.PushConstant(KawaseDownsampleShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT
			, CosntRange.size, &KawaseConstants);

		// resource barriers
		RenderBuilder.PushResourceBarrier(UHImageBarrier(KawaseRTs[Idx], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, InputMip));
		RenderBuilder.PushResourceBarrier(UHImageBarrier(KawaseRTs[OutputIdx], VK_IMAGE_LAYOUT_GENERAL, OutputMip));
		RenderBuilder.FlushResourceBarrier();

		// bind IO
		KawaseDownsampleShader->BindParameters(RenderBuilder, KawaseRTs[Idx], KawaseRTs[OutputIdx]
			, InputMip, OutputMip);

		// dispatch compute
		RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(KawaseExtent.width, GThreadGroup2D_X)
			, MathHelpers::RoundUpDivide(KawaseExtent.height, GThreadGroup2D_Y), 1);
	}

	// N upsample passes
	if (!InConstants.bDownsampleOnly)
	{
		KawaseRTs[0] = Output;
		for (int32_t Idx = InConstants.PassCount; Idx > 0; Idx--)
		{
			const int32_t OutputIdx = Idx - 1;

			// whether to use mip as temp RT
			const int32_t InputMip = InConstants.bUseMipAsTempRT ? InConstants.StartInputMip + Idx : 0;
			const int32_t OutputMip = InConstants.bUseMipAsTempRT ? InConstants.StartOutputMip + Idx - 1 : 0;

			// bind compute state
			UHGraphicState* State = KawaseUpsampleShader->GetComputeState();
			RenderBuilder.BindComputeState(State);

			// push constants
			VkExtent2D KawaseExtent;
			KawaseExtent.width = InConstants.bUseMipAsTempRT ? FilterResolution.width >> OutputMip : FilterResolution.width >> OutputIdx;
			KawaseExtent.height = InConstants.bUseMipAsTempRT ? FilterResolution.height >> OutputMip : FilterResolution.height >> OutputIdx;

			UHKawaseBlurConstants KawaseConstants;
			KawaseConstants.Width = KawaseExtent.width;
			KawaseConstants.Height = KawaseExtent.height;
			KawaseConstants.PreserveAlpha = InConstants.PreserveAlpha;

			const VkPushConstantRange& CosntRange = KawaseDownsampleShader->GetPushConstantRange();
			RenderBuilder.PushConstant(KawaseUpsampleShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT
				, CosntRange.size, &KawaseConstants);

			// resource barriers
			RenderBuilder.PushResourceBarrier(UHImageBarrier(KawaseRTs[Idx], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, InputMip));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(KawaseRTs[OutputIdx], VK_IMAGE_LAYOUT_GENERAL, OutputMip));
			RenderBuilder.FlushResourceBarrier();

			// bind IO
			KawaseDownsampleShader->BindParameters(RenderBuilder, KawaseRTs[Idx], KawaseRTs[OutputIdx]
				, InputMip, OutputMip);

			// dispatch compute
			RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(KawaseExtent.width, GThreadGroup2D_X)
				, MathHelpers::RoundUpDivide(KawaseExtent.height, GThreadGroup2D_Y), 1);
		}
	}

	RenderBuilder.ResourceBarrier(Output, Output->GetImageLayout(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}