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
	, bIsMoveable(false)
	, RendererBound(BoundingBox())
#if WITH_EDITOR
	, bIsVisibleEditor(true)
#endif
	, MeshId(UUID())
	, MaterialId(UUID())
	, SquareDistanceToMainCam(0.0f)
{
	SetMaterial(MaterialCache);
	SetName("MeshRendererComponent" + std::to_string(GetId()));
	ObjectClassIdInternal = ClassId;
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
		SetMotionDirties(true);

		// update renderer bound as well
		const BoundingBox& MeshBound = MeshCache->GetMeshBound();
		const XMFLOAT4X4& World = GetWorldMatrix();
		XMMATRIX W = XMLoadFloat4x4(&World);
		MeshBound.Transform(RendererBound, XMMatrixTranspose(W));

		// calculate world bound matrix, this matrix is mainly for bounding box rendering (e.g. occlusion test), rotation isn't needed
		W = XMMatrixTranspose(XMMatrixTranslation(RendererBound.Center.x, RendererBound.Center.y, RendererBound.Center.z))
			* XMMatrixTranspose(XMMatrixScaling(RendererBound.Extents.x * 2.0f, RendererBound.Extents.y * 2.0f, RendererBound.Extents.z * 2.0f));
		XMStoreFloat4x4(&WorldBoundMatrix, W);
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

void UHMeshRendererComponent::CalculateSquareDistanceTo(const XMFLOAT3 Position)
{
	// this roughly does the calculation with the center point of a bound
	// to have more precise result, find the closest point on a bound first
	const XMFLOAT3 Center = GetRendererBound().Center;
	SquareDistanceToMainCam = MathHelpers::VectorDistanceSqr(Center, Position);
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
	CB.GWorld = GetWorldMatrix();
	CB.GWorldIT = GetWorldMatrixIT();
	CB.GPrevWorld = GetPrevWorldMatrix();
	CB.InstanceIndex = GetBufferDataIndex();

	return CB;
}

BoundingBox UHMeshRendererComponent::GetRendererBound() const
{
	return RendererBound;
}

float UHMeshRendererComponent::GetSquareDistanceToMainCam() const
{
	return SquareDistanceToMainCam;
}

XMFLOAT4X4 UHMeshRendererComponent::GetWorldBoundMatrix() const
{
	return WorldBoundMatrix;
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

void UHMeshRendererComponent::SetMoveable(bool bMoveable)
{
	bIsMoveable = bMoveable;
}

bool UHMeshRendererComponent::IsMoveable() const
{
	return bIsMoveable;
}

#if WITH_EDITOR
UHDebugBoundConstant UHMeshRendererComponent::GetDebugBoundConst() const
{
	UHDebugBoundConstant BoundConst{};
	BoundConst.BoundType = UHDebugBoundType::DebugBox;
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
	/*if (ImGui::BeginCombo("Mesh", (MeshCache) ? MeshCache->GetName().c_str() : "##"))
	{
		const std::vector<UHMesh*>& Meshes = AssetMgr->GetUHMeshes();
		for (size_t Idx = 0; Idx < Meshes.size(); Idx++)
		{
			bool bIsSelected = (MeshCache == Meshes[Idx]);
			if (ImGui::Selectable(Meshes[Idx]->GetName().c_str(), bIsSelected))
			{
				MeshCache = Meshes[Idx];
			}
		}
		ImGui::EndCombo();
	}*/

	// @TODO: Make mesh editable
	ImGui::NewLine();
	if (MeshCache)
	{
		ImGui::Text(("Mesh in use: " + MeshCache->GetName()).c_str());
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