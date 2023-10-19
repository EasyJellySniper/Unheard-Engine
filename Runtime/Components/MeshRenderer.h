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
	UHMeshRendererComponent(UHMesh* InMesh, UHMaterial* InMaterial);
	virtual void Update() override;

	UHMesh* GetMesh() const;
	UHMaterial* GetMaterial() const;
	UHObjectConstants GetConstants() const;
	BoundingBox GetRendererBound() const;

	void SetVisible(bool bVisible);
	bool IsVisible() const;

#if WITH_EDITOR
	virtual UHDebugBoundConstant GetDebugBoundConst() const override;
	virtual void OnGenerateDetailView() override;

	bool IsVisibleInEditor() const;
#endif

private:
	UHMesh* MeshCache;
	UHMaterial* MaterialCache;

	bool bIsVisible;
	BoundingBox RendererBound;

#if WITH_EDITOR
	bool bIsVisibleEditor;
#endif
};