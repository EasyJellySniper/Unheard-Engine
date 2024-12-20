#pragma once
#include "Component.h"
#include "../Classes/Types.h"
#include <optional>

const XMFLOAT3 GWorldRight = { 1.0f, 0.0f, 0.0f };
const XMFLOAT3 GWorldUp = { 0.0f, 1.0f, 0.0f };
const XMFLOAT3 GWorldForward = { 0.0f, 0.0f, 1.0f };
static std::optional<XMVECTOR> GWorldRightVec;
static std::optional<XMVECTOR> GWorldUpVec;
static std::optional<XMVECTOR> GWorldForwardVec;

enum class UHTransformSpace
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
	STATIC_CLASS_ID(15385393)
	UHTransformComponent();
	virtual void Update() override;
	virtual void OnSave(std::ofstream& OutStream) override;
	virtual void OnLoad(std::ifstream& InStream) override;

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
	XMFLOAT3 GetScale() const;

	// is transform changed
	bool IsWorldDirty() const;
	bool IsTransformChanged() const;

#if WITH_EDITOR
	virtual void OnGenerateDetailView() override;
	XMFLOAT3 GetRotationEuler();
#endif

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
	bool bIsFirstFrame;

	// rotation only matrix
	XMFLOAT4X4 RotationMatrix;
private:
	// world matrix, also store previous frame's world matrix
	XMFLOAT4X4 WorldMatrix;
	XMFLOAT4X4 PrevWorldMatrix;

	// world matrix IT
	XMFLOAT4X4 WorldMatrixIT;
};