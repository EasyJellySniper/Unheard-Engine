#pragma once
#include "../../UnheardEngine.h"
#include <windowsx.h>
#include <vector>
#include <regex>
#include <shobjidl_core.h>

// all kinds of editor utility
// @TODO: Implement high-level class for managing all kinds of controls
namespace UHEditorUtil
{
	void SetMenuItemChecked(HWND Hwnd, int32_t MenuItemID, UINT Flag);

	// window size
	void GetWindowSize(HWND Hwnd, RECT& OutRect, HWND ParentWnd);

	// file dialog
	std::wstring FileSelectInput(const COMDLG_FILTERSPEC& InFilter);
	std::wstring FileSelectOutputFolder();
}