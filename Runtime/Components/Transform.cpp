#include "Transform.h"

UHTransformComponent::UHTransformComponent()
	: Scale(1.0f, 1.0f, 1.0f)
	, Position(0.0f, 0.0f, 0.0f)
	, RotationEuler(0.0f, 0.0f, 0.0f)
	, Right(GWorldRight)
	, Up(GWorldUp)
	, Forward(GWorldForward)
	, WorldMatrix(UHMathHelpers::Identity4x4())
	, PrevWorldMatrix(UHMathHelpers::Identity4x4())
	, WorldMatrixIT(UHMathHelpers::Identity4x4())
	, RotationMatrix(UHMathHelpers::Identity4x4())
	, bTransformChanged(true)
	, bIsWorldDirty(true)
	, bIsFirstFrame(true)
{

}

void UHTransformComponent::Update()
{
	bTransformChanged = bIsWorldDirty;

	if (bIsWorldDirty)
	{
		// update TRS matrix
		UHMatrix4x4 W = UHMathHelpers::UHMatrixTranspose(UHMathHelpers::UHMatrixTranslation(Position))
			* UHMathHelpers::UHMatrixTranspose(RotationMatrix)
			* UHMathHelpers::UHMatrixTranspose(UHMathHelpers::UHMatrixScaling(Scale));

		PrevWorldMatrix = WorldMatrix;
		WorldMatrix = W;

		// store inverse transposed world as well
		W = UHMathHelpers::UHMatrixInverse(W);
		WorldMatrixIT = UHMathHelpers::UHMatrixTranspose(W);

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

void UHTransformComponent::OnSave(std::ofstream& OutStream)
{
	OutStream.write(reinterpret_cast<const char*>(&Position), sizeof(Position));
	OutStream.write(reinterpret_cast<const char*>(&RotationEuler), sizeof(RotationEuler));
	OutStream.write(reinterpret_cast<const char*>(&Scale), sizeof(Scale));
}

void UHTransformComponent::OnLoad(std::ifstream& InStream)
{
	InStream.read(reinterpret_cast<char*>(&Position), sizeof(Position));
	InStream.read(reinterpret_cast<char*>(&RotationEuler), sizeof(RotationEuler));
	InStream.read(reinterpret_cast<char*>(&Scale), sizeof(Scale));

	// to refresh the rotation matrix
	SetRotation(RotationEuler);
}

void UHTransformComponent::Translate(UHVector3 InDelta, UHTransformSpace InSpace)
{
	const float Dx = InDelta.X;
	const float Dy = InDelta.Y;
	const float Dz = InDelta.Z;
	UHVector3 P = Position;

	if (InSpace == UHTransformSpace::Local)
	{
		// move along up/right/front
		const UHVector3 R = Right;
		const UHVector3 U = Up;
		const UHVector3 F = Forward;

		P = P + R * Dx + U * Dy + F * Dz;
		Position = P;
	}
	else
	{
		// move along world up/right/front
		const UHVector3 R = GWorldRight;
		const UHVector3 U = GWorldUp;
		const UHVector3 F = GWorldForward;

		P = P + R * Dx + U * Dy + F * Dz;
		Position = P;
	}

	bIsWorldDirty = true;
}

void UHTransformComponent::Rotate(UHVector3 InDelta, UHTransformSpace InSpace)
{
	// perform rotation with matrix, and get euler from it instead of using euler directly (prevent gimbal lock!)
	UHMatrix4x4 OldRot = RotationMatrix;
	UHMatrix4x4 NewRot;

	if (InSpace == UHTransformSpace::Local)
	{
		// for local, rotation around current axis
		const UHMatrix4x4 Rz = UHMathHelpers::UHMatrixRotationAxis(Forward, UHMathHelpers::ToRadians(InDelta.Z));
		const UHMatrix4x4 Rx = UHMathHelpers::UHMatrixRotationAxis(Right, UHMathHelpers::ToRadians(InDelta.X));
		const UHMatrix4x4 Ry = UHMathHelpers::UHMatrixRotationAxis(Up, UHMathHelpers::ToRadians(InDelta.Y));
		NewRot = Rz * Rx * Ry;
		
		// transform vector
		Right = UHMathHelpers::UHVector3TransformNormal(Right, NewRot);
		Up = UHMathHelpers::UHVector3TransformNormal(Up, NewRot);
		Forward = UHMathHelpers::UHVector3TransformNormal(Forward, NewRot);

		// transform matrix
		NewRot = OldRot * NewRot;
		RotationMatrix = NewRot;
	}
	else
	{
		// for world space simply build matrix with delta
		const UHMatrix4x4 R = UHMathHelpers::UHMatrixRotationAxis(GWorldForward, UHMathHelpers::ToRadians(InDelta.Z))
			* UHMathHelpers::UHMatrixRotationAxis(GWorldRight, UHMathHelpers::ToRadians(InDelta.X))
			* UHMathHelpers::UHMatrixRotationAxis(GWorldUp, UHMathHelpers::ToRadians(InDelta.Y));

		// transform matrix
		NewRot = OldRot * R;
		RotationMatrix = NewRot;

		// transform vectors
		Right = UHMathHelpers::UHVector3TransformNormal(Right, R);
		Up = UHMathHelpers::UHVector3TransformNormal(Up, R);
		Forward = UHMathHelpers::UHVector3TransformNormal(Forward, R);
	}

	bIsWorldDirty = true;
#if WITH_EDITOR
	// sync euler for editor
	RotationEuler = GetRotationEuler();
#endif
}

void UHTransformComponent::SetScale(UHVector3 InScale)
{
	Scale = InScale;
	bIsWorldDirty = true;
}

void UHTransformComponent::SetPosition(UHVector3 InPos)
{
	Position = InPos;
	bIsWorldDirty = true;
}

void UHTransformComponent::SetRotation(UHVector3 InEulerRot)
{
	// this should be used when initialization, for other roation it's better to perform on matrix or quaternion
	RotationEuler = InEulerRot;
	RotationMatrix = UHMathHelpers::UHMatrixRotationRollPitchYaw(
		UHMathHelpers::ToRadians(RotationEuler.X),
		UHMathHelpers::ToRadians(RotationEuler.Y),
		UHMathHelpers::ToRadians(RotationEuler.Z)
	);

	// transform direction vectors also
	Right = UHMathHelpers::UHVector3TransformNormal(GWorldRight, RotationMatrix);
	Up = UHMathHelpers::UHVector3TransformNormal(GWorldUp, RotationMatrix);
	Forward = UHMathHelpers::UHVector3TransformNormal(GWorldForward, RotationMatrix);

	bIsWorldDirty = true;
}

UHMatrix4x4 UHTransformComponent::GetWorldMatrix() const
{
	return WorldMatrix;
}

UHMatrix4x4 UHTransformComponent::GetPrevWorldMatrix() const
{
	return PrevWorldMatrix;
}

UHMatrix4x4 UHTransformComponent::GetWorldMatrixIT() const
{
	return WorldMatrixIT;
}

UHMatrix4x4 UHTransformComponent::GetRotationMatrix() const
{
	return RotationMatrix;
}

UHVector3 UHTransformComponent::GetRight() const
{
	return Right;
}

UHVector3 UHTransformComponent::GetUp() const
{
	return Up;
}

UHVector3 UHTransformComponent::GetForward() const
{
	return Forward;
}

UHVector3 UHTransformComponent::GetPosition() const
{
	return Position;
}

UHVector3 UHTransformComponent::GetScale() const
{
	return Scale;
}

UHVector3 UHTransformComponent::GetRotationEuler()
{
	// get euler angle from rotation matrix
	// this function should be used for editor display and not available in runtime
	UHMathHelpers::MatrixToPitchYawRoll(RotationMatrix, RotationEuler.X, RotationEuler.Y, RotationEuler.Z);

	return RotationEuler;
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
	UHComponent::OnGenerateDetailView();

	ImGui::NewLine();
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
#endif