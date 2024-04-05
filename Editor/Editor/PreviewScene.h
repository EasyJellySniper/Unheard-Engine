#pragma once
#include "../../UnheardEngine.h"

#if WITH_EDITOR
#include "../../../Runtime/Engine/Graphic.h"
#include "../../../Runtime/Renderer/ShaderClass/MeshPreviewShader.h"
#include "../../Runtime/Components/Camera.h"

enum class UHPreviewSceneType
{
	MeshPreview,
};

// preview scene class for editor use, this basically create another set of surface/swap chain from gfx
class UHPreviewScene
{
public:
	UHPreviewScene(UHGraphic* InGraphic, UHPreviewSceneType InType);
	void Release();
	void Render(bool bIsActive);

	void SetMesh(UHMesh* InMesh);
	UHRenderTexture* GetPreviewRT() const;

private:
	void UpdateCamera();

	UHGraphic* Gfx;
	UHRenderTexture* PreviewRT;
	UHRenderTexture* PreviewDepth;
	VkFramebuffer PreviewFrameBuffer;
	UHRenderPassObject PreviewRenderPass;
	VkExtent2D PreviewExtent;
	UHPreviewSceneType PreviewSceneType;

	// shaders
	UniquePtr<UHMeshPreviewShader> MeshPreviewShader;
	UniquePtr<UHCameraComponent> PreviewCamera;
	UniquePtr<UHRenderBuffer<float>> MeshPreviewData;

	UHMesh* CurrentMesh;
	ImVec2 CurrentMousePos;
	ImVec2 LastMousePos;
	float PreviewCameraSpeed;
};

#endif