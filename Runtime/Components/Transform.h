#pragma once
#include "Component.h"
#include "../Classes/Types.h"

static XMFLOAT3 GWorldRight = { 1.0f, 0.0f, 0.0f };
static XMFLOAT3 GWorldUp = { 0.0f, 1.0f, 0.0f };
static XMFLOAT3 GWorldForward = { 0.0f, 0.0f, 1.0f };

enum UHTransformSpace
{
	Local = 0,
	World
};

// Transform component of UH engine, some other components might inherit from this
// based on left coordinate system
// while rendering related stuffs will be convert to RH to fit Vulkan spec
// +X: right
// +Y: down
// +Z: forward
class UHTransformComponent : public UHComponent
{
public:
	UHTransformComponent();
	virtual void Update() override;

	void Translate(XMFLOAT3 InDelta, UHTransformSpace InSpace = UHTransformSpace::Local);
	void Rotate(XMFLOAT3 InDelta, UHTransformSpace InSpace = UHTransformSpace::Local);

	void SetScale(XMFLOAT3 InScale);
	void SetPosition(XMFLOAT3 InPos);
	void SetRotation(XMFLOAT3 InEulerRot);

	XMFLOAT4X4 GetWorldMatrix() const;
	XMFLOAT4X4 GetPrevWorldMatrix() const;
	XMFLOAT4X4 GetWorldMatrixIT() const;
	XMFLOAT4X4 GetRotationMatrix() const;
	XMFLOAT3 GetRight() const;
	XMFLOAT3 GetUp() const;
	XMFLOAT3 GetForward() const;
	XMFLOAT3 GetPosition() const;

	// is transform changed
	bool IsWorldDirty() const;
	bool IsTransformChanged() const;

protected:
	XMFLOAT3 Scale;
	XMFLOAT3 Position;
	XMFLOAT3 RotationEuler;

	// direction vectors
	XMFLOAT3 Right;
	XMFLOAT3 Up;
	XMFLOAT3 Forward;

	bool bTransformChanged;

	// dirty flag
	bool bIsWorldDirty;

private:
	// world matrix, also store previous frame's world matrix
	XMFLOAT4X4 WorldMatrix;
	XMFLOAT4X4 PrevWorldMatrix;

	// world matrix IT
	XMFLOAT4X4 WorldMatrixIT;

	// rotation only matrix
	XMFLOAT4X4 RotationMatrix;
};