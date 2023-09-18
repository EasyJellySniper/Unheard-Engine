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
	return bIsVisible
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

void UHMeshRendererComponent::OnPropertyChange(std::string PropertyName)
{
	UHTransformComponent::OnPropertyChange(PropertyName);

	const UHBoolDetailGUI* BoolGUI = static_cast<const UHBoolDetailGUI*>(DetailView->GetGUI(PropertyName));
	const UHDropListDetailGUI* DetailGUI = static_cast<const UHDropListDetailGUI*>(DetailView->GetGUI(PropertyName));
	const UHAssetManager* AssetMgr = UHAssetManager::GetAssetMgrEditor();

	if (PropertyName == "IsVisible")
	{
		bIsVisibleEditor = BoolGUI->GetValue();
		SetRayTracingDirties(true);
	}
	else if (PropertyName == "Mesh")
	{
		// switch mesh cache, mark world dirty for updating bounds
		MeshCache = AssetMgr->GetMesh(UHUtilities::ToStringA(DetailGUI->GetValue()));
		bIsWorldDirty = true;
	}
	else if (PropertyName == "Material")
	{
		// remove old reference and add the new one
		UHMaterial* OldMat = MaterialCache;
		UHMaterial* NewMat = AssetMgr->GetMaterial(UHUtilities::ToStringA(DetailGUI->GetValue()));
		MaterialCache = NewMat;
		UHDeferredShadingRenderer::GetRendererEditorOnly()->OnRendererMaterialChanged(this, OldMat, NewMat);

		// mark dirties
		SetRenderDirties(true);
		SetRayTracingDirties(true);
		SetMotionDirties(true);
	}
}

void UHMeshRendererComponent::OnGenerateDetailView(HWND ParentWnd)
{
	UHTransformComponent::OnGenerateDetailView(ParentWnd);

	DetailView = MakeUnique<UHDetailView>("Mesh Renderer");
	DetailView->OnGenerateDetailView<UHMeshRendererComponent>(this, ParentWnd, DetailStartHeight);

	// init material and mesh list
	UHDropListDetailGUI* MeshListGUI = static_cast<UHDropListDetailGUI*>(DetailView->GetGUI("Mesh"));
	UHDropListDetailGUI* MaterialListGUI = static_cast<UHDropListDetailGUI*>(DetailView->GetGUI("Material"));
	const UHAssetManager* AssetMgr = UHAssetManager::GetAssetMgrEditor();

	std::vector<std::wstring> MeshList;
	for (const UHMesh* Mesh : AssetMgr->GetUHMeshes())
	{
		MeshList.push_back(UHUtilities::ToStringW(Mesh->GetName()));
	}

	std::vector<std::wstring> MaterialList;
	for (const UHMaterial* Mat : AssetMgr->GetMaterials())
	{
		MaterialList.push_back(UHUtilities::ToStringW(Mat->GetName()));
	}

	MeshListGUI->InitList(MeshList);
	if (MeshCache)
	{
		MeshListGUI->SetValue(UHUtilities::ToStringW(MeshCache->GetName()));
	}

	MaterialListGUI->InitList(MaterialList);
	if (MaterialCache)
	{
		MaterialListGUI->SetValue(UHUtilities::ToStringW(MaterialCache->GetName()));
	}

	UH_SYNC_DETAIL_VALUE("IsVisible", bIsVisibleEditor)
}

bool UHMeshRendererComponent::IsVisibleInEditor() const
{
	return bIsVisibleEditor;
}
#endif