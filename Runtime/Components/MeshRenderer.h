#pragma once
#include "../Classes/Mesh.h"
#include "../Classes/Material.h"
#include "Transform.h"
#include "../Renderer/RenderingTypes.h"

// mesh renderer component of UH engine, which holds a mesh and a material refernce
// for now, a mesh renderer simply uses 1 on 1 mapping to mesh/materal. Which means submeshes or multiple materials are not allowed.
class UHMeshRendererComponent : public UHTransformComponent, public UHRenderState
{
public:
	STATIC_CLASS_ID(85533359)
	UHMeshRendererComponent();
	UHMeshRendererComponent(UHMesh* InMesh, UHMaterial* InMaterial);
	virtual void Update() override;
	virtual void OnSave(std::ofstream& OutStream) override;
	virtual void OnLoad(std::ifstream& InStream) override;
	virtual void OnPostLoad(UHAssetManager* InAssetMgr) override;

	void SetMesh(UHMesh* InMesh);
	void SetMaterial(UHMaterial* InMaterial);
	void CalculateSquareDistanceTo(const XMFLOAT3 Position);

	UHMesh* GetMesh() const;
	UHMaterial* GetMaterial() const;
	UHObjectConstants GetConstants() const;
	BoundingBox GetRendererBound() const;
	float GetSquareDistanceToMainCam() const;
	XMFLOAT4X4 GetWorldBoundMatrix() const;

	void SetVisible(bool bVisible);
	bool IsVisible() const;

	void SetMoveable(bool bMoveable);
	bool IsMoveable() const;

#if WITH_EDITOR
	virtual UHDebugBoundConstant GetDebugBoundConst() const override;
	virtual void OnGenerateDetailView() override;

	bool IsVisibleInEditor() const;
#endif

private:
	UHMesh* MeshCache;
	UUID MeshId;
	UHMaterial* MaterialCache;
	UUID MaterialId;

	bool bIsVisible;
	bool bIsMoveable;
	BoundingBox RendererBound;
	float SquareDistanceToMainCam;

	XMFLOAT4X4 WorldBoundMatrix;

#if WITH_EDITOR
	bool bIsVisibleEditor;
#endif
};