#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::DispatchGaussianBlur(UHRenderBuilder& RenderBuilder, std::string InName, UHRenderTexture* Input, UHRenderTexture* Output, int32_t IterationCount)
{
	GraphicInterface->BeginCmdDebug(RenderBuilder.GetCmdList(), InName);

	// I/O needs to match the size
	VkExtent2D InputExtent = Input->GetExtent();
	VkExtent2D OutputExtent = Output->GetExtent();
	assert(InputExtent.width == OutputExtent.width && InputExtent.height == OutputExtent.height);

	VkExtent2D BlurResolution = InputExtent;
	if (GaussianBlurTempRT0 == nullptr)
	{
		// Use R11G11B10 as a performance hack
		GaussianBlurTempRT0 = GraphicInterface->RequestRenderTexture("GaussianBlurTemp0", BlurResolution, UH_FORMAT_R11G11B10, true);
	}

	if (GaussianBlurTempRT1 == nullptr)
	{
		GaussianBlurTempRT1 = GraphicInterface->RequestRenderTexture("GaussianBlurTemp1", BlurResolution, UH_FORMAT_R11G11B10, true);
	}

	// Blit the input to RT1
	RenderBuilder.PushResourceBarrier(UHImageBarrier(Input, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GaussianBlurTempRT1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();
	RenderBuilder.Blit(Input, GaussianBlurTempRT1);

	BlurHorizontalShader->BindImage(GaussianBlurTempRT0, 1, CurrentFrameRT, true);
	BlurHorizontalShader->BindImage(GaussianBlurTempRT1, 2, CurrentFrameRT);

	BlurVerticalShader->BindImage(GaussianBlurTempRT1, 1, CurrentFrameRT, true);
	BlurVerticalShader->BindImage(GaussianBlurTempRT0, 2, CurrentFrameRT);

	// Push blur resolution
	vkCmdPushConstants(RenderBuilder.GetCmdList(), BlurHorizontalShader->GetPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(VkExtent2D), &BlurResolution);

	for (int32_t Idx = 0; Idx < IterationCount; Idx++)
	{
		// Horizontal pass to the temp texture
		{
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GaussianBlurTempRT1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GaussianBlurTempRT0, VK_IMAGE_LAYOUT_GENERAL));
			RenderBuilder.FlushResourceBarrier();

			// bind compute state
			UHGraphicState* State = BlurHorizontalShader->GetComputeState();
			RenderBuilder.BindComputeState(State);
			RenderBuilder.BindDescriptorSetCompute(BlurHorizontalShader->GetPipelineLayout(), BlurHorizontalShader->GetDescriptorSet(CurrentFrameRT));

			// dispatch compute
			RenderBuilder.Dispatch(MathHelpers::RoundUpDivide(BlurResolution.width, GThreadGroup1D), BlurResolution.height, 1);
		}

		// Vertical pass from temp horizontal blur result to output
		{
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GaussianBlurTempRT0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			RenderBuilder.PushResourceBarrier(UHImageBarrier(GaussianBlurTempRT1, VK_IMAGE_LAYOUT_GENERAL));
			RenderBuilder.FlushResourceBarrier();

			// bind compute state
			UHGraphicState* State = BlurVerticalShader->GetComputeState();
			RenderBuilder.BindComputeState(State);
			RenderBuilder.BindDescriptorSetCompute(BlurVerticalShader->GetPipelineLayout(), BlurVerticalShader->GetDescriptorSet(CurrentFrameRT));

			// dispatch compute
			RenderBuilder.Dispatch(BlurResolution.width, MathHelpers::RoundUpDivide(BlurResolution.height, GThreadGroup1D), 1);
		}
	}

	// blit result to output
	RenderBuilder.PushResourceBarrier(UHImageBarrier(GaussianBlurTempRT1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL));
	RenderBuilder.PushResourceBarrier(UHImageBarrier(Output, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
	RenderBuilder.FlushResourceBarrier();
	RenderBuilder.Blit(GaussianBlurTempRT1, Output);

	GraphicInterface->EndCmdDebug(RenderBuilder.GetCmdList());
}