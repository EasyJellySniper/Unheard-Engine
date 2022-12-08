#include "DeferredShadingRenderer.h"

void UHDeferredShadingRenderer::RenderMotionPass(UHGraphicBuilder& GraphBuilder)
{
	if (CurrentScene == nullptr)
	{
		return;
	}

	GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing Motion Pass");

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::MotionPass]->BeginTimeStamp(GraphBuilder.GetCmdList());
#endif

	// copy result to history velocity before rendering a new one
	GraphBuilder.ResourceBarrier(MotionVectorRT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	GraphBuilder.ResourceBarrier(PrevMotionVectorRT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	GraphBuilder.CopyTexture(MotionVectorRT, PrevMotionVectorRT);
	GraphBuilder.ResourceBarrier(PrevMotionVectorRT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	GraphBuilder.ResourceBarrier(MotionVectorRT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	GraphBuilder.ResourceBarrier(SceneDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkClearValue Clear = { 0,0,0,0 };
	GraphBuilder.BeginRenderPass(MotionCameraPassObj.RenderPass, MotionCameraPassObj.FrameBuffer, RenderResolution, Clear);

	GraphBuilder.SetViewport(RenderResolution);
	GraphBuilder.SetScissor(RenderResolution);

	// bind state
	UHGraphicState* State = MotionCameraShader.GetState();
	GraphBuilder.BindGraphicState(State);

	// bind sets
	GraphBuilder.BindDescriptorSet(MotionCameraShader.GetPipelineLayout(), MotionCameraShader.GetDescriptorSet(CurrentFrame));

	// doesn't need VB/IB for full screen quad
	GraphBuilder.DrawFullScreenQuad();
	GraphBuilder.EndRenderPass();



	// -------------------- after motion camera pass is done, draw per-object motions -------------------- //
	GraphBuilder.ResourceBarrier(SceneDepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	// begin for secondary cmd
	GraphBuilder.BeginRenderPass(MotionObjectPassObj.RenderPass, MotionObjectPassObj.FrameBuffer, RenderResolution);

	std::vector<VkCommandBuffer> CmdToExecute;
	for (UHMeshRendererComponent* Renderer : OpaquesToRender)
	{
		const UHMaterial* Mat = Renderer->GetMaterial();
		UHMesh* Mesh = Renderer->GetMesh();

		// skip skybox mat and only draw dirty renderer
		if (Mat->IsSkybox() || !Renderer->IsMotionDirty(CurrentFrame))
		{
			continue;
		}

		int32_t RendererIdx = Renderer->GetBufferDataIndex();
		if (MotionObjectShaders.find(RendererIdx) == MotionObjectShaders.end())
		{
			// unlikely to happen, but printing a message for debug
			UHE_LOG(L"[MotionObjectPass] Can't find motion object pass shader for material: \n");
			continue;
		}

		const UHMotionObjectPassShader& MotionShader = MotionObjectShaders[RendererIdx];

		GraphicInterface->BeginCmdDebug(GraphBuilder.GetCmdList(), "Drawing " + Mesh->GetName() + " (Tris: " +
			std::to_string(Mesh->GetIndicesCount() / 3) + ")");

		// bind pipelines
		GraphBuilder.BindGraphicState(MotionShader.GetState());
		GraphBuilder.BindVertexBuffer(Mesh->GetPositionBuffer()->GetBuffer());
		GraphBuilder.BindIndexBuffer(Mesh);
		GraphBuilder.BindDescriptorSet(MotionShader.GetPipelineLayout(), MotionShader.GetDescriptorSet(CurrentFrame));

		// draw call
		GraphBuilder.DrawIndexed(Mesh->GetIndicesCount());

		GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
		Renderer->SetMotionDirty(false, CurrentFrame);
	}

	GraphBuilder.EndRenderPass();

	// depth/motion vector will be used in shader later, transition them
	GraphBuilder.ResourceBarrier(MotionVectorRT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	GraphBuilder.ResourceBarrier(SceneDepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

#if WITH_DEBUG
	GPUTimeQueries[UHRenderPassTypes::MotionPass]->EndTimeStamp(GraphBuilder.GetCmdList());
#endif

	GraphicInterface->EndCmdDebug(GraphBuilder.GetCmdList());
}