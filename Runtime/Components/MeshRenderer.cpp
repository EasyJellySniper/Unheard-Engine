#include "MeshRenderer.h"

UHMeshRendererComponent::UHMeshRendererComponent(UHMesh* InMesh, UHMaterial* InMaterial)
	: MeshCache(InMesh)
	, MaterialCache(InMaterial)
{

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
	UHObjectConstants CB;
	CB.UHWorld = GetWorldMatrix();
	CB.UHWorldIT = GetWorldMatrixIT();
	CB.UHPrevWorld = GetPrevWorldMatrix();

	return CB;
}