#include "DetailGUIs.h"

#if WITH_EDITOR
#include "Float3DetailGUI.h"
#include "FloatDetailGUI.h"
#include "BoolDetailGUI.h"
#include "DropListDetailGUI.h"

UHDetailGUIBase::UHDetailGUIBase(std::string InName)
	: Name(InName)
{

}

std::string UHDetailGUIBase::GetName() const
{
	return Name;
}

UniquePtr<UHDetailGUIBase> GetDetailGUI(const std::string InType, const std::string InName, const UHGUIProperty InProps)
{
	UniquePtr<UHDetailGUIBase> NewGUI = nullptr;

	if (InType == "XMFLOAT3")
	{
		NewGUI = MakeUnique<UHFloat3DetailGUI>(InName, InProps);
	}
	else if (InType == "float")
	{
		NewGUI = MakeUnique<UHFloatDetailGUI>(InName, InProps);
	}
	else if (InType == "bool")
	{
		NewGUI = MakeUnique<UHBoolDetailGUI>(InName, InProps);
	}
	else if (InType == UHSTRINGA || InType == UHSTRINGW)
	{
		NewGUI = MakeUnique<UHDropListDetailGUI>(InName, InProps);
	}

	return std::move(NewGUI);
}

#endif