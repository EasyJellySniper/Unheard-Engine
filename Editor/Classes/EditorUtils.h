#pragma once
#include "../../UnheardEngine.h"
#include <windowsx.h>
#include <vector>
#include <regex>
#include <shobjidl_core.h>

// all kinds of editor utility
namespace UHEditorUtil
{
	void SetMenuItemChecked(HWND Hwnd, int32_t MenuItemID, UINT Flag);

	// window size
	void GetWindowSize(HWND Hwnd, RECT& OutRect, HWND ParentWnd);

	// file dialog
	std::wstring FileSelectInput(const COMDLG_FILTERSPEC& InFilter, const std::wstring InDefaultFolder = L"");
	std::wstring FileSelectOutputFolder(const std::wstring InDefaultFolder = L"");
	std::wstring FileSelectSavePath(const COMDLG_FILTERSPEC& InFilter, const std::wstring InDefaultFolder = L"");

	// text size
	SIZE GetTextSize(HWND Hwnd, std::string InText);
	SIZE GetTextSizeW(HWND Hwnd, std::wstring InText);
}