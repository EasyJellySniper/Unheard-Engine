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

private:
	UHMesh* MeshCache;
	UHMaterial* MaterialCache;
};