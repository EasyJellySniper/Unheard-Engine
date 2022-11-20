#pragma once
#include "../../UnheardEngine.h"

#if WITH_DEBUG
#include <windowsx.h>
#include <vector>
#include <regex>

namespace UHEditorUtil
{
	void SetMenuItemChecked(HWND Hwnd, int32_t MenuItemID, UINT Flag);
	void SetCheckedBox(HWND Hwnd, int32_t BoxID, bool bFlag);
	void SetEditControl(HWND Hwnd, int32_t ControlID, std::wstring InValue);
	void SetEditControlChar(HWND Hwnd, int32_t ControlID, char InValue);
	void InitComboBox(HWND Hwnd, int32_t BoxID, std::wstring DefaultValue, std::vector<std::wstring> Options);

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
	int32_t GetComboBoxSelectedIndex(HWND Hwnd, int32_t BoxID);
}

#endif