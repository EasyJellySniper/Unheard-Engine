#include "DetailView.h"

#if WITH_EDITOR
#include "../../Runtime/Components/Transform.h"
#include "../../Runtime/Components/Camera.h"
#include "../../Runtime/Components/SkyLight.h"
#include "../../Runtime/Components/Light.h"
#include "../../Runtime/Components/MeshRenderer.h"

template <typename T>
void UHDetailView::OnGenerateDetailView(T* InComp, HWND ParentWnd, int32_t& StartHeight)
{
	// get all properties from a class and generate GUI
	UHReflectionTypeDescriptor RefDesc = UHReflection::Get<T>();
	const int32_t StartX = 5;
	const int32_t StartY = 25 + StartHeight;
	const int32_t Gap = 30;
	int32_t EndY = StartY;

	UHGUIProperty GUIProp = UHGUIProperty().SetParent(ParentWnd);
	for (int32_t Idx = 0; Idx < static_cast<int32_t>(RefDesc.MemberName.size()); Idx++)
	{
		UniquePtr<UHDetailGUIBase> GUI = GetDetailGUI(RefDesc.MemberType[Idx], RefDesc.MemberName[Idx], GUIProp.SetPosX(StartX).SetPosY(StartY + Idx * Gap));
		GUI->BindCallback(StdBind(&T::OnPropertyChange, InComp, RefDesc.MemberName[Idx]));
		DetailGUIs.push_back(std::move(GUI));
		EndY += Idx * Gap;
	}

	RECT R;
	GetWindowRect(ParentWnd, &R);
	const int32_t GroupBoxHeight = static_cast<int32_t>(RefDesc.MemberName.size() + 1) * Gap;
	DetailBlock = MakeUnique<UHGroupBox>(DetailViewName, GUIProp.SetPosX(0).SetPosY(StartHeight).SetWidth(R.right - R.left).SetHeight(GroupBoxHeight));
	StartHeight += EndY;
}

template <typename T>
void UHDetailView::SetValue(std::string PropertyName, T InVal)
{
	UHDetailGUIBase* GUI = GetGUI(PropertyName);
	if (GUI == nullptr)
	{
		return;
	}

	if constexpr (std::is_same<T, float>::value)
	{
		((UHFloatDetailGUI*)GUI)->SetValue(InVal);
	}
	else if constexpr (std::is_same<T, XMFLOAT3>::value)
	{
		((UHFloat3DetailGUI*)GUI)->SetValue(InVal);
	}
	else if constexpr (std::is_same<T, bool>::value)
	{
		((UHBoolDetailGUI*)GUI)->SetValue(InVal);
	}
	else if constexpr (std::is_same<T, std::wstring>::value)
	{
		((UHDropListDetailGUI*)GUI)->SetValue(InVal);
	}
	else if constexpr (std::is_same<T, std::string>::value)
	{
		((UHDropListDetailGUI*)GUI)->SetValue(UHUtilities::ToStringW(InVal));
	}
}

template <typename T>
T UHDetailView::GetValue(std::string PropertyName) const
{
	UHDetailGUIBase* GUI = GetGUI(PropertyName);
	if (GUI == nullptr)
	{
		return T();
	}

	if constexpr (std::is_same<T, float>::value)
	{
		return ((UHFloatDetailGUI*)GUI)->GetValue();
	}
	else if constexpr (std::is_same<T, XMFLOAT3>::value)
	{
		return ((UHFloat3DetailGUI*)GUI)->GetValue();
	}
	else if constexpr (std::is_same<T, bool>::value)
	{
		return ((UHBoolDetailGUI*)GUI)->GetValue();
	}
	else if constexpr (std::is_same<T, std::wstring>::value)
	{
		return ((UHDropListDetailGUI*)GUI)->GetValue();
	}
	else if constexpr (std::is_same<T, std::string>::value)
	{
		return UHUtilities::ToStringA(((UHDropListDetailGUI*)GUI)->GetValue());
	}

	return T();
}

UHDetailGUIBase* UHDetailView::GetGUI(std::string InName) const
{
	for (const UniquePtr<UHDetailGUIBase>& GUI : DetailGUIs)
	{
		if (GUI->GetName() == InName)
		{
			return GUI.get();
		}
	}

	return nullptr;
}

template void UHDetailView::OnGenerateDetailView<UHTransformComponent>(UHTransformComponent* InComp, HWND ParentWnd, int32_t& StartHeight);
template void UHDetailView::OnGenerateDetailView<UHCameraComponent>(UHCameraComponent* InComp, HWND ParentWnd, int32_t& StartHeight);
template void UHDetailView::OnGenerateDetailView<UHSkyLightComponent>(UHSkyLightComponent* InComp, HWND ParentWnd, int32_t& StartHeight);
template void UHDetailView::OnGenerateDetailView<UHDirectionalLightComponent>(UHDirectionalLightComponent* InComp, HWND ParentWnd, int32_t& StartHeight);
template void UHDetailView::OnGenerateDetailView<UHPointLightComponent>(UHPointLightComponent* InComp, HWND ParentWnd, int32_t& StartHeight);
template void UHDetailView::OnGenerateDetailView<UHSpotLightComponent>(UHSpotLightComponent* InComp, HWND ParentWnd, int32_t& StartHeight);
template void UHDetailView::OnGenerateDetailView<UHMeshRendererComponent>(UHMeshRendererComponent* InComp, HWND ParentWnd, int32_t& StartHeight);

template void UHDetailView::SetValue(std::string PropertyName, float InVal);
template void UHDetailView::SetValue(std::string PropertyName, XMFLOAT3 InVal);
template void UHDetailView::SetValue(std::string PropertyName, bool InVal);
template void UHDetailView::SetValue(std::string PropertyName, std::string InVal);
template void UHDetailView::SetValue(std::string PropertyName, std::wstring InVal);

template float UHDetailView::GetValue(std::string PropertyName) const;
template XMFLOAT3 UHDetailView::GetValue(std::string PropertyName) const;
template bool UHDetailView::GetValue(std::string PropertyName) const;
template std::string UHDetailView::GetValue(std::string PropertyName) const;
template std::wstring UHDetailView::GetValue(std::string PropertyName) const;

#endif