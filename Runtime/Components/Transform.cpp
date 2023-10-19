#include "Transform.h"

UHTransformComponent::UHTransformComponent()
	: Scale(1.0f, 1.0f, 1.0f)
	, Position(0.0f, 0.0f, 0.0f)
	, RotationEuler(0.0f, 0.0f, 0.0f)
	, Right(GWorldRight)
	, Up(GWorldUp)
	, Forward(GWorldForward)
	, WorldMatrix(MathHelpers::Identity4x4())
	, PrevWorldMatrix(MathHelpers::Identity4x4())
	, WorldMatrixIT(MathHelpers::Identity4x4())
	, RotationMatrix(MathHelpers::Identity4x4())
	, bTransformChanged(true)
	, bIsWorldDirty(true)
	, bIsFirstFrame(true)
{
	if (!GWorldRightVec.has_value())
	{
		GWorldRightVec = XMLoadFloat3(&GWorldRight);
	}

	if (!GWorldUpVec.has_value())
	{
		GWorldUpVec = XMLoadFloat3(&GWorldUp);
	}

	if (!GWorldForwardVec.has_value())
	{
		GWorldForwardVec = XMLoadFloat3(&GWorldForward);
	}
}

void UHTransformComponent::Update()
{
	bTransformChanged = bIsWorldDirty;

	if (bIsWorldDirty)
	{
		// update TRS matrix
		XMMATRIX W = XMMatrixTranspose(XMMatrixTranslation(Position.x, Position.y, Position.z))
			* XMMatrixTranspose(XMLoadFloat4x4(&RotationMatrix))
			* XMMatrixTranspose(XMMatrixScaling(Scale.x, Scale.y, Scale.z));

		PrevWorldMatrix = WorldMatrix;
		XMStoreFloat4x4(&WorldMatrix, W);

		// store inverse transposed world as well
		XMVECTOR Det = XMMatrixDeterminant(W);
		W = XMMatrixInverse(&Det, W);
		XMStoreFloat4x4(&WorldMatrixIT, XMMatrixTranspose(W));

		bIsWorldDirty = false;
	}
	else if (bIsFirstFrame)
	{
		// if it's not moving, sync the prev world matrix at least
		// otherwise the objects that only get updated at the first frame will not have previous world info
		PrevWorldMatrix = WorldMatrix;
		bIsFirstFrame = false;
	}
}

void UHTransformComponent::Translate(XMFLOAT3 InDelta, UHTransformSpace InSpace)
{
	const XMVECTOR Dx = XMVectorReplicate(InDelta.x);
	const XMVECTOR Dy = XMVectorReplicate(InDelta.y);
	const XMVECTOR Dz = XMVectorReplicate(InDelta.z);
	XMVECTOR P = XMLoadFloat3(&Position);

	if (InSpace == UHTransformSpace::Local)
	{
		// move along up/right/front
		const XMVECTOR R = XMLoadFloat3(&Right);
		const XMVECTOR U = XMLoadFloat3(&Up);
		const XMVECTOR F = XMLoadFloat3(&Forward);

		P += Dx * R + Dy * U + Dz * F;
		XMStoreFloat3(&Position, P);
	}
	else
	{
		// move along world up/right/front
		const XMVECTOR R = XMLoadFloat3(&GWorldRight);
		const XMVECTOR U = XMLoadFloat3(&GWorldUp);
		const XMVECTOR F = XMLoadFloat3(&GWorldForward);

		P += Dx * R + Dy * U + Dz * F;
		XMStoreFloat3(&Position, P);
	}

	bIsWorldDirty = true;
}

void UHTransformComponent::Rotate(XMFLOAT3 InDelta, UHTransformSpace InSpace)
{
	// perform rotation with matrix, and get euler from it instead of using euler directly (prevent gimbal lock!)
	XMMATRIX OldRot = XMLoadFloat4x4(&RotationMatrix);
	XMMATRIX NewRot;

	if (InSpace == UHTransformSpace::Local)
	{
		// for local, rotation around current axis
		const XMMATRIX Rz = XMMatrixRotationAxis(XMLoadFloat3(&Forward), XMConvertToRadians(InDelta.z));
		const XMMATRIX Rx = XMMatrixRotationAxis(XMLoadFloat3(&Right), XMConvertToRadians(InDelta.x));
		const XMMATRIX Ry = XMMatrixRotationAxis(XMLoadFloat3(&Up), XMConvertToRadians(InDelta.y));
		NewRot = Rz * Rx * Ry;
		
		// transform vector
		XMStoreFloat3(&Right, XMVector3TransformNormal(XMLoadFloat3(&Right), NewRot));
		XMStoreFloat3(&Up, XMVector3TransformNormal(XMLoadFloat3(&Up), NewRot));
		XMStoreFloat3(&Forward, XMVector3TransformNormal(XMLoadFloat3(&Forward), NewRot));

		// transform matrix
		NewRot = OldRot * NewRot;
		XMStoreFloat4x4(&RotationMatrix, NewRot);
	}
	else
	{
		// for world space simply build matrix with delta
		const XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&GWorldForward), XMConvertToRadians(InDelta.z))
			* XMMatrixRotationAxis(XMLoadFloat3(&GWorldRight), XMConvertToRadians(InDelta.x))
			* XMMatrixRotationAxis(XMLoadFloat3(&GWorldUp), XMConvertToRadians(InDelta.y));

		// transform matrix
		NewRot = OldRot * R;
		XMStoreFloat4x4(&RotationMatrix, NewRot);

		// transform vectors
		XMStoreFloat3(&Right, XMVector3TransformNormal(XMLoadFloat3(&Right), R));
		XMStoreFloat3(&Up, XMVector3TransformNormal(XMLoadFloat3(&Up), R));
		XMStoreFloat3(&Forward, XMVector3TransformNormal(XMLoadFloat3(&Forward), R));
	}

	bIsWorldDirty = true;
#if WITH_EDITOR
	// sync euler for editor
	RotationEuler = GetRotationEuler();
#endif
}

void UHTransformComponent::SetScale(XMFLOAT3 InScale)
{
	Scale = InScale;
	bIsWorldDirty = true;
}

void UHTransformComponent::SetPosition(XMFLOAT3 InPos)
{
	Position = InPos;
	bIsWorldDirty = true;
}

void UHTransformComponent::SetRotation(XMFLOAT3 InEulerRot)
{
	// this should be used when initialization, for other roation it's better to perform on matrix or quaternion
	RotationEuler = InEulerRot;
	XMStoreFloat4x4(&RotationMatrix, XMMatrixRotationRollPitchYaw(XMConvertToRadians(RotationEuler.x), XMConvertToRadians(RotationEuler.y), XMConvertToRadians(RotationEuler.z)));

	// transform direction vectors also
	XMStoreFloat3(&Right, XMVector3TransformNormal(XMLoadFloat3(&GWorldRight), XMLoadFloat4x4(&RotationMatrix)));
	XMStoreFloat3(&Up, XMVector3TransformNormal(XMLoadFloat3(&GWorldUp), XMLoadFloat4x4(&RotationMatrix)));
	XMStoreFloat3(&Forward, XMVector3TransformNormal(XMLoadFloat3(&GWorldForward), XMLoadFloat4x4(&RotationMatrix)));

	bIsWorldDirty = true;
}

XMFLOAT4X4 UHTransformComponent::GetWorldMatrix() const
{
	return WorldMatrix;
}

XMFLOAT4X4 UHTransformComponent::GetPrevWorldMatrix() const
{
	return PrevWorldMatrix;
}

XMFLOAT4X4 UHTransformComponent::GetWorldMatrixIT() const
{
	return WorldMatrixIT;
}

XMFLOAT4X4 UHTransformComponent::GetRotationMatrix() const
{
	return RotationMatrix;
}

XMFLOAT3 UHTransformComponent::GetRight() const
{
	return Right;
}

XMFLOAT3 UHTransformComponent::GetUp() const
{
	return Up;
}

XMFLOAT3 UHTransformComponent::GetForward() const
{
	return Forward;
}

XMFLOAT3 UHTransformComponent::GetPosition() const
{
	return Position;
}

XMFLOAT3 UHTransformComponent::GetScale() const
{
	return Scale;
}

bool UHTransformComponent::IsWorldDirty() const
{
	return bIsWorldDirty || bIsFirstFrame;
}

bool UHTransformComponent::IsTransformChanged() const
{
	return bTransformChanged;
}

#if WITH_EDITOR
void UHTransformComponent::OnGenerateDetailView()
{
	ImGui::Text("------ Transform ------");
	if (ImGui::InputFloat3("Position", (float*)&Position))
	{
		SetPosition(Position);
	}

	if (ImGui::InputFloat3("Rotation", (float*)&RotationEuler))
	{
		SetRotation(RotationEuler);
	}

	if (ImGui::InputFloat3("Scale", (float*)&Scale))
	{
		SetScale(Scale);
	}
}

XMFLOAT3 UHTransformComponent::GetRotationEuler()
{
	// get euler angle from rotation matrix
	// this function should be used for editor display and not available in runtime
	MathHelpers::MatrixToPitchYawRoll(RotationMatrix, RotationEuler.x, RotationEuler.y, RotationEuler.z);

	return RotationEuler;
}

#endif