#include "PreviewScene.h"

#if WITH_EDITOR
#include "../../Runtime/Renderer/RenderBuilder.h"

UHPreviewScene::UHPreviewScene(UHGraphic* InGraphic, UHPreviewSceneType InType)
	: Gfx(InGraphic)
	, LastMousePos(ImVec2())
	, CurrentMousePos(ImVec2())
	, PreviewRT(nullptr)
	, PreviewExtent(VkExtent2D())
	, PreviewFrameBuffer(nullptr)
	, PreviewSceneType(InType)
	, CurrentMesh(nullptr)
	, PreviewCameraSpeed(5.0f)
{
	if (InType == MeshPreview)
	{
		PreviewExtent.width = 512;
		PreviewExtent.height = 512;
	}

	// create preview RT
	PreviewRT = Gfx->RequestRenderTexture("PreviewRT", PreviewExtent, UH_FORMAT_RGBA8_SRGB);
	PreviewDepth = Gfx->RequestRenderTexture("PreviewDepthRT", PreviewExtent, UH_FORMAT_D32F);

	UHTransitionInfo Transition = UHTransitionInfo();
	Transition.FinalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	PreviewRenderPass = Gfx->CreateRenderPass(PreviewRT, Transition, PreviewDepth);

	std::vector<VkImageView> PreviewViews = { PreviewRT->GetImageView(), PreviewDepth->GetImageView() };
	PreviewFrameBuffer = Gfx->CreateFrameBuffer(PreviewViews, PreviewRenderPass.RenderPass, PreviewExtent);
	PreviewRenderPass.FrameBuffer = PreviewFrameBuffer;

	// init debug view shaders
	MeshPreviewShader = MakeUnique<UHMeshPreviewShader>(Gfx, "MeshPreviewShader", PreviewRenderPass.RenderPass);
	PreviewCamera = MakeUnique<UHCameraComponent>();
	PreviewCamera->Update();

	MeshPreviewData = Gfx->RequestRenderBuffer<float>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	MeshPreviewShader->BindConstant(MeshPreviewData, 0, 0);
}

void UHPreviewScene::Release()
{
	MeshPreviewShader->Release();
	MeshPreviewData->Release();

	VkDevice LogicalDevice = Gfx->GetLogicalDevice();
	Gfx->RequestReleaseRT(PreviewRT);
	Gfx->RequestReleaseRT(PreviewDepth);
	vkDestroyFramebuffer(LogicalDevice, PreviewFrameBuffer, nullptr);
	vkDestroyRenderPass(LogicalDevice, PreviewRenderPass.RenderPass, nullptr);
}

void UHPreviewScene::Render(bool bIsActive)
{
	if (PreviewSceneType == MeshPreview && CurrentMesh == nullptr)
	{
		return;
	}

	CurrentMousePos = ImGui::GetMousePos();
	if (bIsActive)
	{
		UpdateCamera();
	}

	XMFLOAT4X4 ViewProj = PreviewCamera->GetViewProjMatrixNonJittered();
	MeshPreviewData->UploadData(&ViewProj, 0);
	MeshPreviewShader->BindConstant(MeshPreviewData, 0, 0);

	VkCommandBuffer PreviewCmd = Gfx->BeginOneTimeCmd();
	UHRenderBuilder PreviewBuilder(Gfx, PreviewCmd);

	Gfx->BeginCmdDebug(PreviewCmd, "Drawing preview mesh");

	VkClearValue ClearColor = { {0.0f,0.0f,0.8f,1.0f} };
	VkClearValue ClearDepth = { 0.0f,0 };
	const std::vector<VkClearValue> ClearValues = { ClearColor , ClearDepth };
	PreviewBuilder.BeginRenderPass(PreviewRenderPass, PreviewExtent, ClearValues);

	// render based on the preview scene type
	PreviewBuilder.SetViewport(PreviewExtent);
	PreviewBuilder.SetScissor(PreviewExtent);
	PreviewBuilder.BindVertexBuffer(CurrentMesh->GetPositionBuffer()->GetBuffer());
	PreviewBuilder.BindIndexBuffer(CurrentMesh);
	PreviewBuilder.BindDescriptorSet(MeshPreviewShader->GetPipelineLayout(), MeshPreviewShader->GetDescriptorSet(0));

	UHGraphicState* State = MeshPreviewShader->GetState();
	PreviewBuilder.BindGraphicState(State);
	PreviewBuilder.DrawIndexed(CurrentMesh->GetIndicesCount());

	PreviewBuilder.EndRenderPass();

	Gfx->EndCmdDebug(PreviewCmd);
	Gfx->EndOneTimeCmd(PreviewCmd);

	LastMousePos = CurrentMousePos;
}

void UHPreviewScene::SetMesh(UHMesh* InMesh)
{
	CurrentMesh = InMesh;
	CurrentMesh->CreateGPUBuffers(Gfx);

	// make camera in front of mesh's center
	PreviewCamera->SetRotation(XMFLOAT3(0, 0, 0));
	PreviewCamera->SetPosition(CurrentMesh->GetMeshCenter() - PreviewCamera->GetForward() * 5);
}

UHRenderTexture* UHPreviewScene::GetPreviewRT() const
{
	return PreviewRT;
}

void UHPreviewScene::UpdateCamera()
{
	// set aspect ratio of default camera
	PreviewCamera->SetAspect(PreviewExtent.width / static_cast<float>(PreviewExtent.height));
	const float CameraMoveSpd = PreviewCameraSpeed * ImGui::GetIO().DeltaTime;
	const float MouseRotSpeed = CameraMoveSpd;

	if (ImGui::IsKeyDown(ImGuiKey_W))
	{
		PreviewCamera->Translate(XMFLOAT3(0, 0, CameraMoveSpd));
	}

	if (ImGui::IsKeyDown(ImGuiKey_S))
	{
		PreviewCamera->Translate(XMFLOAT3(0, 0, -CameraMoveSpd));
	}

	if (ImGui::IsKeyDown(ImGuiKey_A))
	{
		PreviewCamera->Translate(XMFLOAT3(-CameraMoveSpd, 0, 0));
	}

	if (ImGui::IsKeyDown(ImGuiKey_D))
	{
		PreviewCamera->Translate(XMFLOAT3(CameraMoveSpd, 0, 0));
	}

	if (ImGui::IsKeyDown(ImGuiKey_E))
	{
		PreviewCamera->Translate(XMFLOAT3(0, CameraMoveSpd, 0));
	}

	if (ImGui::IsKeyDown(ImGuiKey_Q))
	{
		PreviewCamera->Translate(XMFLOAT3(0, -CameraMoveSpd, 0));
	}

	if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
	{
		const float X = static_cast<float>(CurrentMousePos.x - LastMousePos.x) * MouseRotSpeed;
		const float Y = static_cast<float>(CurrentMousePos.y - LastMousePos.y) * MouseRotSpeed;

		PreviewCamera->Rotate(XMFLOAT3(Y, 0, 0));
		PreviewCamera->Rotate(XMFLOAT3(0, X, 0), UHTransformSpace::World);
	}

	PreviewCamera->Update();
}

#endif