#pragma once
#include "../../UnheardEngine.h"

#if WITH_DEBUG
#include <windowsx.h>
#include <vector>
#include <regex>
#include <shobjidl_core.h>

// all kinds of editor utility
// @TODO: Implement high-level class for managing all kinds of controls
namespace UHEditorUtil
{
	void SetMenuItemChecked(HWND Hwnd, int32_t MenuItemID, UINT Flag);
	void SetCheckedBox(HWND Hwnd, int32_t BoxID, bool bFlag);
	void SetEditControl(HWND Hwnd, int32_t ControlID, std::wstring InValue);
	void SetEditControl(HWND Hwnd, std::wstring InValue);
	void SetEditControl(HWND Hwnd, std::string InValue);
	void SetEditControlChar(HWND Hwnd, int32_t ControlID, char InValue);
	void InitComboBox(HWND Hwnd, int32_t BoxID, std::wstring DefaultValue, const std::vector<std::wstring>& Options, int32_t MinVisible = 0);
	void InitComboBox(HWND Hwnd, std::string DefaultValue, const std::vector<std::string>& Options, int32_t MinVisible = 0);
	void SelectComboBox(HWND Hwnd, std::string Value);
	void SelectComboBox(HWND Hwnd, std::wstring Value);

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
	int32_t GetComboBoxItemCount(HWND Hwnd);

	// Add list box string
	void AddListBoxString(HWND HWnd, int32_t BoxID, std::string InValue);
	void AddListBoxString(HWND HWnd, int32_t BoxID, std::wstring InValue);
	int32_t GetListBoxSelectedIndex(HWND HWnd, int32_t BoxID);
	void SetListBoxSelectedIndex(HWND HWnd, int32_t Index);

	// window size
	void SetWindowSize(HWND Hwnd, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height);
	void GetWindowSize(HWND Hwnd, RECT& OutRect, HWND ParentWnd);

	// check point inside client
	bool IsPointInsideClient(HWND Hwnd, POINT P);

	// set static text 
	void SetStaticText(HWND Hwnd, std::string InText);

	// set slider range
	void SetSliderRange(HWND Hwnd, uint32_t MinValue, uint32_t MaxValue);
	void SetSliderPos(HWND Hwnd, uint32_t InValue);
	uint32_t GetSliderPos(HWND Hwnd);

	// file dialog
	std::wstring FileSelectInput(const COMDLG_FILTERSPEC& InFilter);
	std::wstring FileSelectOutputFolder();
}

#endif