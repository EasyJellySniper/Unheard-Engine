#include "MeshRenderer.h"

UHMeshRendererComponent::UHMeshRendererComponent(UHMesh* InMesh, UHMaterial* InMaterial)
	: MeshCache(InMesh)
	, MaterialCache(InMaterial)
	, bIsVisible(true)
	, RendererBound(BoundingBox())
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
	UHObjectConstants CB;
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
	return bIsVisible;
}