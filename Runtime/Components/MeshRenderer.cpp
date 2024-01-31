#include "MeshRenderer.h"
#include "../Engine/Asset.h"
#if WITH_EDITOR
#include "../Renderer/DeferredShadingRenderer.h"
#endif

UHMeshRendererComponent::UHMeshRendererComponent()
	: UHMeshRendererComponent(nullptr, nullptr)
{

}

UHMeshRendererComponent::UHMeshRendererComponent(UHMesh* InMesh, UHMaterial* InMaterial)
	: MeshCache(InMesh)
	, MaterialCache(InMaterial)
	, bIsVisible(true)
	, RendererBound(BoundingBox())
#if WITH_EDITOR
	, bIsVisibleEditor(true)
#endif
	, MeshId(UUID())
	, MaterialId(UUID())
{
	SetMaterial(MaterialCache);
	SetName("MeshRendererComponent" + std::to_string(GetId()));
	ComponentClassIdInternal = ClassId;
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

void UHMeshRendererComponent::OnSave(std::ofstream& OutStream)
{
	UHComponent::OnSave(OutStream);
	UHTransformComponent::OnSave(OutStream);

#if WITH_EDITOR
	OutStream.write(reinterpret_cast<const char*>(&bIsVisibleEditor), sizeof(bIsVisibleEditor));
#else
	bool bDummy = true;
	OutStream.write(reinterpret_cast<const char*>(&bDummy), sizeof(bDummy));
#endif

	// mesh cache
	UUID Dummy{};
	if (MeshCache != nullptr)
	{
		UUID Id = MeshCache->GetRuntimeGuid();
		OutStream.write(reinterpret_cast<const char*>(&Id), sizeof(Id));
	}
	else
	{
		OutStream.write(reinterpret_cast<const char*>(&Dummy), sizeof(Dummy));
	}

	// material cache
	if (MaterialCache != nullptr)
	{
		UUID Id = MaterialCache->GetRuntimeGuid();
		OutStream.write(reinterpret_cast<const char*>(&Id), sizeof(Id));
	}
	else
	{
		OutStream.write(reinterpret_cast<const char*>(&Dummy), sizeof(Dummy));
	}
}

void UHMeshRendererComponent::OnLoad(std::ifstream& InStream)
{
	UHComponent::OnLoad(InStream);
	UHTransformComponent::OnLoad(InStream);

#if WITH_EDITOR
	InStream.read(reinterpret_cast<char*>(&bIsVisibleEditor), sizeof(bIsVisibleEditor));
#else
	bool bDummy;
	InStream.read(reinterpret_cast<char*>(&bDummy), sizeof(bDummy));
#endif

	// mesh cache
	InStream.read(reinterpret_cast<char*>(&MeshId), sizeof(MeshId));

	// material cache
	InStream.read(reinterpret_cast<char*>(&MaterialId), sizeof(MaterialId));
}

void UHMeshRendererComponent::OnPostLoad(UHAssetManager* InAssetMgr)
{
	SetMesh((UHMesh*)InAssetMgr->GetAsset(MeshId));
	SetMaterial((UHMaterial*)InAssetMgr->GetAsset(MaterialId));
}

void UHMeshRendererComponent::SetMesh(UHMesh* InMesh)
{
	MeshCache = InMesh;
}

void UHMeshRendererComponent::SetMaterial(UHMaterial* InMaterial)
{
	MaterialCache = InMaterial;
	if (MaterialCache)
	{
		MaterialCache->AddReferenceObject(this);
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

	if (UHMaterial* Mat = GetMaterial())
	{
		CB.UHNeedWorldTBN = Mat->GetMaterialUsages().bIsTangentSpace;
	}

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