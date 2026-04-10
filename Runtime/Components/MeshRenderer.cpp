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
	, RendererBound(UHBoundingBox())
#if WITH_EDITOR
	, bIsVisibleEditor(true)
#endif
	, MeshId(UHGUID())
	, MaterialId(UHGUID())
	, SquareDistanceToMainCam(0.0f)
	, bIsCameraInsideBound(false)
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
		const UHBoundingBox& MeshBound = MeshCache->GetMeshBound();
		const UHMatrix4x4& World = GetWorldMatrix();
		MeshBound.Transform(RendererBound, UHMathHelpers::UHMatrixTranspose(World));

		// calculate world bound matrix, this matrix is mainly for bounding box rendering (e.g. occlusion test), rotation isn't needed
		WorldBoundMatrix = UHMathHelpers::UHMatrixTranspose(UHMathHelpers::UHMatrixTranslation(RendererBound.Center)
			* UHMathHelpers::UHMatrixScaling(RendererBound.Extents * 2.0f));
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
		UHGUID Id = MeshCache->GetRuntimeGuid();
		OutStream.write(reinterpret_cast<const char*>(&Id), sizeof(Id));
	}
	else
	{
		OutStream.write(reinterpret_cast<const char*>(&Dummy), sizeof(Dummy));
	}

	// material cache
	if (MaterialCache != nullptr)
	{
		UHGUID Id = MaterialCache->GetRuntimeGuid();
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

void UHMeshRendererComponent::CalculateSquareDistanceToCamera(const UHVector3 Position)
{
	// this roughly does the calculation with the center point of a bound
	// to have more precise result, find the closest point on a bound first
	const UHVector3 Center = GetRendererBound().Center;
	SquareDistanceToMainCam = UHMathHelpers::VectorDistanceSqr(Center, Position);

	// check if the camera is inside this renderer bound
	bIsCameraInsideBound = RendererBound.Contains(Position);
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
	CB.WorldPos = RendererBound.Center;
	CB.BoundExtent = RendererBound.Extents;

	return CB;
}

UHBoundingBox UHMeshRendererComponent::GetRendererBound() const
{
	return RendererBound;
}

float UHMeshRendererComponent::GetSquareDistanceToMainCam() const
{
	return SquareDistanceToMainCam;
}

UHMatrix4x4 UHMeshRendererComponent::GetWorldBoundMatrix() const
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

bool UHMeshRendererComponent::IsCameraInsideThisRenderer() const
{
	return bIsCameraInsideBound;
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
	BoundConst.Color = UHVector3(1, 1, 0);

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
	if (ImGui::BeginCombo("Mesh", (MeshCache) ? MeshCache->GetSourcePath().c_str() : "##"))
	{
		const std::vector<UHMesh*>& Meshes = AssetMgr->GetUHMeshes();
		for (size_t Idx = 0; Idx < Meshes.size(); Idx++)
		{
			bool bIsSelected = (MeshCache == Meshes[Idx]);
			if (ImGui::Selectable(Meshes[Idx]->GetSourcePath().c_str(), bIsSelected))
			{
				MeshCache = Meshes[Idx];
				bIsWorldDirty = true;

				// when a mesh changed, it has to recreate mesh shader data and upload renderer instances again
				UHDeferredShadingRenderer::GetRendererEditorOnly()->RecreateMeshShaderData(MaterialCache);
				UHDeferredShadingRenderer::GetRendererEditorOnly()->UploadRendererInstances();
				UHDeferredShadingRenderer::GetRendererEditorOnly()->UpdateDescriptors();
			}
		}
		ImGui::EndCombo();
	}

	// material list
	if (ImGui::BeginCombo("Material", (MaterialCache) ? MaterialCache->GetSourcePath().c_str() : "##"))
	{
		const std::vector<UHMaterial*>& Materials = AssetMgr->GetMaterials();
		for (size_t Idx = 0; Idx < Materials.size(); Idx++)
		{
			bool bIsSelected = (MaterialCache == Materials[Idx]);
			if (ImGui::Selectable(Materials[Idx]->GetSourcePath().c_str(), bIsSelected))
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