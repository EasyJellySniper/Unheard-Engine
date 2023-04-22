#pragma once
#include "../../UnheardEngine.h"

#if WITH_DEBUG
#include <windowsx.h>
#include <vector>
#include <regex>

// all kinds of editor utility
namespace UHEditorUtil
{
	void SetMenuItemChecked(HWND Hwnd, int32_t MenuItemID, UINT Flag);
	void SetCheckedBox(HWND Hwnd, int32_t BoxID, bool bFlag);
	void SetEditControl(HWND Hwnd, int32_t ControlID, std::wstring InValue);
	void SetEditControl(HWND Hwnd, std::wstring InValue);
	void SetEditControlChar(HWND Hwnd, int32_t ControlID, char InValue);
	void InitComboBox(HWND Hwnd, int32_t BoxID, std::wstring DefaultValue, std::vector<std::wstring> Options, int32_t MinVisible = 0);
	void InitComboBox(HWND Hwnd, std::string DefaultValue, std::vector<std::string> Options, int32_t MinVisible = 0);
	void SelectComboBox(HWND Hwnd, std::string Value);

	bool GetCheckedBox(HWND Hwnd, int32_t BoxID);

	// GetEditControl, this will return number only
	template<typename T>
	T GetEditControl(HWND Hwnd, int32_t ControlID)
	{
		char Buff[1024];
		memset(Buff, 0, 1024);
		GetDlgItemTextA(Hwnd, ControlID, Buff, 1024);

		std::string Value = Buff;
		std::regex NumberRegex = std::regex("^-?\\d+$");
		if (!std::regex_match(Value, NumberRegex))
		{
			return static_cast<T>(0);
		}

		return static_cast<T>(std::stod(Value));
	}

	char GetEditControlChar(HWND Hwnd, int32_t ControlID);
	std::string GetEditControlText(HWND Hwnd);
	int32_t GetComboBoxSelectedIndex(HWND Hwnd, int32_t BoxID);
	int32_t GetComboBoxSelectedIndex(HWND Hwnd);
	std::string GetComboBoxSelectedText(HWND Hwnd);

	// Add list box string
	void AddListBoxString(HWND HWnd, int32_t BoxID, std::wstring InValue);
	int32_t GetListBoxSelectedIndex(HWND HWnd, int32_t BoxID);

	// window size
	void SetWindowSize(HWND Hwnd, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height);
	void GetWindowSize(HWND Hwnd, RECT& OutRect, HWND ParentWnd);

	// check point inside client
	bool IsPointInsideClient(HWND Hwnd, POINT P);
}

#endif