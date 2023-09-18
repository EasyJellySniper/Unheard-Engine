#pragma once
#include "../../UnheardEngine.h"
#include "../../resource.h"
#include <functional>

enum UHAutoSizeMethod
{
	AutoSizeNone,
	AutoSizeX,
	AutoSizeY,
	AutoSizeBoth
};

enum UHAutoMoveMethod
{
	AutoMoveNone,
	AutoMoveX,
	AutoMoveY,
	AutoMoveBoth
};


struct UHGUIProperty
{
	UHGUIProperty();

	UHGUIProperty& SetAutoSize(const UHAutoSizeMethod InFlag);
	UHGUIProperty& SetAutoMove(const UHAutoMoveMethod InFlag);
	UHGUIProperty& SetWidth(const int32_t InWidth);
	UHGUIProperty& SetHeight(const int32_t InHeight);
	UHGUIProperty& SetPosX(const int32_t InPosX);
	UHGUIProperty& SetPosY(const int32_t InPosY);
	UHGUIProperty& SetInstance(HINSTANCE InInstance);
	UHGUIProperty& SetParent(HWND InHwnd);
	UHGUIProperty& SetMinVisibleItem(const int32_t InMinVisible);
	UHGUIProperty& SetMarginX(const int32_t Val);
	UHGUIProperty& SetMarginY(const int32_t Val);
	UHGUIProperty& SetClip(bool InFlag);

	UHAutoSizeMethod AutoSize;
	UHAutoMoveMethod AutoMove;
	int32_t InitPosX;
	int32_t InitPosY;
	int32_t InitWidth;
	int32_t InitHeight;
	HINSTANCE Instance;
	HWND ParentWnd;
	int32_t MinVisibleItem;
	int32_t MarginX;
	int32_t MarginY;
	bool bClipped;
};

// base class for all GUI controls, for now it contains the native window object for the control
// and some initial information for auto resizing/moving..etc
// the control can be from resource directly or created in code
class UHGUIControlBase
{
public:
	UHGUIControlBase();
	UHGUIControlBase(HWND InControl, UHGUIProperty InProperties);
	virtual ~UHGUIControlBase();

	// layout functions
	void Resize(int32_t NewWidth, int32_t NewHeight);

	HWND GetHwnd() const;
	HWND GetParentHwnd() const;
	RECT GetCurrentRelativeRect() const;
	bool IsPointInside(POINT P) const;

	// text functions
	UHGUIControlBase& SetText(std::wstring InText);
	UHGUIControlBase& SetText(std::string InText);
	std::wstring GetText() const;

	void SetIsFromCode(bool bFlag);
	bool IsSetFromCode() const;

	// callback functions
	std::vector<std::function<void()>> OnClicked;
	std::vector<std::function<void()>> OnEditText;
	std::vector<std::function<void()>> OnSelected;
	std::vector<std::function<void()>> OnResized;
	std::vector<std::function<void()>> OnDoubleClicked;

#if WITH_EDITOR
	// if it's editing property
	std::vector<std::function<void(std::string InPropName)>> OnEditProperty;
#endif

	UHGUIProperty ControlProperty;

protected:
	void InternalInit(HWND InControl, UHGUIProperty InProperties);
	HWND ControlObj;
	HWND ParentControl;
	bool bIsManuallyCreated;
	bool bIsSetFromCode;

private:
	RECT InitAbsoluteRect;
	RECT InitRelativeRect;
	RECT InitParentRect;
};