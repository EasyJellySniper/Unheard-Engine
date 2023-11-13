#include "MeshRenderer.h"
#if WITH_EDITOR
#include "../Engine/Asset.h"
#include "../Renderer/DeferredShadingRenderer.h"
#endif

UHMeshRendererComponent::UHMeshRendererComponent(UHMesh* InMesh, UHMaterial* InMaterial)
	: MeshCache(InMesh)
	, MaterialCache(InMaterial)
	, bIsVisible(true)
	, RendererBound(BoundingBox())
#if WITH_EDITOR
	, bIsVisibleEditor(true)
#endif
{
	MaterialCache->AddReferenceObject(this);
	SetName("MeshRendererComponent" + std::to_string(GetId()));
}

void UHMeshRendererComponent::Update()
{
	if (!bIsEnabled)
	{
		return;
	}

	// make sure transform is up-to-date
	UHTransformComponent::Update();

	// mark frame dirty if transform changed, so system will update data to GPU and do related updates
	if (IsTransformChanged())
	{
		SetRenderDirties(true);
		SetRayTracingDirties(true);
		SetMotionDirties(true);

		// update renderer bound as well
		const BoundingBox& MeshBound = MeshCache->GetMeshBound();
		const XMFLOAT4X4& World = GetWorldMatrix();
		const XMMATRIX W = XMLoadFloat4x4(&World);
		MeshBound.Transform(RendererBound, XMMatrixTranspose(W));
	}
}

UHMesh* UHMeshRendererComponent::GetMesh() const
{
	return MeshCache;
}

UHMaterial* UHMeshRendererComponent::GetMaterial() const
{
	return MaterialCache;
}

UHObjectConstants UHMeshRendererComponent::GetConstants() const
{
	// fill CB and return
	UHObjectConstants CB{};
	CB.UHWorld = GetWorldMatrix();
	CB.UHWorldIT = GetWorldMatrixIT();
	CB.UHPrevWorld = GetPrevWorldMatrix();
	CB.InstanceIndex = GetBufferDataIndex();

	return CB;
}

BoundingBox UHMeshRendererComponent::GetRendererBound() const
{
	return RendererBound;
}

void UHMeshRendererComponent::SetVisible(bool bVisible)
{
	bIsVisible = bVisible;
}

bool UHMeshRendererComponent::IsVisible() const
{
	return bIsVisible && bIsEnabled
#if WITH_EDITOR
		&& bIsVisibleEditor
#endif
		;
}

#if WITH_EDITOR
UHDebugBoundConstant UHMeshRendererComponent::GetDebugBoundConst() const
{
	UHDebugBoundConstant BoundConst{};
	BoundConst.BoundType = DebugBox;
	BoundConst.Position = GetRendererBound().Center;
	BoundConst.Extent = GetRendererBound().Extents;
	BoundConst.Color = XMFLOAT3(1, 1, 0);

	return BoundConst;
}

void UHMeshRendererComponent::OnGenerateDetailView()
{
	UHTransformComponent::OnGenerateDetailView();
	ImGui::NewLine();
	ImGui::Text("------ Mesh Renderer ------");

	ImGui::Checkbox("IsVisible", &bIsVisibleEditor);

	// init material and mesh list
	const UHAssetManager* AssetMgr = UHAssetManager::GetAssetMgrEditor();

	// mesh list
	if (ImGui::BeginCombo("Mesh", (MeshCache) ? MeshCache->GetName().c_str() : "##"))
	{
		const std::vector<UHMesh*>& Meshes = AssetMgr->GetUHMeshes();
		for (size_t Idx = 0; Idx < Meshes.size(); Idx++)
		{
			bool bIsSelected = (MeshCache == Meshes[Idx]);
			if (ImGui::Selectable(Meshes[Idx]->GetName().c_str(), bIsSelected))
			{
				MeshCache = Meshes[Idx];
				bIsWorldDirty = true;
			}
		}
		ImGui::EndCombo();
	}

	// material list
	if (ImGui::BeginCombo("Material", (MaterialCache) ? MaterialCache->GetName().c_str() : "##"))
	{
		const std::vector<UHMaterial*>& Materials = AssetMgr->GetMaterials();
		for (size_t Idx = 0; Idx < Materials.size(); Idx++)
		{
			bool bIsSelected = (MaterialCache == Materials[Idx]);
			if (ImGui::Selectable(Materials[Idx]->GetName().c_str(), bIsSelected))
			{
				UHMaterial* OldMat = MaterialCache;
				UHMaterial* NewMat = Materials[Idx];
				MaterialCache = NewMat;
				UHDeferredShadingRenderer::GetRendererEditorOnly()->OnRendererMaterialChanged(this, OldMat, NewMat);

				SetRenderDirties(true);
				SetRayTracingDirties(true);
				SetMotionDirties(true);
			}
		}

		ImGui::EndCombo();
	}
}

bool UHMeshRendererComponent::IsVisibleInEditor() const
{
	return bIsVisibleEditor;
}
#endif