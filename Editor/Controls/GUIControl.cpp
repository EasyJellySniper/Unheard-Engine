#include "GUIControl.h"
#include <winuser.h>
#include "../Dialog/Dialog.h"
#include "../../../../Runtime/Classes/Utility.h"

UHGUIProperty::UHGUIProperty()
	: AutoSize(UHAutoSizeMethod::AutoSizeNone)
	, AutoMove(UHAutoMoveMethod::AutoMoveNone)
	, InitPosX(0)
	, InitPosY(0)
	, InitWidth(0)
	, InitHeight(0)
	, Instance(nullptr)
	, MarginX(0)
	, MarginY(0)
	, MinVisibleItem(15)
	, ParentWnd(nullptr)
	, bClipped(false)
{

}

UHGUIProperty& UHGUIProperty::SetAutoSize(const UHAutoSizeMethod InFlag)
{
	AutoSize = InFlag;
	return *this;
}

UHGUIProperty& UHGUIProperty::SetAutoMove(const UHAutoMoveMethod InFlag)
{
	AutoMove = InFlag;
	return *this;
}

UHGUIProperty& UHGUIProperty::SetWidth(const int32_t InWidth)
{
	InitWidth = InWidth;
	return *this;
}

UHGUIProperty& UHGUIProperty::SetHeight(const int32_t InHeight)
{
	InitHeight = InHeight;
	return *this;
}

UHGUIProperty& UHGUIProperty::SetPosX(const int32_t InPosX)
{
	InitPosX = InPosX;
	return *this;
}

UHGUIProperty& UHGUIProperty::SetPosY(const int32_t InPosY)
{
	InitPosY = InPosY;
	return *this;
}

UHGUIProperty& UHGUIProperty::SetInstance(HINSTANCE InInstance)
{
	Instance = InInstance;
	return *this;
}

UHGUIProperty& UHGUIProperty::SetParent(HWND InHwnd)
{
	ParentWnd = InHwnd;
	return *this;
}

UHGUIProperty& UHGUIProperty::SetMinVisibleItem(const int32_t InMinVisible)
{
	MinVisibleItem = InMinVisible;
	return *this;
}

UHGUIProperty& UHGUIProperty::SetMarginX(const int32_t Val)
{
	MarginX = Val;
	return *this;
}

UHGUIProperty& UHGUIProperty::SetMarginY(const int32_t Val)
{
	MarginY = Val;
	return *this;
}

UHGUIProperty& UHGUIProperty::SetClip(bool InFlag)
{
	bClipped = InFlag;
	return *this;
}

UHGUIControlBase::UHGUIControlBase()
	: ControlObj(nullptr)
	, ParentControl(nullptr)
	, InitAbsoluteRect(RECT{})
	, InitRelativeRect(RECT{})
	, InitParentRect(RECT{})
	, bIsManuallyCreated(false)
	, bIsSetFromCode(false)
{

}

UHGUIControlBase::UHGUIControlBase(HWND InControl, UHGUIProperty InProperties)
{
	InternalInit(InControl, InProperties);
	bIsManuallyCreated = false;
}

UHGUIControlBase::~UHGUIControlBase()
{
	OnClicked.clear();
	OnEditText.clear();
	OnSelected.clear();
	OnResized.clear();
	OnDoubleClicked.clear();
	GControlGUITable.erase(ControlObj);

	if (bIsManuallyCreated)
	{
		DestroyWindow(ControlObj);
	}
}

// resize function, should be called when the parent was resized
void UHGUIControlBase::Resize(int32_t NewWidth, int32_t NewHeight)
{
	if (ParentControl == nullptr)
	{
		return;
	}

	// calculate delta of width/height
	const int32_t DeltaW = NewWidth - (int32_t)(InitParentRect.right - InitParentRect.left);
	const int32_t DeltaH = NewHeight - (int32_t)(InitParentRect.bottom - InitParentRect.top);
	const int32_t ControlW = InitAbsoluteRect.right - InitAbsoluteRect.left;
	const int32_t ControlH = InitAbsoluteRect.bottom - InitAbsoluteRect.top;

	// for moving, it needs to use the original relative rect
	const int32_t ControlX = static_cast<int32_t>(InitRelativeRect.left);
	const int32_t ControlY = static_cast<int32_t>(InitRelativeRect.top);

	// process auto size, minimum size won't be smaller than its original size
	switch (ControlProperty.AutoSize)
	{
	case UHAutoSizeMethod::AutoSizeX:
		SetWindowPos(ControlObj, nullptr, 0, 0, std::max(ControlW + DeltaW, ControlW), ControlH, SWP_NOMOVE);
		RedrawWindow(ControlObj, nullptr, nullptr, RDW_INVALIDATE);
		break;
	case UHAutoSizeMethod::AutoSizeY:
		SetWindowPos(ControlObj, nullptr, 0, 0, ControlW, std::max(ControlH + DeltaH, ControlH), SWP_NOMOVE);
		RedrawWindow(ControlObj, nullptr, nullptr, RDW_INVALIDATE);
		break;
	case UHAutoSizeMethod::AutoSizeBoth:
		SetWindowPos(ControlObj, nullptr, 0, 0, std::max(ControlW + DeltaW, ControlW), std::max(ControlH + DeltaH, ControlH), SWP_NOMOVE);
		RedrawWindow(ControlObj, nullptr, nullptr, RDW_INVALIDATE);
		break;
	}

	// process auto move, minimum position won't be smaller than its original pos
	switch (ControlProperty.AutoMove)
	{
	case UHAutoMoveMethod::AutoMoveX:
		SetWindowPos(ControlObj, nullptr, std::max(ControlX + DeltaW, ControlX), ControlY, 0, 0, SWP_NOSIZE);
		RedrawWindow(ControlObj, nullptr, nullptr, RDW_INVALIDATE);
		break;
	case UHAutoMoveMethod::AutoMoveY:
		SetWindowPos(ControlObj, nullptr, ControlX, std::max(ControlY + DeltaH, ControlY), 0, 0, SWP_NOSIZE);
		RedrawWindow(ControlObj, nullptr, nullptr, RDW_INVALIDATE);
		break;
	case UHAutoMoveMethod::AutoMoveBoth:
		SetWindowPos(ControlObj, nullptr, std::max(ControlX + DeltaW, ControlX), std::max(ControlY + DeltaH, ControlY), 0, 0, SWP_NOSIZE);
		RedrawWindow(ControlObj, nullptr, nullptr, RDW_INVALIDATE);
		break;
	}
}

HWND UHGUIControlBase::GetHwnd() const
{
	return ControlObj;
}

HWND UHGUIControlBase::GetParentHwnd() const
{
	return ParentControl;
}

RECT UHGUIControlBase::GetCurrentRelativeRect() const
{
	RECT R;
	GetWindowRect(ControlObj, &R);
	MapWindowPoints(HWND_DESKTOP, ParentControl, (LPPOINT)&R, 2);
	return R;
}

bool UHGUIControlBase::IsPointInside(POINT P) const
{
	if (ScreenToClient(ControlObj, &P))
	{
		RECT R;
		GetClientRect(ControlObj, &R);
		if (R.left < P.x && R.right > P.x && R.top < P.y && R.bottom > P.y)
		{
			return true;
		}
	}

	return false;
}

UHGUIControlBase& UHGUIControlBase::SetText(std::wstring InText)
{
	SetWindowTextW(ControlObj, InText.c_str());
	return *this;
}

UHGUIControlBase& UHGUIControlBase::SetText(std::string InText)
{
	SetText(UHUtilities::ToStringW(InText));
	return *this;
}

std::wstring UHGUIControlBase::GetText() const
{
	const int32_t TextLength = GetWindowTextLengthW(ControlObj);
	wchar_t Buff[1024];
	memset(Buff, 0, 1024);
	Buff[1023] = L'\0';

	GetWindowTextW(ControlObj, Buff, TextLength + 1);
	return Buff;
}

void UHGUIControlBase::SetIsFromCode(bool bFlag)
{
	bIsSetFromCode = bFlag;
}

bool UHGUIControlBase::IsSetFromCode() const
{
	return bIsSetFromCode;
}

void UHGUIControlBase::InternalInit(HWND InControl, UHGUIProperty InProperties)
{
	ControlObj = InControl;
	ParentControl = GetParent(ControlObj);
	ControlProperty = InProperties;
	GControlGUITable[ControlObj] = this;

	// cache the rect at the beginning
	GetWindowRect(ControlObj, &InitAbsoluteRect);

	InitRelativeRect = InitAbsoluteRect;
	MapWindowPoints(HWND_DESKTOP, ParentControl, (LPPOINT)&InitRelativeRect, 2);

	GetClientRect(ParentControl, &InitParentRect);
	ShowWindow(ControlObj, SW_SHOW);
}