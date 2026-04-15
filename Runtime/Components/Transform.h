#pragma once
#include "Component.h"
#include "../Classes/Math.h"
#include <optional>

const UHVector3 GWorldRight = { 1.0f, 0.0f, 0.0f };
const UHVector3 GWorldUp = { 0.0f, 1.0f, 0.0f };
const UHVector3 GWorldForward = { 0.0f, 0.0f, 1.0f };

enum class UHTransformSpace : uint32_t
{
	Local = 0,
	World
};

// Transform component of UH engine, some other components might inherit from this
// based on left coordinate system
// while rendering related stuffs will be convert to RH to fit Vulkan spec
// +X: right
// +Y: up
// +Z: forward
class UHTransformComponent : public UHComponent
{
public:
	STATIC_CLASS_ID(15385393)
	UHTransformComponent();
	virtual void Update() override;
	virtual void OnSave(std::ofstream& OutStream) override;
	virtual void OnLoad(std::ifstream& InStream) override;

	void Translate(UHVector3 InDelta, UHTransformSpace InSpace = UHTransformSpace::Local);
	void Rotate(UHVector3 InDelta, UHTransformSpace InSpace = UHTransformSpace::Local);

	void SetScale(UHVector3 InScale);
	void SetPosition(UHVector3 InPos);
	void SetRotation(UHVector3 InEulerRot);

	UHMatrix4x4 GetWorldMatrix() const;
	UHMatrix4x4 GetPrevWorldMatrix() const;
	UHMatrix4x4 GetWorldMatrixIT() const;
	UHMatrix4x4 GetRotationMatrix() const;
	UHVector3 GetRight() const;
	UHVector3 GetUp() const;
	UHVector3 GetForward() const;
	UHVector3 GetPosition() const;
	UHVector3 GetScale() const;
	UHVector3 GetRotationEuler();

	// is transform changed
	bool IsWorldDirty() const;
	bool IsTransformChanged() const;

#if WITH_EDITOR
	virtual void OnGenerateDetailView() override;
#endif

protected:
	UHVector3 Scale;
	UHVector3 Position;
	UHVector3 RotationEuler;

	// direction vectors
	UHVector3 Right;
	UHVector3 Up;
	UHVector3 Forward;

	bool bTransformChanged;

	// dirty flag
	bool bIsWorldDirty;
	bool bIsFirstFrame;

	// rotation only matrix
	UHMatrix4x4 RotationMatrix;
private:
	// world matrix, also store previous frame's world matrix
	UHMatrix4x4 WorldMatrix;
	UHMatrix4x4 PrevWorldMatrix;

	// world matrix IT
	UHMatrix4x4 WorldMatrixIT;
};